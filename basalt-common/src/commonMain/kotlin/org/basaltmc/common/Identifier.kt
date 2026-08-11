package org.basaltmc.common

/** Thrown when a string does not parse as a valid `namespace:path` identifier. */
public class IdentifierFormatException(
    message: String,
) : Exception(message)

/** A namespaced key (`minecraft:stone`, `basalt:some_plugin_block`), the vanilla resource ID format. */
public data class Identifier(
    public val namespace: String,
    public val path: String,
) {
    override fun toString(): String = "$namespace:$path"

    public companion object {
        public const val VANILLA_NAMESPACE: String = "minecraft"

        public fun vanilla(path: String): Identifier = Identifier(VANILLA_NAMESPACE, path)

        /** Parses `namespace:path`, or bare `path` (defaulting to [VANILLA_NAMESPACE]). */
        public fun parse(value: String): Identifier {
            val separator = value.indexOf(':')
            if (separator < 0) return Identifier(VANILLA_NAMESPACE, value)
            if (separator != value.lastIndexOf(':')) throw IdentifierFormatException("Too many ':' in '$value'")
            val namespace = value.substring(0, separator)
            val path = value.substring(separator + 1)
            if (namespace.isEmpty() || path.isEmpty()) {
                throw IdentifierFormatException("Empty namespace or path in '$value'")
            }
            return Identifier(namespace, path)
        }
    }
}
