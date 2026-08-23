package org.basaltmc.wire.multiversion

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.play.PalettedContainer
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/**
 * Renumbers the block states inside a chunk's section data.
 *
 * The section stream carries no count of its own, so this reads until the bytes run out rather than
 * needing to know how tall the world is: a chunk of a 384-block world and one of a superflat walk
 * the same way.
 *
 * Biomes are copied byte for byte rather than decoded. Their ids are handed out by the server in
 * the order it sends the registry, so both sides already agree on them, and copying leaves a
 * container this class has no business re-encoding exactly as it was.
 */
public class ChunkRewriter(
    private val blockStates: IdLookup,
    /** How wide a direct palette is on the older side: `ceil(log2(states))` of that version. */
    private val olderDirectBits: Int,
    /** What to show where the older version has nothing at all, normally its air. */
    private val fallbackState: Int = 0,
    /**
     * Whether the older side counts its packed longs out loud.
     *
     * Up to 1.21.4 a palette wrote the length of its data array and then the array; from 1.21.5 the
     * length is left out, because both sides can work it out from the width and the container size.
     * A client that expects the number and does not get it reads the next field as a length.
     */
    private val lengthPrefixedData: Boolean = false,
    /**
     * Whether the far end counts the fluids in a section.
     *
     * 26.1 added a second short beside the block count. Everything below reads one short and then
     * the palette, so leaving it in shifts every byte of every chunk they are sent.
     */
    private val fluidCount: Boolean = true,
    /**
     * Whether the sections coming in still carry it.
     *
     * A link below the one that took it out reads what that link wrote, not what this server does.
     */
    private val readsFluidCount: Boolean = true,
) {
    private val blockScratch = IntArray(BLOCKS_PER_SECTION)

    public fun rewrite(sections: ByteArray): ByteArray {
        val input = ByteBuffer().apply { writeBytes(sections) }
        val output = ByteBuffer()
        while (input.readableBytes > 0) {
            output.writeShort(input.readShort())
            val fluids = if (readsFluidCount) input.readShort() else 0
            if (fluidCount) output.writeShort(fluids)
            val blocks = PalettedContainer.read(input, BLOCKS_PER_SECTION, PalettedContainer.BLOCK_MAX_INDIRECT_BITS)
            blocks.copyInto(blockScratch)
            for (i in blockScratch.indices) {
                val mapped = blockStates.map(blockScratch[i])
                blockScratch[i] = if (mapped == IdMap.NONE) fallbackState else mapped
            }
            writeContainer(
                output,
                blockScratch,
                PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
                PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
                olderDirectBits,
            )
            copyContainer(input, output, BIOMES_PER_SECTION, PalettedContainer.BIOME_MAX_INDIRECT_BITS)
        }
        return output.toByteArray()
    }

    /**
     * Just the biome containers of one column, as `chunks_biomes` carries them.
     *
     * Nothing is renumbered, but the container itself may need writing the older way, which is the
     * whole reason this exists rather than the bytes being passed through.
     */
    public fun rewriteBiomes(biomes: ByteArray): ByteArray {
        val input = ByteBuffer().apply { writeBytes(biomes) }
        val output = ByteBuffer()
        while (input.readableBytes > 0) {
            copyContainer(input, output, BIOMES_PER_SECTION, PalettedContainer.BIOME_MAX_INDIRECT_BITS)
        }
        return output.toByteArray()
    }

    /**
     * The values, as the older side writes a container.
     *
     * [PalettedContainer.write] is the modern form; the older one is the same bytes with the length
     * of the long array in front of it, so this writes into a scratch buffer and then copies it out
     * with the count it just worked out.
     */
    private fun writeContainer(
        output: ByteBuffer,
        values: IntArray,
        minIndirectBits: Int,
        maxIndirectBits: Int,
        directBits: Int,
    ) {
        if (!lengthPrefixedData) {
            PalettedContainer.write(output, values, minIndirectBits, maxIndirectBits, directBits)
            return
        }
        val modern = ByteBuffer()
        PalettedContainer.write(modern, values, minIndirectBits, maxIndirectBits, directBits)
        val bits = modern.readByte().toInt() and 0xFF
        output.writeByte(bits.toByte())
        if (bits == 0) {
            output.writeVarInt(modern.readVarInt())
            // Even a container of one value counts its longs out loud in the older form, and the
            // count is zero. Leaving it out is a byte short and the client reads the next field
            // as a length.
            output.writeVarInt(0)
            return
        }
        if (bits <= maxIndirectBits) {
            val entries = modern.readVarInt()
            output.writeVarInt(entries)
            repeat(entries) { output.writeVarInt(modern.readVarInt()) }
        }
        val longs = modern.readableBytes / Long.SIZE_BYTES
        output.writeVarInt(longs)
        repeat(longs) { output.writeLong(modern.readLong()) }
    }

    /** Copies a container across, adding the length the older side expects when it needs one. */
    public fun copyContainer(
        input: ByteBuffer,
        output: ByteBuffer,
        size: Int,
        maxIndirectBits: Int,
    ) {
        val bits = input.readByte().toInt() and 0xFF
        output.writeByte(bits.toByte())
        if (bits == 0) {
            output.writeVarInt(input.readVarInt())
            if (lengthPrefixedData) output.writeVarInt(0)
            return
        }
        if (bits <= maxIndirectBits) {
            val entries = input.readVarInt()
            output.writeVarInt(entries)
            repeat(entries) { output.writeVarInt(input.readVarInt()) }
        }
        val valuesPerLong = 64 / bits
        val longs = (size + valuesPerLong - 1) / valuesPerLong
        if (lengthPrefixedData) output.writeVarInt(longs)
        repeat(longs) { output.writeLong(input.readLong()) }
    }

    private companion object {
        const val BLOCKS_PER_SECTION = 16 * 16 * 16
        const val BIOMES_PER_SECTION = 4 * 4 * 4
    }
}
