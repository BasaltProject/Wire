package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.OpenSignEditorPacket
import org.basaltmc.wire.protocol.play.PlayerCombatKillPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.SectionBlocksUpdatePacket
import org.basaltmc.wire.protocol.play.SignUpdatePacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.20 to 1.19.4. Small fields, in and out.
 *
 * 1.20 took the "trust edges" flag out of the light that comes with a chunk, and added a cooldown
 * to what the portal remembers when a player spawns. A sign gained a side, and the packet that says
 * who killed you lost the killer.
 */
internal class Link762(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            MapChunkPacket.ID ->
                encode(packet) { output ->
                    output.writeBytes(packet.body)
                    output.writeBoolean(true)
                }
            UpdateLightPacket.ID -> {
                // The flag sits between the position and the masks, so this one is spliced.
                val input = decode(packet) { it }
                val x = input.readVarInt()
                val z = input.readVarInt()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeVarInt(x)
                    output.writeVarInt(z)
                    output.writeBoolean(true)
                    output.writeBytes(rest)
                }
            }
            SectionBlocksUpdatePacket.ID -> {
                val input = decode(packet) { it }
                val section = input.readLong()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeLong(section)
                    output.writeBoolean(false)
                    output.writeBytes(rest)
                }
            }
            OpenSignEditorPacket.ID -> {
                val input = decode(packet) { it }
                val position = input.readLong()
                encode(packet) { it.writeLong(position) }
            }
            PlayerCombatKillPacket.ID -> {
                val input = decode(packet) { it }
                val playerId = input.readVarInt()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeVarInt(playerId)
                    // 1.19.4 still names a killer, and there is nobody to name.
                    output.writeInt(-1)
                    output.writeBytes(rest)
                }
            }
            LoginPlayPacket.ID -> dropPortalCooldown(packet, afterLogin = true)
            RespawnPacket.ID -> dropPortalCooldown(packet, afterLogin = false)
        }
        return true
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY || packet.nativeId != SignUpdatePacket.ID) return true
        // A client this old only edits the front of a sign, and has no field to say so.
        val input = decode(packet) { it }
        val position = input.readLong()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeLong(position)
            output.writeBoolean(true)
            output.writeBytes(rest)
        }
        return true
    }

    /**
     * Rewrites the packet the link below built, minus the portal cooldown 1.20 added at its end.
     *
     * Both of them end with it, and both carry a spawn that is already named rather than numbered,
     * so this walks the shape rather than the packet class: by here it is no longer one.
     */
    private fun dropPortalCooldown(
        packet: MutablePacket,
        afterLogin: Boolean,
    ) {
        val input = decode(packet) { it }
        val head =
            if (afterLogin) {
                val entityId = input.readInt()
                val hardcore = input.readByte()
                val gamemode = input.readByte()
                val previous = input.readByte()
                val worlds = List(input.readVarInt()) { input.readProtocolString() }
                val codec = input.readAnonymousNbt()
                Head(entityId, hardcore, gamemode, previous, worlds, codec)
            } else {
                null
            }
        val dimensionType = input.readProtocolString()
        val worldName = input.readProtocolString()
        val seed = input.readLong()
        val gamemode = if (afterLogin) 0 else input.readByte().toInt()
        val previousGamemode = if (afterLogin) 0 else input.readByte().toInt()
        val maxPlayers = if (afterLogin) input.readVarInt() else 0
        val viewDistance = if (afterLogin) input.readVarInt() else 0
        val simulationDistance = if (afterLogin) input.readVarInt() else 0
        val reducedDebugInfo = if (afterLogin) input.readByte().toInt() else 0
        val respawnScreen = if (afterLogin) input.readByte().toInt() else 0
        val debug = input.readByte().toInt()
        val flat = input.readByte().toInt()
        val keep = if (afterLogin) 0 else input.readByte().toInt()
        val hasDeath = input.readByte().toInt() != 0
        val deathDimension = if (hasDeath) input.readProtocolString() else null
        val deathPosition = if (hasDeath) input.readLong() else 0L
        input.readVarInt() // The portal cooldown, which is what goes.

        encode(packet) { output ->
            head?.let {
                output.writeInt(it.entityId)
                output.writeByte(it.hardcore)
                output.writeByte(it.gamemode)
                output.writeByte(it.previousGamemode)
                output.writeVarInt(it.worlds.size)
                for (world in it.worlds) output.writeProtocolString(world)
                output.writeAnonymousNbt(it.codec)
            }
            output.writeProtocolString(dimensionType)
            output.writeProtocolString(worldName)
            output.writeLong(seed)
            if (afterLogin) {
                output.writeVarInt(maxPlayers)
                output.writeVarInt(viewDistance)
                output.writeVarInt(simulationDistance)
                output.writeByte(reducedDebugInfo.toByte())
                output.writeByte(respawnScreen.toByte())
                output.writeByte(debug.toByte())
                output.writeByte(flat.toByte())
            } else {
                output.writeByte(gamemode.toByte())
                output.writeByte(previousGamemode.toByte())
                output.writeByte(debug.toByte())
                output.writeByte(flat.toByte())
                output.writeByte(keep.toByte())
            }
            output.writeBoolean(deathDimension != null)
            if (deathDimension != null) {
                output.writeProtocolString(deathDimension)
                output.writeLong(deathPosition)
            }
        }
    }

    private class Head(
        val entityId: Int,
        val hardcore: Byte,
        val gamemode: Byte,
        val previousGamemode: Byte,
        val worlds: List<String>,
        val codec: org.basaltmc.wire.nbt.NbtTag,
    )
}
