package org.basaltmc.wire.multiversion

/**
 * Everything one step down the chain is: which packet is called what, and how each registry is
 * numbered.
 *
 * All of it is generated from the two versions' own reports by `tools/protocol-diff`. A link that
 * needs nothing else is a table and no code, which is what most modern links are.
 */
public class VersionTables(
    public val older: Int,
    public val packets: PacketIdTables,
    public val blockStates: IdMap,
    public val block: IdMap = IdMap.IDENTITY,
    /**
     * Below 1.13 the high half of the value is the damage: a variant of an item was a number there
     * and a name of its own here, and the two travel together so that they map as one thing.
     */
    public val item: IdMap = IdMap.IDENTITY,
    public val particleType: IdMap = IdMap.IDENTITY,
    public val entityType: IdMap = IdMap.IDENTITY,
    public val attribute: IdMap = IdMap.IDENTITY,
    public val dataComponentType: IdMap = IdMap.IDENTITY,
    public val menu: IdMap = IdMap.IDENTITY,
    public val mobEffect: IdMap = IdMap.IDENTITY,
    public val soundEvent: IdMap = IdMap.IDENTITY,
    public val gameEvent: IdMap = IdMap.IDENTITY,
    /** How a recipe draws one slot. Its order changed in 26.1, and a wrong one is read as another. */
    public val slotDisplay: IdMap = IdMap.IDENTITY,
    /**
     * Registry entries the version above added, each as its registry and its name.
     *
     * A whole registry the older version does not have is named with a star in place of the entry.
     * They are taken out of the registries this server sends.
     */
    public val newEntries: NameSet = NameSet(""),
    /** Attribute keys the version above invented, taken out of every element that carries them. */
    public val newAttributes: NameSet = NameSet(""),
    /**
     * The older version's own dimension types, as text.
     *
     * Only the step where the attributes appeared carries them: what a dimension is was rearranged
     * there, and the fields the older one needs are not all in what arrives, so it is sent its own.
     */
    public val dimensionTypes: Map<String, String> = emptyMap(),
    /** What the older version numbers `minecraft:player`, or -1 where it is not in its registry. */
    public val playerEntityType: Int = -1,
    /** The same for the version above it, which for the first link of a chain is this build's own. */
    public val newerPlayerEntityType: Int = -1,
    /** What the older version calls its own biomes, for the versions that have them built in. */
    public val staticBiomes: NameIdMap = NameIdMap.EMPTY,
    /** What the older version calls its own attributes, for the versions that read them by name. */
    public val attributeNames: NameIdMap = NameIdMap.EMPTY,
    /** `ceil(log2(states))` on the older side, which is how wide its direct palettes are. */
    public val blockStateDirectBits: Int = 15,
)
