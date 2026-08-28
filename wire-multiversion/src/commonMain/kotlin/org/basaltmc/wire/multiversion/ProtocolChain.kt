package org.basaltmc.wire.multiversion

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket

/**
 * A packet on its way through the chain: an id and a body, both of which a link may replace.
 *
 * [id] is what the version above the current link calls it, a number no link has constants for.
 * [nativeId] is what this server calls it, which is what a link asks to know what it is looking at.
 */
public class MutablePacket {
    public var id: Int = 0
    public var body: ByteArray = EMPTY

    /**
     * What this server calls this packet, whatever the versions in between call it.
     *
     * A link that turns one packet into another sets it, so that the links below it recognise what
     * they are looking at: a chunk that was folded into the light packet is a chunk from there on.
     */
    public var nativeId: Int = 0
        internal set

    /** Says the packet is now the one [nativeId] names, for a link that made it into that. */
    internal fun becomes(id: Int) {
        nativeId = id
    }

    /** Whether a link named the packet itself, which the renumbering then leaves alone. */
    public var idOverridden: Boolean = false
        private set

    /**
     * Says what this link's older side calls the packet, when the link knows better than its table.
     *
     * Needed where a packet was renamed or replaced by another: a table matches by name, so it sees
     * the old name as gone and would drop it. What is named here is the id **at that link**, and
     * the links below carry on renumbering it from there.
     */
    public fun overrideId(id: Int) {
        this.id = id
        idOverridden = true
    }

    internal fun takeOverride(): Boolean {
        if (!idOverridden) return false
        idOverridden = false
        return true
    }

    /**
     * Packets this one turned into on top of itself, sent after it. Empty in almost every link.
     *
     * The id is the one **that link's** older side uses, the same as [overrideId]: the links below
     * go on renumbering it from there.
     */
    public val extras: MutableList<Pair<Int, ByteArray>> = mutableListOf()

    internal fun reset(
        id: Int,
        nativeId: Int,
        body: ByteArray,
    ) {
        this.id = id
        this.nativeId = nativeId
        this.body = body
        idOverridden = false
        if (extras.isNotEmpty()) extras.clear()
    }

    public companion object {
        public val EMPTY: ByteArray = ByteArray(0)
    }
}

/**
 * Whatever renumbers what a packet carries, for a whole chain at once.
 *
 * Its own interface rather than a link so that the chain can run it exactly once, on the packet as
 * this server wrote it: a link halfway down is looking at a packet the links above it have already
 * rewritten, and cannot read its fields.
 */
public interface PacketContentRewriter {
    public fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean

    public fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean
}

/**
 * One step between two neighbouring protocols.
 *
 * A link knows only about its two versions, which is what keeps the whole thing tractable: the
 * chain composes them, so 1.21 is reached by running the same small steps one after another rather
 * than by anybody writing "1.21 to 26.2".
 *
 * The chain does the renumbering itself, out of [packets]; a link only writes the bodies that
 * changed shape. Returning false drops the packet, which is how a link handles something the older
 * version has no word for.
 */
public interface ProtocolLink {
    /** The older side. The newer side is the [older] of the next link up, or the native version. */
    public val older: Int

    /** What the older side calls each of the newer side's packets. */
    public val packets: PacketIdTables

    /**
     * What the older side cannot be told, in the registries this server sends it.
     *
     * Applied by the chain rather than by the link, because it is the same work every step and
     * because every step has to do it: a client four versions back has to be spared what each of
     * the four added.
     */
    public val registryRemovals: RegistryRemovals get() = RegistryRemovals.NONE

    public fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean

    public fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean
}

/**
 * The entries and attribute keys one version added, which the version below it cannot read.
 *
 * An element naming something that does not exist over there is not a smaller world: the client
 * refuses to load its registries at all and never reaches the game, which is what a real client
 * does when it is sent a 26.2 biome and a 26.2 song.
 */
public class RegistryRemovals(
    public val entries: NameSet = NameSet(""),
    public val attributes: NameSet = NameSet(""),
) {
    public val isEmpty: Boolean get() = entries.isEmpty && attributes.isEmpty

    public companion object {
        public val NONE: RegistryRemovals = RegistryRemovals()
    }
}

/**
 * The links between what the server speaks and what one client speaks, in order.
 *
 * Clientbound runs newest to oldest and serverbound runs the other way, which is the whole idea:
 * every link is written once, against the version right below it, and a client five versions back
 * is five hops rather than a translator of its own.
 */
public class ProtocolChain(
    private val links: List<ProtocolLink>,
    /**
     * What puts every id in the far end's numbering, run once for the whole chain.
     *
     * Null leaves the bodies alone, which is what a test of the id mapping alone wants.
     */
    private val content: PacketContentRewriter? = null,
) {
    public val isEmpty: Boolean get() = links.isEmpty()

    /** Runs a packet on its way to the client and hands the result to [emit]. */
    public fun clientbound(
        state: ProtocolState,
        id: Int,
        body: ByteArray,
        emit: (Int, ByteArray) -> Unit,
    ) {
        // One packet and one list per call, not one per chain: a session is written to by whatever
        // thread has something to say to it (its own reader, the world's owner, another player's),
        // and a translation that kept its state on the chain was being torn in half by the second
        // writer. A translated client pays one small allocation per packet for that.
        val packet = MutablePacket()
        val carried = mutableListOf<Pair<Int, ByteArray>>()
        packet.reset(id, id, body)
        if (content?.clientbound(state, packet) == false) return
        // The rewriter's own extras are in this server's numbering, so they are carried before the
        // first link rather than after it: every link renumbers them, the same as the packet itself.
        if (packet.extras.isNotEmpty()) {
            carried += packet.extras
            packet.extras.clear()
        }
        var current = id
        var dropped = false
        for (link in links) {
            if (!dropped && !stripRegistry(state, packet, link)) dropped = true
            if (!dropped && !link.clientbound(state, packet)) dropped = true
            renumberCarried(carried, state, link)
            if (packet.extras.isNotEmpty()) {
                carried += packet.extras
                packet.extras.clear()
            }
            if (dropped) continue
            // A link that named the packet itself replaces its own table's answer, and the links
            // below go on renumbering from the name it gave.
            if (packet.takeOverride()) {
                current = packet.id
                continue
            }
            val mapped = link.packets.toOlder(state, current)
            if (mapped == PacketIdMap.DROPPED) {
                dropped = true
            } else {
                current = mapped
                // Kept up to date so that a link below can ask what this packet is called where it
                // sits, which is what one making a packet of its own needs to know.
                packet.id = current
            }
        }
        if (!dropped) emit(current, packet.body)
        for ((extraId, extraBody) in carried) emit(extraId, extraBody)
    }

    /**
     * Takes out of a registry packet what this link's older side cannot read.
     *
     * False when nothing is left worth sending, which is a registry that version does not have at
     * all: an empty one is not the same as none, and the client is better off never hearing of it.
     */
    private fun stripRegistry(
        state: ProtocolState,
        packet: MutablePacket,
        link: ProtocolLink,
    ): Boolean {
        if (state != ProtocolState.CONFIGURATION || packet.nativeId != RegistryDataPacket.ID) return true
        val removals = link.registryRemovals
        if (removals.isEmpty) return true
        val data = RegistryDataPacket.read(ByteBuffer().apply { writeBytes(packet.body) })
        if ("${data.registryId}/*" in removals.entries) return false
        val kept =
            data.entries.mapNotNull { entry ->
                if ("${data.registryId}/${entry.key}" in removals.entries) return@mapNotNull null
                entry.copy(value = withoutAttributes(entry.value, removals))
            }
        if (kept.size == data.entries.size && removals.attributes.isEmpty) return true
        val output = ByteBuffer()
        RegistryDataPacket(data.registryId, kept).write(output)
        packet.body = output.toByteArray()
        return true
    }

    /** The same element without the attribute keys that version has never heard of. */
    private fun withoutAttributes(
        value: NbtTag?,
        removals: RegistryRemovals,
    ): NbtTag? {
        if (removals.attributes.isEmpty) return value
        val element = value as? NbtCompound ?: return value
        val attributes = element["attributes"] as? NbtCompound ?: return value
        val unknown = attributes.keys.filter { it in removals.attributes }
        if (unknown.isEmpty()) return value
        val rewritten = NbtCompound()
        for ((name, tag) in element) rewritten[name] = tag
        val kept = NbtCompound()
        for ((name, tag) in attributes) if (name !in unknown) kept[name] = tag
        rewritten["attributes"] = kept
        return rewritten
    }

    /** The extras of a link above are renumbered by every link below it, like any other packet. */
    private fun renumberCarried(
        carried: MutableList<Pair<Int, ByteArray>>,
        state: ProtocolState,
        link: ProtocolLink,
    ) {
        if (carried.isEmpty()) return
        var kept = 0
        for (index in carried.indices) {
            val (extraId, extraBody) = carried[index]
            val mapped = link.packets.toOlder(state, extraId)
            if (mapped == PacketIdMap.DROPPED) continue
            carried[kept++] = mapped to extraBody
        }
        while (carried.size > kept) carried.removeAt(carried.size - 1)
    }

    /**
     * The same on the way in, oldest link first.
     *
     * Renumbering runs to the end before any body is touched, so that every link is looking at the
     * packet this server would have read, not at the number the version below it happened to use.
     */
    public fun serverbound(
        state: ProtocolState,
        id: Int,
        body: ByteArray,
        emit: (Int, ByteArray) -> Unit,
    ) {
        var current = id
        for (i in links.indices.reversed()) {
            val mapped = links[i].packets.toNative(state, current)
            if (mapped == PacketIdMap.DROPPED) return
            current = mapped
        }
        val packet = MutablePacket()
        packet.reset(current, current, body)
        for (i in links.indices.reversed()) {
            if (!links[i].serverbound(state, packet)) return
        }
        if (content?.serverbound(state, packet) == false) return
        emit(packet.id, packet.body)
        for ((extraId, extraBody) in packet.extras) emit(extraId, extraBody)
    }
}
