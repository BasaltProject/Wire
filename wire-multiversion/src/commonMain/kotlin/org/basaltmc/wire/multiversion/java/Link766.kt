package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.play.LevelEventPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionLookPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemPacket
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeUuid
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.21 to 1.20.5 and 1.20.6.
 *
 * The three registries 1.21 invented (enchantments, paintings, jukebox songs) leave the
 * configuration, the rotation 1.21 added to using an item is filled in from the last movement, and
 * an attribute modifier goes back to being a uuid, made from its name so that it stays the same.
 */
@OptIn(kotlin.uuid.ExperimentalUuidApi::class)
internal class Link766(
    tables: VersionTables,
) : RenumberingLink(tables) {
    private var yaw = 0f
    private var pitch = 0f

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state == ProtocolState.CONFIGURATION && packet.nativeId == RegistryDataPacket.ID) {
            val registry = decode(packet) { it.readProtocolString() }
            return registry !in REGISTRIES_1_21_ADDED
        }
        if (state == ProtocolState.PLAY && packet.nativeId == UpdateAttributesPacket.ID) {
            unnameModifiers(packet)
            return true
        }
        if (state == ProtocolState.PLAY && packet.nativeId == LevelEventPacket.ID) {
            val event = decode(packet) { LevelEventPacket.read(it) }
            // The song a jukebox plays is a registry id in 1.21 and an item id before it. There is
            // no way to name it over there, so the record simply does not play.
            if (event.event == JUKEBOX_PLAY) return false
        }
        return true
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            ServerboundPositionLookPacket.ID -> {
                val move = decode(packet) { ServerboundPositionLookPacket.read(it) }
                yaw = move.yaw
                pitch = move.pitch
            }
            MOVE_PLAYER_ROT -> {
                decode(packet) {
                    yaw = it.readFloat()
                    pitch = it.readFloat()
                }
            }
            ServerboundUseItemPacket.ID -> {
                encode(packet) { output ->
                    output.writeBytes(packet.body)
                    output.writeFloat(yaw)
                    output.writeFloat(pitch)
                }
            }
        }
        return true
    }

    private fun unnameModifiers(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val attributes =
            List(input.readVarInt()) {
                val id = input.readVarInt()
                val base = input.readDouble()
                val modifiers =
                    List(input.readVarInt()) {
                        Triple(input.readProtocolString(), input.readDouble(), input.readByte())
                    }
                Triple(id, base, modifiers)
            }
        encode(packet) { output ->
            output.writeVarInt(entityId)
            output.writeVarInt(attributes.size)
            for ((id, base, modifiers) in attributes) {
                output.writeVarInt(id)
                output.writeDouble(base)
                output.writeVarInt(modifiers.size)
                for ((name, amount, operation) in modifiers) {
                    output.writeUuid(uuidOf(name))
                    output.writeDouble(amount)
                    output.writeByte(operation)
                }
            }
        }
    }

    /** The same name always gives the same uuid, which is the whole of what a client needs. */
    private fun uuidOf(name: String): kotlin.uuid.Uuid =
        kotlin.uuid.Uuid.fromLongs(hash(name, HIGH_SEED), hash(name, LOW_SEED))

    private fun hash(
        name: String,
        seed: Long,
    ): Long {
        var value = seed
        for (character in name) {
            value = value xor character.code.toLong()
            value *= FNV_PRIME
        }
        return value
    }

    private companion object {
        const val FNV_PRIME = 0x100000001B3L
        const val HIGH_SEED = -0x340d631b7bdddcdbL
        const val LOW_SEED = 0x7ed55d16aa5d1f27L

        /** parity: `LevelEvent.SOUND_PLAY_JUKEBOX_SONG`. */
        const val JUKEBOX_PLAY = 1010
        const val MOVE_PLAYER_ROT = 32

        val REGISTRIES_1_21_ADDED =
            setOf("minecraft:enchantment", "minecraft:painting_variant", "minecraft:jukebox_song")
    }
}
