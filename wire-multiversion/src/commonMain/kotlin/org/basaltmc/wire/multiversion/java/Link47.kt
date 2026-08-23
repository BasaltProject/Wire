package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.CompactBitStorage
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.EntityPositionSyncPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.9 to 1.8, where a position is fixed point and a chunk has no palette.
 *
 * 1.8 writes every block as a short of `id << 4 | metadata`, and the light of all the sections
 * behind all of them rather than section by section, so the chunk is decoded and written again.
 *
 * The data an entity carries does not travel: 1.8 packs it a different way entirely.
 */
internal class Link47(
    tables: VersionTables,
) : RenumberingLink(tables) {
    private var hasSkyLight = true

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            MapChunkPacket.ID -> return rewriteChunk(packet)
            ForgetLevelChunkPacket.ID -> emptyChunk(packet)
            SetEntityDataPacket.ID -> return false
            SpawnEntityPacket.ID -> return rewriteSpawn(packet)
            EntityTeleportPacket.ID, EntityPositionSyncPacket.ID -> fixedPointTeleport(packet)
            SetEquipmentPacket.ID -> return rewriteEquipment(packet)
            SoundPacket.ID -> dropCategory(packet)
            PlayerPositionPacket.ID -> dropTeleportId(packet)
            LoginPlayPacket.ID -> rememberDimension(packet, afterEntityAndGamemode = true)
            RespawnPacket.ID -> rememberDimension(packet, afterEntityAndGamemode = false)
        }
        return true
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            CLIENT_INFORMATION -> {
                // 1.9 asked which hand is the main one. There is one hand there, and it is the right.
                val body = packet.body
                encode(packet) { output ->
                    output.writeBytes(body)
                    output.writeVarInt(MAIN_HAND_RIGHT)
                }
            }
            // Placing a block is the one thing a 1.8 client says that this server cannot read: it
            // sends the stack it believes it is holding and the cursor in sixteenths, and what
            // reads it here wants a hand, a hit and a sequence number that 1.8 has no idea about.
            // Dropped rather than half-read: a misread packet takes the connection with it.
            ServerboundUseItemOnPacket.ID -> return false
        }
        return true
    }

    /**
     * The chunk as 1.8 reads it: every block a short, and the light behind all of them.
     *
     * The values arrive packed the old way (a value may span two longs), which is what everything
     * below 1.16 uses, so they are unpacked, looked up in the palette and written out flat.
     */
    private fun rewriteChunk(packet: MutablePacket): Boolean {
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val fullChunk = input.readByte().toInt() != 0
        val mask = input.readVarInt()
        val data = ByteBuffer().apply { writeBytes(input.readBytes(input.readVarInt())) }

        val blocks = ByteBuffer()
        val blockLight = ByteBuffer()
        val skyLight = ByteBuffer()
        for (index in 0 until SECTIONS) {
            if (mask and (1 shl index) == 0) continue
            val values = readContainer(data)
            for (value in values) {
                blocks.writeByte((value and 0xFF).toByte())
                blocks.writeByte(((value ushr 8) and 0xFF).toByte())
            }
            blockLight.writeBytes(data.readBytes(LIGHT_BYTES))
            if (hasSkyLight) skyLight.writeBytes(data.readBytes(LIGHT_BYTES))
        }
        val biomes = if (fullChunk) data.readBytes(minOf(COLUMNS, data.readableBytes)) else ByteArray(0)

        val body = ByteBuffer()
        body.writeBytes(blocks.toByteArray())
        body.writeBytes(blockLight.toByteArray())
        body.writeBytes(skyLight.toByteArray())
        body.writeBytes(biomes)
        val whole = body.toByteArray()
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeBoolean(fullChunk)
            output.writeShort(mask.toShort())
            output.writeVarInt(whole.size)
            output.writeBytes(whole)
        }
        return true
    }

    /** parity: 1.8 forgets a chunk by being sent an empty one, because it has no packet for it. */
    private fun emptyChunk(packet: MutablePacket) {
        val chunk = decode(packet) { ForgetLevelChunkPacket.read(it) }
        packet.overrideId(MAP_CHUNK_47)
        encode(packet) { output ->
            output.writeInt(chunk.chunkX)
            output.writeInt(chunk.chunkZ)
            output.writeBoolean(true)
            output.writeShort(0)
            output.writeVarInt(0)
        }
    }

    /** The values of one section, whatever width and palette it was written with. */
    private fun readContainer(input: ByteBuffer): IntArray {
        val bits = input.readByte().toInt() and 0xFF
        val palette = if (bits <= MAX_INDIRECT_BITS) IntArray(input.readVarInt()) { input.readVarInt() } else null
        val longs = LongArray(input.readVarInt()) { input.readLong() }
        val values = CompactBitStorage.unpack(longs, bits, BLOCKS_PER_SECTION)
        if (palette == null) return values
        for (index in values.indices) values[index] = palette.getOrElse(values[index]) { 0 }
        return values
    }

    /**
     * A spawn, with its position in thirty-seconds of a block and no uuid.
     *
     * Which of the three spawns this is has to be read off the number the versions above gave it:
     * this server has one packet for all of them and the links above split it.
     */
    private fun rewriteSpawn(packet: MutablePacket): Boolean {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val mostSignificant = input.readLong()
        val leastSignificant = input.readLong()
        when (packet.id) {
            ADD_ENTITY -> {
                val type = input.readByte()
                val x = input.readDouble()
                val y = input.readDouble()
                val z = input.readDouble()
                val pitch = input.readByte()
                val yaw = input.readByte()
                val data = input.readInt()
                val velocity = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeByte(type)
                    writeFixed(output, x, y, z)
                    output.writeByte(pitch)
                    output.writeByte(yaw)
                    output.writeInt(data)
                    // 1.8 only carries the velocity when there is object data to go with it.
                    if (data != 0) output.writeBytes(velocity)
                }
            }
            ADD_MOB -> {
                val type = input.readByte()
                val x = input.readDouble()
                val y = input.readDouble()
                val z = input.readDouble()
                val yaw = input.readByte()
                val pitch = input.readByte()
                val headPitch = input.readByte()
                val velocity = input.readBytes(VELOCITY_BYTES)
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeByte(type)
                    writeFixed(output, x, y, z)
                    output.writeByte(yaw)
                    output.writeByte(pitch)
                    output.writeByte(headPitch)
                    output.writeBytes(velocity)
                    output.writeByte(END_OF_DATA)
                }
            }
            ADD_PLAYER -> {
                val x = input.readDouble()
                val y = input.readDouble()
                val z = input.readDouble()
                val yaw = input.readByte()
                val pitch = input.readByte()
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeLong(mostSignificant)
                    output.writeLong(leastSignificant)
                    writeFixed(output, x, y, z)
                    output.writeByte(yaw)
                    output.writeByte(pitch)
                    output.writeShort(0)
                    output.writeByte(END_OF_DATA)
                }
            }
            else -> return false
        }
        return true
    }

    /**
     * A teleport, with its position in thirty-seconds of a block.
     *
     * Both of this server's ways of saying where something is are one packet by the time they get
     * here: the link to 1.21 turned the exact one into a teleport, and the angles are already the
     * byte each that every version below it reads.
     */
    private fun fixedPointTeleport(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val x = input.readDouble()
        val y = input.readDouble()
        val z = input.readDouble()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeVarInt(entityId)
            writeFixed(output, x, y, z)
            output.writeBytes(rest)
        }
    }

    /** 1.8 has five slots and no second hand, and numbers them from the one being held. */
    private fun rewriteEquipment(packet: MutablePacket): Boolean {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val slot = input.readVarInt()
        val stack = input.readBytes(input.readableBytes)
        val old =
            when (slot) {
                MAIN_HAND -> 0
                OFF_HAND -> return false
                else -> slot - 1
            }
        encode(packet) { output ->
            output.writeVarInt(entityId)
            output.writeShort(old.toShort())
            output.writeBytes(stack)
        }
        return true
    }

    private fun dropCategory(packet: MutablePacket) {
        val input = decode(packet) { it }
        val named = packet.id == NAMED_SOUND_47
        if (!named) return
        val name = input.readProtocolString()
        input.readVarInt()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeProtocolString(name)
            output.writeBytes(rest)
        }
    }

    private fun dropTeleportId(packet: MutablePacket) {
        val input = decode(packet) { it }
        val head = input.readBytes(POSITION_HEAD_BYTES)
        encode(packet) { it.writeBytes(head) }
    }

    /** Whether this world has a sky, which is what says if a chunk carries sky light. */
    private fun rememberDimension(
        packet: MutablePacket,
        afterEntityAndGamemode: Boolean,
    ) {
        val input = decode(packet) { it }
        if (afterEntityAndGamemode) {
            input.readInt()
            input.readByte()
        }
        hasSkyLight = input.readByte().toInt() == OVERWORLD
    }

    private fun writeFixed(
        output: ByteBuffer,
        x: Double,
        y: Double,
        z: Double,
    ) {
        output.writeInt((x * FIXED_POINT).toInt())
        output.writeInt((y * FIXED_POINT).toInt())
        output.writeInt((z * FIXED_POINT).toInt())
    }

    private companion object {
        const val SECTIONS = 16
        const val BLOCKS_PER_SECTION = 16 * 16 * 16
        const val MAX_INDIRECT_BITS = 8
        const val LIGHT_BYTES = 2048
        const val COLUMNS = 16 * 16
        const val VELOCITY_BYTES = 6
        const val POSITION_HEAD_BYTES = 8 * 3 + 4 * 2 + 1

        /** What 1.9 calls the packets this one has to know apart, in that version's numbering. */
        const val ADD_ENTITY = 0x00
        const val ADD_MOB = 0x03
        const val ADD_PLAYER = 0x05
        const val NAMED_SOUND_47 = 0x19
        const val MAP_CHUNK_47 = 0x21

        /** parity: `ServerboundClientInformationPacket` in play, which is where 1.8 sends it. */
        const val CLIENT_INFORMATION = 14

        const val MAIN_HAND = 0
        const val OFF_HAND = 1
        const val MAIN_HAND_RIGHT = 1
        const val OVERWORLD = 0

        /** parity: `EntityDataSerializer` ended a list with 127 before the indexed format. */
        const val END_OF_DATA = 0x7F.toByte()

        /** parity: a position was sent in thirty-seconds of a block. */
        const val FIXED_POINT = 32.0
    }
}
