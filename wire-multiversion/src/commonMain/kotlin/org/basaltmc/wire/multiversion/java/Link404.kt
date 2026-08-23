package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ChangeDifficultyPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.14 to 1.13.2, where the light is part of the chunk.
 *
 * 1.14 took the light out of the chunk and gave it a packet of its own. Below that it belongs
 * inside each section, so the chunk is held back until the light for it arrives (this server sends
 * one straight after the other) and the two are sent as the one packet 1.13 reads.
 *
 * The rest is fields either version has and the other does not: the difficulty travelled in the
 * login packet then, the view distance did not exist, and a chunk had no heightmaps.
 */
internal class Link404(
    tables: VersionTables,
) : RenumberingLink(tables) {
    private var pending: PendingChunk? = null
    private var hasSkyLight = true

    private class PendingChunk(
        val x: Int,
        val z: Int,
        val fullChunk: Byte,
        val mask: Int,
        val sections: ByteArray,
        val rest: ByteArray,
    )

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            MapChunkPacket.ID -> {
                holdChunk(packet)
                return false
            }
            UpdateLightPacket.ID -> return lightTheChunk(packet)
            LoginPlayPacket.ID -> rewriteLogin(packet)
            RespawnPacket.ID -> rewriteRespawn(packet)
            ChangeDifficultyPacket.ID -> {
                val difficulty = decode(packet) { it.readByte() }
                encode(packet) { it.writeByte(difficulty) }
            }
            // Only the generic spawn: a mob or a player is its own packet by here, with its own
            // shape, and the links above made it one.
            SpawnEntityPacket.ID -> if (packet.id == ADD_ENTITY) narrowType(packet)
        }
        return true
    }

    private fun holdChunk(packet: MutablePacket) {
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val fullChunk = input.readByte()
        val mask = input.readVarInt()
        input.readAnonymousNbt()
        val sections = input.readBytes(input.readVarInt())
        pending = PendingChunk(x, z, fullChunk, mask, sections, input.readBytes(input.readableBytes))
    }

    /**
     * The chunk that was held back, with the light of this packet inside its sections.
     *
     * The light arrives as one array per section of the column plus one above and one below, which
     * is why the section at index zero is the one under the world and is skipped.
     */
    private fun lightTheChunk(packet: MutablePacket): Boolean {
        val held = pending ?: return false
        pending = null
        val input = decode(packet) { it }
        val x = input.readVarInt()
        val z = input.readVarInt()
        if (x != held.x || z != held.z) return false
        val skyMask = input.readVarInt()
        val blockMask = input.readVarInt()
        input.readVarInt()
        input.readVarInt()
        // How many arrays there are is the masks: the count is not sent.
        val skyLight = List(skyMask.countOneBits()) { input.readBytes(input.readVarInt()) }
        val blockLight = List(blockMask.countOneBits()) { input.readBytes(input.readVarInt()) }

        val sections = ByteBuffer().apply { writeBytes(held.sections) }
        val data = ByteBuffer()
        var sky = 0
        var block = 0
        for (index in 0 until SECTIONS) {
            val skyHere = if (skyMask and (1 shl (index + 1)) != 0) skyLight.getOrNull(sky++) else null
            val blockHere = if (blockMask and (1 shl (index + 1)) != 0) blockLight.getOrNull(block++) else null
            if (held.mask and (1 shl index) == 0) continue
            sections.readShort()
            copyContainer(sections, data)
            data.writeBytes(blockHere ?: DARK)
            if (hasSkyLight) data.writeBytes(skyHere ?: DARK)
        }
        data.writeBytes(sections.readBytes(sections.readableBytes))

        val body = data.toByteArray()
        // From here down it is a chunk, not the light it arrived as: the links below have to know.
        packet.becomes(MapChunkPacket.ID)
        packet.overrideId(MAP_CHUNK)
        encode(packet) { output ->
            output.writeInt(held.x)
            output.writeInt(held.z)
            output.writeByte(held.fullChunk)
            output.writeVarInt(held.mask)
            output.writeVarInt(body.size)
            output.writeBytes(body)
            output.writeBytes(held.rest)
        }
        return true
    }

    private fun copyContainer(
        input: ByteBuffer,
        output: ByteBuffer,
    ) {
        val bits = input.readByte().toInt() and 0xFF
        output.writeByte(bits.toByte())
        if (bits <= MAX_INDIRECT_BITS) {
            val entries = input.readVarInt()
            output.writeVarInt(entries)
            repeat(entries) { output.writeVarInt(input.readVarInt()) }
        }
        val longs = input.readVarInt()
        output.writeVarInt(longs)
        repeat(longs) { output.writeLong(input.readLong()) }
    }

    private fun rewriteLogin(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readInt()
        val gamemode = input.readByte()
        val dimension = input.readInt()
        val maxPlayers = input.readByte()
        val levelType = input.readProtocolString()
        input.readVarInt()
        val reducedDebugInfo = input.readByte()
        hasSkyLight = dimension == OVERWORLD
        encode(packet) { output ->
            output.writeInt(entityId)
            output.writeByte(gamemode)
            output.writeInt(dimension)
            output.writeByte(NORMAL)
            output.writeByte(maxPlayers)
            output.writeProtocolString(levelType)
            output.writeByte(reducedDebugInfo)
        }
    }

    private fun rewriteRespawn(packet: MutablePacket) {
        val input = decode(packet) { it }
        val dimension = input.readInt()
        val gamemode = input.readByte()
        val levelType = input.readProtocolString()
        hasSkyLight = dimension == OVERWORLD
        encode(packet) { output ->
            output.writeInt(dimension)
            output.writeByte(NORMAL)
            output.writeByte(gamemode)
            output.writeProtocolString(levelType)
        }
    }

    /** 1.14 made the kind of a spawned thing a varint. Below it there is one byte for it. */
    private fun narrowType(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val mostSignificant = input.readLong()
        val leastSignificant = input.readLong()
        val type = input.readVarInt()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeVarInt(entityId)
            output.writeLong(mostSignificant)
            output.writeLong(leastSignificant)
            output.writeByte(type.toByte())
            output.writeBytes(rest)
        }
    }

    private companion object {
        const val ADD_ENTITY = 0x00
        const val SECTIONS = 16
        const val MAX_INDIRECT_BITS = 8
        const val MAP_CHUNK = 0x22
        const val OVERWORLD = 0

        /** parity: the difficulty stopped travelling with the world, so it is told the usual one. */
        const val NORMAL: Byte = 2

        /** A section with no light of its own is dark rather than absent: 1.13 has no way to say. */
        val DARK = ByteArray(2048)
    }
}
