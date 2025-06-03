package org.basaltmc.wire.common.placeholder

/**
 * What a placeholder resolved to, or why it did not.
 *
 * The two failures are kept apart because they are different mistakes and deserve different
 * messages: [Unknown] means nobody has ever registered that name, which is a typo or a plugin that
 * is not installed, while [Unavailable] means the name is real but this particular situation cannot
 * answer it, such as asking for a player's coordinates while answering a server list ping, where
 * there is no player yet.
 */
public sealed interface PlaceholderValue {
    public data class Text(
        val value: String,
    ) : PlaceholderValue

    /**
     * [reason] says what is missing when the answer depends on something unwritten, so the warning
     * can be "needs the inventory system" instead of a shrug.
     */
    public data class Unavailable(
        val reason: String? = null,
    ) : PlaceholderValue

    public data object Unknown : PlaceholderValue

    public val textOrNull: String?
        get() = (this as? Text)?.value

    public companion object {
        public fun of(value: String?): PlaceholderValue = if (value == null) Unavailable() else Text(value)

        public fun of(value: Number?): PlaceholderValue = if (value == null) Unavailable() else Text(value.toString())

        public fun of(value: Boolean?): PlaceholderValue = if (value == null) Unavailable() else Text(value.toString())
    }
}

/**
 * A typed key for one thing a context can carry, such as "the player this is about".
 *
 * Contexts are built out of these rather than out of a fixed set of fields so that the registry can
 * live here, below every module that has something to put in one: this module knows nothing about
 * players, worlds or connections, and does not need to.
 */
public class PlaceholderSlot<T : Any>(
    public val name: String,
) {
    override fun toString(): String = name
}

/** What a placeholder is being resolved against: a player, a world, a pending connection, nothing. */
public class PlaceholderContext private constructor(
    private val values: Map<PlaceholderSlot<*>, Any>,
) {
    @Suppress("UNCHECKED_CAST")
    public operator fun <T : Any> get(slot: PlaceholderSlot<T>): T? = values[slot] as T?

    public fun <T : Any> with(
        slot: PlaceholderSlot<T>,
        value: T?,
    ): PlaceholderContext = if (value == null) this else PlaceholderContext(values + (slot to value))

    public companion object {
        public val EMPTY: PlaceholderContext = PlaceholderContext(emptyMap())
    }
}

/**
 * Answers every placeholder under one identifier.
 *
 * [params] is what is left after the identifier: `%basalt_server_online%` reaches the provider
 * registered as `basalt_server` with params `online`. That split is what lets one provider serve a
 * whole family, including the ones that carry an argument (`%basalt_player_has_permission_x%`),
 * without registering a name per placeholder.
 */
public fun interface PlaceholderProvider {
    public fun resolve(
        params: String,
        context: PlaceholderContext,
    ): PlaceholderValue
}

/**
 * Every placeholder this server knows, in one place.
 *
 * One registry per server, handed to whatever needs it, rather than a global singleton: a static
 * instance would be reachable from anywhere including a regional tick, which is the pattern this
 * project keeps out of new code. "Global" here means one shared registry, not a global variable.
 *
 * Identifiers may contain underscores (`basalt_server`), so a name is matched against the longest
 * registered identifier that it starts with. That is what keeps `%basalt_server_online%` apart from
 * a plugin that registers `basalt` or `basalt_server_stats`.
 */
public class PlaceholderRegistry {
    private val providers = LinkedHashMap<String, PlaceholderProvider>()

    public val identifiers: Set<String> get() = providers.keys

    /** Registering an identifier twice replaces it, which is how a plugin reloads its own. */
    public fun register(
        identifier: String,
        provider: PlaceholderProvider,
    ) {
        providers[identifier.lowercase()] = provider
    }

    public fun unregister(identifier: String) {
        providers.remove(identifier.lowercase())
    }

    /** Which registered identifier serves [name], if any, so a caller can report or list it. */
    public fun providerFor(name: String): Pair<String, PlaceholderProvider>? {
        val lowercase = name.lowercase()
        val identifier =
            providers.keys
                .filter { lowercase == it || lowercase.startsWith(it + "_") }
                .maxByOrNull { it.length }
                ?: return null
        return identifier to providers.getValue(identifier)
    }

    /** Every name [identifier]'s provider is willing to list, or empty if it does not enumerate itself. */
    public fun namesOf(identifier: String): Collection<String> =
        (providers[identifier.lowercase()] as? PlaceholderCatalog)?.names().orEmpty()

    public fun resolve(
        name: String,
        context: PlaceholderContext = PlaceholderContext.EMPTY,
    ): PlaceholderValue {
        // Through providerFor, not a second copy of the same matching: the copy is what let a
        // broken one of them sit behind a working one.
        val (identifier, provider) = providerFor(name) ?: return PlaceholderValue.Unknown
        val params = name.lowercase().removePrefix(identifier).removePrefix("_")
        return provider.resolve(params, context)
    }
}

/**
 * A provider that can say which names it serves, so they can be listed.
 *
 * Separate from [PlaceholderProvider] rather than a second method on it, because that one is a
 * `fun interface` and a lambda is the right way to register a one-liner. A provider that cannot
 * enumerate itself (a plugin computing names at runtime) simply does not implement this.
 *
 * A name taking an argument is listed with the argument in angle brackets, e.g.
 * `has_permission_<node>`, since there is no finite list of those to print.
 */
public interface PlaceholderCatalog {
    public fun names(): Collection<String>
}
