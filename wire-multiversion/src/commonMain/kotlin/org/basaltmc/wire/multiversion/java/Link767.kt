package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.play.ContainerClosePacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetDataPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.CooldownPacket
import org.basaltmc.wire.protocol.play.EntityPositionSyncPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.ExplodePacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
import org.basaltmc.wire.protocol.play.RecipeBookAddPacket
import org.basaltmc.wire.protocol.play.RecipeBookRemovePacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.ServerboundContainerClickPacket
import org.basaltmc.wire.protocol.play.SetHeldSlotPacket
import org.basaltmc.wire.protocol.play.SpawnInfo
import org.basaltmc.wire.protocol.play.UpdateRecipesPacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.math.roundToInt

/**
 * 1.21.2 to 1.21, the deepest step of the modern range.
 *
 * 1.21.2 rewrote how a position reaches a client, gave the world a sea level and the login a
 * strictness flag, and made the container id a varint where 1.21 reads one byte.
 *
 * Dropped rather than sent wrong: the recipe book, explosions and item cooldowns.
 */
internal class Link767(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state == ProtocolState.LOGIN && packet.nativeId == LoginFinishedPacket.ID) {
            // 1.21 reads one more flag after the profile. Newer clients behave as if it were on.
            encode(packet) { output ->
                output.writeBytes(packet.body)
                output.writeBoolean(true)
            }
            // 1.21 calls it game_profile, so the tables see the name as gone and would drop it.
            packet.overrideId(LoginFinishedPacket.ID)
            return true
        }
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            LoginPlayPacket.ID -> {
                // The online-mode flag is already gone: 26.2 to 26.1 took it out.
                val login = decode(packet) { LoginFields.read(it, withOnlineMode = false, withSeaLevel = true) }
                encode(packet) { login.write(it, withOnlineMode = false, withSeaLevel = false) }
            }
            RespawnPacket.ID -> {
                val respawn = decode(packet) { RespawnPacket.read(it) }
                encode(packet) { output ->
                    writeSpawnInfoWithoutSeaLevel(output, respawn.spawnInfo)
                    output.writeByte(respawn.dataToKeep)
                }
            }
            PlayerPositionPacket.ID -> {
                val position = decode(packet) { PlayerPositionPacket.read(it) }
                encode(packet) { output ->
                    output.writeDouble(position.x)
                    output.writeDouble(position.y)
                    output.writeDouble(position.z)
                    output.writeFloat(position.yaw)
                    output.writeFloat(position.pitch)
                    // The same bits, in the byte 1.21 keeps them in. The delta movement has nowhere
                    // to go: a client that cannot be told about it keeps whatever it had.
                    output.writeByte(position.relativeFlags.toByte())
                    output.writeVarInt(position.teleportId)
                }
            }
            EntityPositionSyncPacket.ID -> {
                val sync = decode(packet) { EntityPositionSyncPacket.read(it) }
                packet.overrideId(TELEPORT_ENTITY_767)
                encode(packet) { output ->
                    output.writeVarInt(sync.entityId)
                    output.writeDouble(sync.x)
                    output.writeDouble(sync.y)
                    output.writeDouble(sync.z)
                    output.writeByte(angleOf(sync.yaw))
                    output.writeByte(angleOf(sync.pitch))
                    output.writeBoolean(sync.onGround)
                }
            }
            EntityTeleportPacket.ID -> {
                val teleport = decode(packet) { EntityTeleportPacket.read(it) }
                packet.overrideId(TELEPORT_ENTITY_767)
                encode(packet) { output ->
                    output.writeVarInt(teleport.entityId)
                    output.writeDouble(teleport.x)
                    output.writeDouble(teleport.y)
                    output.writeDouble(teleport.z)
                    output.writeByte(angleOf(teleport.yaw))
                    output.writeByte(angleOf(teleport.pitch))
                    output.writeBoolean(teleport.onGround)
                }
            }
            SetHeldSlotPacket.ID -> packet.overrideId(SET_CARRIED_ITEM_767)
            ContainerSetSlotPacket.ID, ContainerSetContentPacket.ID,
            ContainerClosePacket.CLIENTBOUND_ID, ContainerSetDataPacket.ID,
            -> narrowContainer(packet)
            CooldownPacket.ID, ExplodePacket.ID, UpdateRecipesPacket.ID,
            RecipeBookAddPacket.ID, RecipeBookRemovePacket.ID,
            -> return false
        }
        return true
    }

    /**
     * The container a packet is about, as the one byte 1.21.1 reads.
     *
     * 1.21.2 made it a varint, and the two agree on every container there is except the one that
     * matters: minus one, the player's own screen, is one byte there and five here.
     */
    private fun narrowContainer(packet: MutablePacket) {
        val input = decode(packet) { it }
        val container = input.readVarInt()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeByte(container.toByte())
            output.writeBytes(rest)
        }
    }

    /** The same the other way, for what the client sends back about a container. */
    private fun widenContainer(packet: MutablePacket) {
        val input = decode(packet) { it }
        val container = input.readByte().toInt()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeVarInt(container)
            output.writeBytes(rest)
        }
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        // The client's own settings gained the particle setting in 1.21.2; a 1.21 client stops one
        // field short, so the one this server reads is written in for it.
        val clientInformation =
            (state == ProtocolState.CONFIGURATION && packet.nativeId == CLIENT_INFORMATION_CONFIGURATION) ||
                (state == ProtocolState.PLAY && packet.nativeId == CLIENT_INFORMATION_PLAY)
        if (state == ProtocolState.PLAY &&
            (
                packet.nativeId == ContainerClosePacket.SERVERBOUND_ID ||
                    packet.nativeId == ServerboundContainerClickPacket.ID
            )
        ) {
            widenContainer(packet)
            return true
        }
        if (clientInformation) {
            encode(packet) { output ->
                output.writeBytes(packet.body)
                output.writeVarInt(PARTICLES_ALL)
            }
        }
        return true
    }

    /** Everything a spawn tells the client, minus the sea level 1.21.2 added at the end. */
    private fun writeSpawnInfoWithoutSeaLevel(
        output: ByteBuffer,
        spawn: SpawnInfo,
    ) {
        val whole = ByteBuffer().also { spawn.write(it) }
        val bytes = whole.readBytes(whole.readableBytes)
        val trailing = ByteBuffer().apply { writeBytes(bytes) }
        // Walk to the sea level and stop: it is a varint at the very end, so what comes before it
        // is written straight out.
        val kept = bytes.size - varIntSize(spawn.seaLevel)
        output.writeBytes(trailing.readBytes(kept))
    }

    private fun varIntSize(value: Int): Int {
        var size = 1
        var remaining = value ushr 7
        while (remaining != 0) {
            size++
            remaining = remaining ushr 7
        }
        return size
    }

    private companion object {
        /** 1.21's own numbering for the two packets that were renamed or replaced under it. */
        const val TELEPORT_ENTITY_767 = 112
        const val SET_CARRIED_ITEM_767 = 83
        const val CLIENT_INFORMATION_PLAY = 14
        const val CLIENT_INFORMATION_CONFIGURATION = 0x00
        const val PARTICLES_ALL = 0

        fun angleOf(degrees: Float): Byte = (degrees * 256f / 360f).roundToInt().toByte()
    }
}
