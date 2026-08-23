package org.basaltmc.wire.multiversion

import org.basaltmc.wire.protocol.ProtocolState

/**
 * What one version calls each of another's packets, in one direction of one phase.
 *
 * Dense rather than clever: a phase has a couple of hundred packets and the table is indexed by the
 * native id, so translating a packet id is an array read. [DROPPED] is a packet the older version
 * has no word for, which is thrown away rather than sent as something else.
 */
public class PacketIdMap(
    private val encoded: String,
) {
    private var forward: IntArray = EMPTY
    private var backward: IntArray = EMPTY
    private var parsed = false

    public fun map(nativeId: Int): Int {
        if (!parsed) parse()
        return if (nativeId in forward.indices) forward[nativeId] else DROPPED
    }

    public fun unmap(olderId: Int): Int {
        if (!parsed) parse()
        return if (olderId in backward.indices) backward[olderId] else DROPPED
    }

    private fun parse() {
        val parts = if (encoded.isEmpty()) emptyList() else encoded.split(',')
        forward = IntArray(parts.size) { parts[it].toInt() }
        val highest = forward.maxOrNull() ?: -1
        backward = IntArray(highest + 1) { DROPPED }
        for (native in forward.indices) {
            val older = forward[native]
            if (older >= 0) backward[older] = native
        }
        parsed = true
    }

    public companion object {
        public const val DROPPED: Int = -1

        private val EMPTY = IntArray(0)

        /** Nothing moved, which is the common case and costs nothing to say. */
        public val IDENTITY: PacketIdMap = PacketIdMap("")
    }
}

/** The packet ids of one version, by phase and direction. An empty map means nothing moved. */
public class PacketIdTables(
    private val clientbound: Map<ProtocolState, PacketIdMap> = emptyMap(),
    private val serverbound: Map<ProtocolState, PacketIdMap> = emptyMap(),
) {
    /** [PacketIdMap.DROPPED] when the older version has no such packet. */
    public fun toOlder(
        state: ProtocolState,
        id: Int,
    ): Int = clientbound[state]?.map(id) ?: id

    public fun toNative(
        state: ProtocolState,
        id: Int,
    ): Int = serverbound[state]?.unmap(id) ?: id

    public fun hasClientbound(state: ProtocolState): Boolean = clientbound.containsKey(state)

    public fun hasServerbound(state: ProtocolState): Boolean = serverbound.containsKey(state)
}
