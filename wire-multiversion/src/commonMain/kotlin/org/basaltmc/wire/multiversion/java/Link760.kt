package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.19.3 to 1.19.1 and 1.19.2, where the tab list is one packet with a mode rather than two.
 *
 * 1.19.3 split adding, changing and removing a player into their own packets and let one carry
 * several changes at once. Before that there is a single `player_info` with an action at the front,
 * and each action has its own shape, so what this server sends has to be sorted into them.
 */
@OptIn(kotlin.uuid.ExperimentalUuidApi::class)
internal class Link760(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            PlayerInfoUpdatePacket.ID -> {
                val info = decode(packet) { PlayerInfoUpdatePacket.read(it) }
                packet.overrideId(PLAYER_INFO_760)
                return writeOneAction(packet, info)
            }
            PlayerInfoRemovePacket.ID -> {
                val remove = decode(packet) { PlayerInfoRemovePacket.read(it) }
                packet.overrideId(PLAYER_INFO_760)
                encode(packet) { output ->
                    output.writeVarInt(REMOVE_PLAYER)
                    output.writeVarInt(remove.players.size)
                    for (uuid in remove.players) writeUuid(output, uuid)
                }
            }
        }
        return true
    }

    /**
     * Writes the one action a 1.19.1 client can read out of what this server sent.
     *
     * Adding a player carries the game mode and the ping with it, so a join is one packet. Anything
     * else this server sends on its own is a single action too, and anything with no action that
     * version knows is dropped rather than sent as a mode it would misread.
     */
    private fun writeOneAction(
        packet: MutablePacket,
        info: PlayerInfoUpdatePacket,
    ): Boolean {
        val actions = info.actions
        return when {
            PlayerInfoUpdatePacket.Action.ADD_PLAYER in actions -> {
                encode(packet) { output ->
                    output.writeVarInt(ADD_PLAYER)
                    output.writeVarInt(info.entries.size)
                    for (entry in info.entries) {
                        writeUuid(output, entry.uuid)
                        output.writeProtocolString(entry.name, 16)
                        output.writeVarInt(entry.properties.size)
                        for (property in entry.properties) {
                            output.writeProtocolString(property.name)
                            output.writeProtocolString(property.value)
                            output.writeBoolean(property.signature != null)
                            property.signature?.let { output.writeProtocolString(it) }
                        }
                        output.writeVarInt(entry.gamemode.ordinal)
                        output.writeVarInt(entry.latencyMillis)
                        output.writeBoolean(false)
                        // No signed chat session either: a client this old asks for a public key
                        // it will not get from this server.
                        output.writeBoolean(false)
                    }
                }
                true
            }
            PlayerInfoUpdatePacket.Action.UPDATE_GAME_MODE in actions -> {
                encode(packet) { output ->
                    output.writeVarInt(UPDATE_GAME_MODE)
                    output.writeVarInt(info.entries.size)
                    for (entry in info.entries) {
                        writeUuid(output, entry.uuid)
                        output.writeVarInt(entry.gamemode.ordinal)
                    }
                }
                true
            }
            PlayerInfoUpdatePacket.Action.UPDATE_LATENCY in actions -> {
                encode(packet) { output ->
                    output.writeVarInt(UPDATE_LATENCY)
                    output.writeVarInt(info.entries.size)
                    for (entry in info.entries) {
                        writeUuid(output, entry.uuid)
                        output.writeVarInt(entry.latencyMillis)
                    }
                }
                true
            }
            else -> false
        }
    }

    private fun writeUuid(
        output: ByteBuffer,
        uuid: kotlin.uuid.Uuid,
    ) {
        output.writeLong(uuid.toLongs { most, _ -> most })
        output.writeLong(uuid.toLongs { _, least -> least })
    }

    private companion object {
        const val PLAYER_INFO_760 = 55

        /** parity: `ClientboundPlayerInfoPacket.Action`, in the order 1.19.1 numbers them. */
        const val ADD_PLAYER = 0
        const val UPDATE_GAME_MODE = 1
        const val UPDATE_LATENCY = 2
        const val REMOVE_PLAYER = 4
    }
}
