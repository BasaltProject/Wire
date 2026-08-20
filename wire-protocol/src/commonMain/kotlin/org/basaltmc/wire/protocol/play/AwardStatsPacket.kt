package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/**
 * One counter: which kind of statistic it is, what it counts, and how far it has counted.
 *
 * parity: `Stat`, whose stream codec is the stat type's registry id followed by the id of whatever
 * that type counts, in that type's own registry: a block for `mined`, an item for `used`, and an
 * entry of `minecraft:custom_stat` for `custom`.
 */
public data class StatCounter(
    val statType: Int,
    val value: Int,
    val count: Int,
)

/**
 * Clientbound, id 0x03: the statistics screen's contents. Mirrors `ClientboundAwardStatsPacket`.
 *
 * The screen asks for them with `client_command` and shows a spinner until this arrives, so a
 * server that never answers leaves it spinning for ever.
 */
public data class AwardStatsPacket(
    val stats: List<StatCounter>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(stats.size)
        for (stat in stats) {
            buffer.writeVarInt(stat.statType)
            buffer.writeVarInt(stat.value)
            buffer.writeVarInt(stat.count)
        }
    }

    public companion object {
        public const val ID: Int = 0x03

        /** parity: `Stats`' own registration order in `minecraft:stat_type`. */
        public const val TYPE_MINED: Int = 0
        public const val TYPE_CRAFTED: Int = 1
        public const val TYPE_USED: Int = 2
        public const val TYPE_BROKEN: Int = 3
        public const val TYPE_PICKED_UP: Int = 4
        public const val TYPE_DROPPED: Int = 5
        public const val TYPE_KILLED: Int = 6
        public const val TYPE_KILLED_BY: Int = 7
        public const val TYPE_CUSTOM: Int = 8

        public fun read(buffer: ByteBuffer): AwardStatsPacket =
            AwardStatsPacket(
                List(buffer.readVarInt()) {
                    StatCounter(buffer.readVarInt(), buffer.readVarInt(), buffer.readVarInt())
                },
            )
    }
}
