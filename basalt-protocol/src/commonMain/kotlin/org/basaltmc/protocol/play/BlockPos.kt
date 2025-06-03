package org.basaltmc.protocol.play

/** A block position, packed into an `i64` the way vanilla's `BlockPos.asLong` does: 26 bits x, 26 bits z, 12 bits y. */
public data class BlockPos(
    val x: Int,
    val y: Int,
    val z: Int,
) {
    public fun pack(): Long =
        ((x.toLong() and 0x3FFFFFFL) shl 38) or
            ((z.toLong() and 0x3FFFFFFL) shl 12) or
            (y.toLong() and 0xFFFL)

    /** The neighbouring position across [direction]'s face. Mirrors `BlockPos.relative`. */
    public fun offset(direction: Direction): BlockPos =
        BlockPos(x + direction.normalX, y + direction.normalY, z + direction.normalZ)

    public companion object {
        public fun unpack(packed: Long): BlockPos {
            val x = (packed shl 0) shr 38
            val y = (packed shl 52) shr 52
            val z = (packed shl 26) shr 38
            return BlockPos(x.toInt(), y.toInt(), z.toInt())
        }
    }
}
