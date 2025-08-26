package org.basaltmc.wire.multiversion

/**
 * Several links' numbering, applied one after another as if it were a single table.
 *
 * A chain of five links moves an id five times, and every step is a lookup in that step's own
 * table. Composing them is what lets the renumbering happen **once**, on the packet as this server
 * wrote it, instead of once per link: a link halfway down would otherwise be reading a packet the
 * link above it had already rewritten, which is how a chunk ends up parsed as something it is not.
 */
public class ComposedIdMap(
    private val steps: List<IdMap>,
) : IdLookup {
    override fun map(id: Int): Int {
        var current = id
        for (step in steps) {
            current = step.map(current)
            if (current == IdMap.NONE) return IdMap.NONE
        }
        return current
    }

    /** True as soon as one step had to stand something else in, which the whole walk inherits. */
    override fun substitutes(id: Int): Boolean {
        var current = id
        for (step in steps) {
            if (step.substitutes(current)) return true
            current = step.map(current)
            if (current == IdMap.NONE) return false
        }
        return false
    }

    /** The same walk backwards, for an id arriving from the client. */
    override fun unmap(id: Int): Int {
        var current = id
        for (i in steps.indices.reversed()) {
            current = steps[i].unmap(current)
            if (current == IdMap.NONE) return IdMap.NONE
        }
        return current
    }
}

/** Every registry a packet can carry an id of, composed across a whole chain. */
public class ComposedTables(
    tables: List<VersionTables>,
) {
    public val blockStates: ComposedIdMap = ComposedIdMap(tables.map { it.blockStates })
    public val block: ComposedIdMap = ComposedIdMap(tables.map { it.block })
    public val item: ComposedIdMap = ComposedIdMap(tables.map { it.item })
    public val particleType: ComposedIdMap = ComposedIdMap(tables.map { it.particleType })
    public val entityType: ComposedIdMap = ComposedIdMap(tables.map { it.entityType })
    public val attribute: ComposedIdMap = ComposedIdMap(tables.map { it.attribute })
    public val dataComponentType: ComposedIdMap = ComposedIdMap(tables.map { it.dataComponentType })
    public val menu: ComposedIdMap = ComposedIdMap(tables.map { it.menu })
    public val mobEffect: ComposedIdMap = ComposedIdMap(tables.map { it.mobEffect })
    public val soundEvent: ComposedIdMap = ComposedIdMap(tables.map { it.soundEvent })
    public val gameEvent: ComposedIdMap = ComposedIdMap(tables.map { it.gameEvent })
    public val slotDisplay: ComposedIdMap = ComposedIdMap(tables.map { it.slotDisplay })

    /**
     * The far end's own names for what it reads by name rather than by number.
     *
     * The far end's and not each link's: the renumbering happens once, so by the time a link sees
     * an id it is already the number the client will read, and only that version's names fit it.
     */
    public val attributeNames: NameIdMap = tables.last().attributeNames

    public val staticBiomes: NameIdMap = tables.last().staticBiomes

    /**
     * Registries the far end does not have at all, by name.
     *
     * A command whose argument names one of them is a command the client cannot read: it looks the
     * registry up while parsing the tree and gives up on the whole packet when it is not there,
     * which costs the connection.
     */
    public val missingRegistries: Set<String> =
        tables
            .flatMap { it.newEntries.toList() }
            .filter { it.endsWith(WHOLE_REGISTRY) }
            .map { it.removeSuffix(WHOLE_REGISTRY) }
            .toSet()

    /**
     * Whether an entity type is a player, which is the only one a link has to recognise.
     *
     * Below 1.13 a player has no number of its own: it is not in the registry there because it is
     * never spawned as an entity, it has a packet of its own. So the number that arrives is this
     * build's, passed through untouched, and that is what is compared.
     */
    public fun isPlayer(entityType: Int): Boolean =
        if (playerEntityType >= 0) entityType == playerEntityType else entityType == nativePlayerEntityType

    private val playerEntityType: Int = tables.last().playerEntityType

    /** What **this** build numbers a player, which is the top of the chain whatever it translates to. */
    public val nativePlayerEntityType: Int = tables.first().newerPlayerEntityType

    /** How wide a direct block palette is at the far end of the chain. */
    public val blockStateDirectBits: Int = tables.last().blockStateDirectBits

    /**
     * Whether the far end reads the fluid count 26.1 added to every chunk section.
     *
     * Anything below it reads one short where this server writes two.
     */
    public val fluidCountInSections: Boolean = tables.last().older >= FLUID_COUNT_SINCE

    private companion object {
        const val WHOLE_REGISTRY = "/*"
        const val FLUID_COUNT_SINCE = 775
    }
}
