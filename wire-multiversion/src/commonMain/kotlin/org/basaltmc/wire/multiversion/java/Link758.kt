package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.19 to 1.18.2, the version before the chat was signed and before every entity spawned the same
 * way.
 *
 * Three things go back: anything alive spawns with its own packet again and with its angles the
 * other way round, a sound loses the seed 1.19 gave it, and a system message becomes an ordinary
 * chat with a position byte.
 */
internal class Link758(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            SpawnEntityPacket.ID -> {
                // A player's spawn is its own packet by here, and its body is not this shape:
                // the link to 1.19.3 already made it one. Only the spawn everything else uses is
                // still numbered zero.
                if (packet.id != ADD_ENTITY) return true
                val input = decode(packet) { it }
                val entityId = input.readVarInt()
                val mostSignificant = input.readLong()
                val leastSignificant = input.readLong()
                val type = input.readVarInt()
                if (type !in LIVING_IN_1_19) return true
                val x = input.readDouble()
                val y = input.readDouble()
                val z = input.readDouble()
                val pitch = input.readByte()
                val yaw = input.readByte()
                val headYaw = input.readByte()
                input.readVarInt() // The object data, which nothing alive uses.
                val velocityX = input.readShort()
                val velocityY = input.readShort()
                val velocityZ = input.readShort()
                packet.overrideId(ADD_MOB_758)
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeLong(mostSignificant)
                    output.writeLong(leastSignificant)
                    output.writeVarInt(type)
                    output.writeDouble(x)
                    output.writeDouble(y)
                    output.writeDouble(z)
                    // The other way round from the packet everything else spawns with.
                    output.writeByte(yaw)
                    output.writeByte(pitch)
                    output.writeByte(headYaw)
                    output.writeShort(velocityX)
                    output.writeShort(velocityY)
                    output.writeShort(velocityZ)
                }
            }
            SoundPacket.ID -> {
                val input = decode(packet) { it }
                if (input.readVarInt() != HOLDER_BY_NAME) return false
                val name = input.readProtocolString()
                input.readByte() // The fixed range, which 1.18.2 has no field for.
                val source = input.readVarInt()
                val x = input.readInt()
                val y = input.readInt()
                val z = input.readInt()
                val volume = input.readFloat()
                val pitch = input.readFloat()
                packet.overrideId(NAMED_SOUND_758)
                encode(packet) { output ->
                    output.writeProtocolString(name)
                    output.writeVarInt(source)
                    output.writeInt(x)
                    output.writeInt(y)
                    output.writeInt(z)
                    output.writeFloat(volume)
                    output.writeFloat(pitch)
                }
            }
            LoginPlayPacket.ID -> dropDeathLocation(packet, headFields = LOGIN_FIELDS_BEFORE_DEATH)
            RespawnPacket.ID -> dropDeathLocation(packet, headFields = RESPAWN_FIELDS_BEFORE_DEATH)
            SystemChatPacket.ID -> {
                // The link to 1.19 wrote the type as a number; 1.18.2 wants a position and a sender.
                val input = decode(packet) { it }
                val json = input.readProtocolString(MAX_TEXT)
                val type = input.readVarInt()
                packet.overrideId(CHAT_758)
                encode(packet) { output ->
                    output.writeProtocolString(json, MAX_TEXT)
                    output.writeByte(if (type == GAME_INFO) ABOVE_HOTBAR else SYSTEM_POSITION)
                    output.writeLong(0L)
                    output.writeLong(0L)
                }
            }
        }
        return true
    }

    /**
     * Cuts the death location off the end, which is what 1.19 added to both of these.
     *
     * The head of the packet is copied byte for byte: none of it changes here, and by this point
     * the shape is the one the links above built rather than anything this server writes.
     */
    private fun dropDeathLocation(
        packet: MutablePacket,
        headFields: List<Field>,
    ) {
        val input = decode(packet) { it }
        val head =
            org.basaltmc.wire.io
                .ByteBuffer()
        for (field in headFields) {
            when (field) {
                Field.INT -> head.writeInt(input.readInt())
                Field.LONG -> head.writeLong(input.readLong())
                Field.BYTE -> head.writeByte(input.readByte())
                Field.VAR_INT -> head.writeVarInt(input.readVarInt())
                Field.STRING -> head.writeProtocolString(input.readProtocolString())
                Field.NBT -> head.writeAnonymousNbt(input.readAnonymousNbt())
                Field.STRING_ARRAY -> {
                    val count = input.readVarInt()
                    head.writeVarInt(count)
                    repeat(count) { head.writeProtocolString(input.readProtocolString()) }
                }
            }
        }
        val kept = head.toByteArray()
        encode(packet) { it.writeBytes(kept) }
    }

    private enum class Field { INT, LONG, BYTE, VAR_INT, STRING, NBT, STRING_ARRAY }

    private companion object {
        /** What every version from 1.9 to 1.20.1 numbers the spawn of anything that is not alive. */
        const val ADD_ENTITY = 0x00

        /** Everything a login says before the death location: 1.19 put it at the very end. */
        val LOGIN_FIELDS_BEFORE_DEATH =
            listOf(
                Field.INT,
                Field.BYTE,
                Field.BYTE,
                Field.BYTE,
                Field.STRING_ARRAY,
                Field.NBT,
                Field.STRING,
                Field.STRING,
                Field.LONG,
                Field.VAR_INT,
                Field.VAR_INT,
                Field.VAR_INT,
                Field.BYTE,
                Field.BYTE,
                Field.BYTE,
                Field.BYTE,
            )

        val RESPAWN_FIELDS_BEFORE_DEATH =
            listOf(
                Field.STRING,
                Field.STRING,
                Field.LONG,
                Field.BYTE,
                Field.BYTE,
                Field.BYTE,
                Field.BYTE,
                Field.BYTE,
            )

        const val ADD_MOB_758 = 2
        const val NAMED_SOUND_758 = 25
        const val CHAT_758 = 15

        const val HOLDER_BY_NAME = 0
        const val MAX_TEXT = 262144
        const val GAME_INFO = 2
        const val SYSTEM_POSITION: Byte = 1
        const val ABOVE_HOTBAR: Byte = 2

        /**
         * Everything alive, as 1.19 numbers entity types.
         *
         * 1.19 spawns everything with one packet; before it, anything alive had its own. Read out
         * of minecraft-data's own categories for that version.
         */
        val LIVING_IN_1_19 =
            setOf(
                0,
                2,
                4,
                5,
                6,
                7,
                10,
                11,
                12,
                13,
                14,
                15,
                16,
                17,
                19,
                20,
                22,
                23,
                24,
                25,
                31,
                32,
                33,
                34,
                36,
                37,
                38,
                39,
                40,
                41,
                42,
                43,
                49,
                51,
                60,
                61,
                62,
                64,
                65,
                66,
                67,
                68,
                69,
                70,
                71,
                73,
                74,
                75,
                76,
                77,
                78,
                80,
                81,
                82,
                83,
                85,
                88,
                89,
                90,
                91,
                92,
                98,
                99,
                100,
                101,
                102,
                103,
                104,
                105,
                106,
                107,
                108,
                110,
                111,
                112,
                113,
                114,
                115,
                116,
            )
    }
}
