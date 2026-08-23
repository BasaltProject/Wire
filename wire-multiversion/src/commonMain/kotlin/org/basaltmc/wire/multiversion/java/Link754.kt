package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtLongArray
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.BitStorage
import org.basaltmc.wire.protocol.play.ClearTitlesPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.SectionBlocksUpdatePacket
import org.basaltmc.wire.protocol.play.SetActionBarTextPacket
import org.basaltmc.wire.protocol.play.SetSubtitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitlesAnimationPacket
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.17 to 1.16.4 and 1.16.5, where the world stops fitting.
 *
 * 1.16 has sixteen sections from 0 to 255 and no way to say otherwise, so the chunk, the light and
 * every block update are cut to that band and the masks become a single number.
 *
 * The five title packets 1.17 split out come back together as the one packet 1.16 has.
 */
internal class Link754(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            MapChunkPacket.ID -> return cutChunk(packet)
            UpdateLightPacket.ID -> return cutLight(packet)
            ClientboundBlockUpdatePacket.ID -> {
                val update = decode(packet) { ClientboundBlockUpdatePacket.read(it) }
                return update.pos.y in 0..MAX_Y
            }
            UpdateAttributesPacket.ID -> {
                // 1.17 made the list of attributes a varint; 1.16 counts them in four bytes.
                val input = decode(packet) { it }
                val entityId = input.readVarInt()
                val count = input.readVarInt()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeInt(count)
                    output.writeBytes(rest)
                }
            }
            SetTitleTextPacket.ID -> return asTitle(packet, SET_TITLE)
            SetSubtitleTextPacket.ID -> return asTitle(packet, SET_SUBTITLE)
            SetActionBarTextPacket.ID -> return asTitle(packet, SET_ACTION_BAR)
            SetTitlesAnimationPacket.ID -> {
                val times = decode(packet) { SetTitlesAnimationPacket.read(it) }
                packet.overrideId(TITLE)
                encode(packet) { output ->
                    output.writeVarInt(SET_TIMES)
                    output.writeInt(times.fadeIn)
                    output.writeInt(times.stay)
                    output.writeInt(times.fadeOut)
                }
            }
            ClearTitlesPacket.ID -> {
                val clear = decode(packet) { ClearTitlesPacket.read(it) }
                packet.overrideId(TITLE)
                encode(packet) { it.writeVarInt(if (clear.resetTimes) RESET else HIDE) }
            }
            SectionBlocksUpdatePacket.ID -> {
                // The link to 1.19.4 put the light flag back in front of the changes, so the
                // packet here is no longer the one this server writes.
                val input = decode(packet) { it }
                val packed = input.readLong()
                val suppressLight = input.readByte()
                val sectionY = ((packed shl 44) shr 44).toInt()
                if (sectionY !in FIRST_SECTION..LAST_SECTION) return false
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeLong(
                        SectionBlocksUpdatePacket.packSection(
                            ((packed shl 0) shr 42).toInt(),
                            sectionY - FIRST_SECTION,
                            ((packed shl 22) shr 42).toInt(),
                        ),
                    )
                    output.writeByte(suppressLight)
                    output.writeBytes(rest)
                }
            }
        }
        return true
    }

    /** One of the three halves of a title that carry text, as the one packet 1.16 has for all of it. */
    private fun asTitle(
        packet: MutablePacket,
        action: Int,
    ): Boolean {
        // Already a string of json by here: the link to 1.20.2 put the text back.
        val json = decode(packet) { it.readProtocolString(MAX_TEXT) }
        packet.overrideId(TITLE)
        encode(packet) { output ->
            output.writeVarInt(action)
            output.writeProtocolString(json, MAX_TEXT)
        }
        return true
    }

    /**
     * Keeps the sixteen sections a 1.16 client has room for, and says which of them are there in
     * the one number it reads.
     */
    private fun cutChunk(packet: MutablePacket): Boolean {
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val mask = LongArray(input.readVarInt()) { input.readLong() }
        val heightmaps = input.readAnonymousNbt()
        val biomes = IntArray(input.readVarInt()) { input.readVarInt() }
        val sections = ByteBuffer().apply { writeBytes(input.readBytes(input.readVarInt())) }

        // The sections arrive in order, but only the ones the mask names are in the stream.
        var newMask = 0
        val kept = ByteBuffer()
        var index = 0
        while (sections.readableBytes > 0) {
            while (index < TOTAL_SECTIONS && !isSet(mask, index)) index++
            if (index >= TOTAL_SECTIONS) break
            val section = ByteBuffer()
            section.writeShort(sections.readShort())
            copyContainer(sections, section, BLOCKS_PER_SECTION, BLOCK_MAX_INDIRECT_BITS)
            if (index in FIRST_SECTION..LAST_SECTION) {
                newMask = newMask or (1 shl (index - FIRST_SECTION))
                kept.writeBytes(section.readBytes(section.readableBytes))
            }
            index++
        }

        val data = kept.toByteArray()
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeBoolean(true)
            output.writeVarInt(newMask)
            output.writeAnonymousNbt(loweredHeightmaps(heightmaps))
            output.writeVarInt(BIOMES_IN_1_16)
            for (i in 0 until BIOMES_IN_1_16) {
                val source = FIRST_SECTION * BIOMES_PER_SECTION + i
                output.writeVarInt(biomes.getOrElse(source) { 0 })
            }
            output.writeVarInt(data.size)
            output.writeBytes(data)
            output.writeVarInt(0)
        }
        return true
    }

    /**
     * The same cut for the light, whose masks are a number here and whose arrays follow in order.
     *
     * 1.16 lights one section above and one below the world, which is why the band is eighteen
     * wide rather than sixteen.
     */
    private fun cutLight(packet: MutablePacket): Boolean {
        val input = decode(packet) { it }
        val x = input.readVarInt()
        val z = input.readVarInt()
        val trustEdges = input.readByte()
        val skyMask = LongArray(input.readVarInt()) { input.readLong() }
        val blockMask = LongArray(input.readVarInt()) { input.readLong() }
        val emptySkyMask = LongArray(input.readVarInt()) { input.readLong() }
        val emptyBlockMask = LongArray(input.readVarInt()) { input.readLong() }
        val skyLight = List(input.readVarInt()) { input.readBytes(input.readVarInt()) }
        val blockLight = List(input.readVarInt()) { input.readBytes(input.readVarInt()) }

        val sky = cutLightArrays(skyMask, skyLight)
        val block = cutLightArrays(blockMask, blockLight)
        encode(packet) { output ->
            output.writeVarInt(x)
            output.writeVarInt(z)
            output.writeByte(trustEdges)
            output.writeVarInt(sky.mask)
            output.writeVarInt(block.mask)
            output.writeVarInt(cutMask(emptySkyMask))
            output.writeVarInt(cutMask(emptyBlockMask))
            for (array in sky.arrays) {
                output.writeVarInt(array.size)
                output.writeBytes(array)
            }
            for (array in block.arrays) {
                output.writeVarInt(array.size)
                output.writeBytes(array)
            }
        }
        return true
    }

    private class CutLight(
        val mask: Int,
        val arrays: List<ByteArray>,
    )

    private fun cutLightArrays(
        mask: LongArray,
        arrays: List<ByteArray>,
    ): CutLight {
        var newMask = 0
        val kept = mutableListOf<ByteArray>()
        var arrayIndex = 0
        for (section in 0 until TOTAL_LIGHT_SECTIONS) {
            if (!isSet(mask, section)) continue
            val array = arrays.getOrNull(arrayIndex++) ?: continue
            val moved = section - FIRST_LIGHT_SECTION
            if (moved in 0 until LIGHT_SECTIONS_IN_1_16) {
                newMask = newMask or (1 shl moved)
                kept.add(array)
            }
        }
        return CutLight(newMask, kept)
    }

    private fun cutMask(mask: LongArray): Int {
        var out = 0
        for (section in 0 until TOTAL_LIGHT_SECTIONS) {
            if (!isSet(mask, section)) continue
            val moved = section - FIRST_LIGHT_SECTION
            if (moved in 0 until LIGHT_SECTIONS_IN_1_16) out = out or (1 shl moved)
        }
        return out
    }

    /**
     * The heightmaps with the floor put back at zero.
     *
     * They count from the bottom of the world, which is sixty four blocks lower than a 1.16 client
     * believes; left alone, rain and particles land that far off.
     */
    private fun loweredHeightmaps(tag: org.basaltmc.wire.nbt.NbtTag): NbtCompound {
        val root = tag as? NbtCompound ?: return NbtCompound()
        val lowered = NbtCompound()
        for (key in root.keys) {
            val data = (root[key] as? NbtLongArray)?.value ?: continue
            val storage = BitStorage(HEIGHTMAP_BITS, HEIGHTMAP_ENTRIES, data)
            val rebuilt = BitStorage(HEIGHTMAP_BITS, HEIGHTMAP_ENTRIES)
            for (i in 0 until HEIGHTMAP_ENTRIES) {
                rebuilt[i] = (storage[i] - WORLD_FLOOR).coerceIn(0, MAX_Y + 1)
            }
            lowered.putLongArray(key, rebuilt.data)
        }
        return lowered
    }

    private fun isSet(
        mask: LongArray,
        index: Int,
    ): Boolean {
        val word = index / Long.SIZE_BITS
        return word < mask.size && (mask[word] and (1L shl (index % Long.SIZE_BITS))) != 0L
    }

    private fun copyContainer(
        input: ByteBuffer,
        output: ByteBuffer,
        size: Int,
        maxIndirectBits: Int,
    ) {
        val bits = input.readByte().toInt() and 0xFF
        output.writeByte(bits.toByte())
        if (bits == 0) {
            output.writeVarInt(input.readVarInt())
            output.writeVarInt(input.readVarInt())
            return
        }
        if (bits <= maxIndirectBits) {
            val entries = input.readVarInt()
            output.writeVarInt(entries)
            repeat(entries) { output.writeVarInt(input.readVarInt()) }
        }
        val longs = input.readVarInt()
        output.writeVarInt(longs)
        repeat(longs) { output.writeLong(input.readLong()) }
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        // What a client says about itself grew: 1.17 added whether it filters text. The link to
        // 1.17.1 fills in the other one 1.18 added.
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

        /** This server's world: twenty four sections from -64, of which 1.16 can hold sixteen. */
        const val TOTAL_SECTIONS = 24
        const val FIRST_SECTION = 4
        const val LAST_SECTION = 19
        const val WORLD_FLOOR = 64
        const val MAX_Y = 255

        const val TOTAL_LIGHT_SECTIONS = TOTAL_SECTIONS + 2
        const val FIRST_LIGHT_SECTION = FIRST_SECTION
        const val LIGHT_SECTIONS_IN_1_16 = 18

        const val BLOCKS_PER_SECTION = 16 * 16 * 16
        const val BLOCK_MAX_INDIRECT_BITS = 8
        const val BIOMES_PER_SECTION = 4 * 4 * 4
        const val BIOMES_IN_1_16 = 1024

        const val HEIGHTMAP_BITS = 9
        const val HEIGHTMAP_ENTRIES = 256

        /** parity: `ClientboundSetTitlesPacket.Type`, the one packet 1.16 had for all of it. */
        const val TITLE = 0x4F
        const val SET_TITLE = 0
        const val SET_SUBTITLE = 1
        const val SET_ACTION_BAR = 2
        const val SET_TIMES = 3
        const val HIDE = 4
        const val RESET = 5
        const val MAX_TEXT = 262144
    }
}
