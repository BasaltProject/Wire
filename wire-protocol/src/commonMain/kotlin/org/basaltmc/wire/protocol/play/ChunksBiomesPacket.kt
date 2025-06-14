package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/** One column's biome data: its position, and every section's biome container back to back. */
public data class ChunkBiomeData(
    val chunkX: Int,
    val chunkZ: Int,
    val biomes: ByteArray,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(packChunkPos(chunkX, chunkZ))
        buffer.writeVarInt(biomes.size)
        buffer.writeBytes(biomes)
    }

    // Generated because the class carries an array: identity equality would make a round-trip test
    // pass on the reference rather than on the bytes.
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is ChunkBiomeData) return false
        return chunkX == other.chunkX && chunkZ == other.chunkZ && biomes.contentEquals(other.biomes)
    }

    override fun hashCode(): Int = (chunkX * 31 + chunkZ) * 31 + biomes.contentHashCode()

    public companion object {
        public fun read(buffer: ByteBuffer): ChunkBiomeData {
            val packed = buffer.readLong()
            return ChunkBiomeData(
                chunkX = packed.toInt(),
                chunkZ = (packed ushr Int.SIZE_BITS).toInt(),
                biomes = buffer.readBytes(buffer.readVarInt()),
            )
        }

        /** parity: `ChunkPos.asLong`, x in the low half and z in the high one. */
        public fun packChunkPos(
            x: Int,
            z: Int,
        ): Long = (x.toLong() and MASK) or ((z.toLong() and MASK) shl Int.SIZE_BITS)

        private const val MASK = 0xFFFFFFFFL
    }
}

/**
 * Clientbound, id 0x0d: new biomes for columns the client already has. Mirrors
 * `ClientboundChunksBiomesPacket`.
 *
 * The point of it is that it is not a chunk resend: the blocks, block entities and light the client
 * holds stay where they are, and only the biome containers are replaced, which is what makes a
 * biome change visible without the column flickering out and back.
 */
public data class ChunksBiomesPacket(
    val chunks: List<ChunkBiomeData>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(chunks.size)
        for (chunk in chunks) chunk.write(buffer)
    }

    public companion object {
        public const val ID: Int = 0x0d

        public fun read(buffer: ByteBuffer): ChunksBiomesPacket =
            ChunksBiomesPacket(List(buffer.readVarInt()) { ChunkBiomeData.read(buffer) })
    }
}
