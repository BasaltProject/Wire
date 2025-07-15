package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.BitStorage
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.18 to 1.17.1, where a chunk is a different packet.
 *
 * Before 1.18 the biomes are one flat list for the whole column, only the sections with something
 * in them are sent, and a bitmask at the front says which.
 *
 * The block entities are left out: 1.17.1 wants each as a tag naming itself and what travels here
 * is a number, so a chest is still a chest but its name does not arrive.
 */
internal class Link756(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        if (packet.nativeId == LoginPlayPacket.ID) {
            dropSimulationDistance(packet)
            return true
        }
        if (packet.nativeId != MapChunkPacket.ID) return true
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val heightmaps = input.readAnonymousNbt()
        val sections = ByteBuffer().apply { writeBytes(input.readBytes(input.readVarInt())) }

        val mask = mutableListOf<Long>()
        val biomes = mutableListOf<Int>()
        val kept = ByteBuffer()
        var index = 0
        while (sections.readableBytes > 0) {
            val nonAir = sections.readShort()
            val blocks = ByteBuffer()
            copyContainer(sections, blocks, BLOCKS_PER_SECTION, BLOCK_MAX_INDIRECT_BITS)
            biomes += readContainer(sections, BIOMES_PER_SECTION, BIOME_MAX_INDIRECT_BITS).toList()
            if (nonAir.toInt() != 0) {
                setBit(mask, index)
                kept.writeShort(nonAir)
                kept.writeBytes(blocks.readBytes(blocks.readableBytes))
            }
            index++
        }

        val data = kept.toByteArray()
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeVarInt(mask.size)
            for (word in mask) output.writeLong(word)
            output.writeAnonymousNbt(heightmaps)
            output.writeVarInt(biomes.size)
            for (biome in biomes) output.writeVarInt(biome)
            output.writeVarInt(data.size)
            output.writeBytes(data)
            output.writeVarInt(0)
        }
        return true
    }

    /** The simulation distance is 1.18's, and a client of 1.17.1 reads what follows it instead. */
    private fun dropSimulationDistance(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readInt()
        val hardcore = input.readByte()
        val gamemode = input.readByte()
        val previousGamemode = input.readByte()
        val worldNames = List(input.readVarInt()) { input.readProtocolString() }
        val codec = input.readAnonymousNbt()
        val dimension = input.readAnonymousNbt()
        val worldName = input.readProtocolString()
        val hashedSeed = input.readLong()
        val maxPlayers = input.readVarInt()
        val viewDistance = input.readVarInt()
        input.readVarInt()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeInt(entityId)
            output.writeByte(hardcore)
            output.writeByte(gamemode)
            output.writeByte(previousGamemode)
            output.writeVarInt(worldNames.size)
            for (name in worldNames) output.writeProtocolString(name)
            output.writeAnonymousNbt(codec)
            output.writeAnonymousNbt(dimension)
            output.writeProtocolString(worldName)
            output.writeLong(hashedSeed)
            output.writeVarInt(maxPlayers)
            output.writeVarInt(viewDistance)
            output.writeBytes(rest)
        }
    }

    private fun setBit(
        mask: MutableList<Long>,
        index: Int,
    ) {
        val word = index / Long.SIZE_BITS
        while (mask.size <= word) mask.add(0L)
        mask[word] = mask[word] or (1L shl (index % Long.SIZE_BITS))
    }

    /**
     * Copies one container across without decoding it, length prefix and all.
     *
     * The exception is a container of a single value, which is 1.18's and which a client below it
     * reads as a four-bit palette of one entry: written as one, since that is the same section.
     */
    private fun copyContainer(
        input: ByteBuffer,
        output: ByteBuffer,
        size: Int,
        maxIndirectBits: Int,
    ) {
        val bits = input.readByte().toInt() and 0xFF
        if (bits == 0) {
            val value = input.readVarInt()
            input.readVarInt()
            output.writeByte(SMALLEST_PALETTE.toByte())
            output.writeVarInt(1)
            output.writeVarInt(value)
            val longs = size * SMALLEST_PALETTE / Long.SIZE_BITS
            output.writeVarInt(longs)
            repeat(longs) { output.writeLong(0L) }
            return
        }
        output.writeByte(bits.toByte())
        if (bits <= maxIndirectBits) {
            val entries = input.readVarInt()
            output.writeVarInt(entries)
            repeat(entries) { output.writeVarInt(input.readVarInt()) }
        }
        val longs = input.readVarInt()
        output.writeVarInt(longs)
        repeat(longs) { output.writeLong(input.readLong()) }
    }

    /** The values of one container, which is what the biomes have to become. */
    private fun readContainer(
        input: ByteBuffer,
        size: Int,
        maxIndirectBits: Int,
    ): IntArray {
        val bits = input.readByte().toInt() and 0xFF
        if (bits == 0) {
            val value = input.readVarInt()
            input.readVarInt() // The length, which is zero for a container of one value.
            return IntArray(size) { value }
        }
        val palette = if (bits <= maxIndirectBits) IntArray(input.readVarInt()) { input.readVarInt() } else null
        val longs = input.readVarInt()
        val words = LongArray(longs) { input.readLong() }
        val storage = BitStorage(bits, size, words)
        return IntArray(size) { i -> palette?.get(storage[i]) ?: storage[i] }
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        // What a client says about itself grew: 1.18 added whether it wants to be listed.
        val settings =
            (state == ProtocolState.CONFIGURATION && packet.nativeId == CLIENT_INFORMATION_CONFIGURATION) ||
                (state == ProtocolState.PLAY && packet.nativeId == CLIENT_INFORMATION_PLAY)
        if (!settings) return true
        val body = packet.body
        encode(packet) { output ->
            output.writeBytes(body)
            output.writeByte(1)
        }
        return true
    }

    private companion object {
        /** parity: `ServerboundClientInformationPacket`, which sits in both phases. */
        const val CLIENT_INFORMATION_CONFIGURATION = 0x00
        const val CLIENT_INFORMATION_PLAY = 14

        const val BLOCKS_PER_SECTION = 16 * 16 * 16
        const val BIOMES_PER_SECTION = 4 * 4 * 4
        const val BLOCK_MAX_INDIRECT_BITS = 8
        const val BIOME_MAX_INDIRECT_BITS = 3

        /** parity: `PalettedContainer` never goes below four bits for blocks. */
        const val SMALLEST_PALETTE = 4
    }
}
