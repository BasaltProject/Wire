package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.PlayDisconnectPacket
import org.basaltmc.wire.protocol.play.SetActionBarTextPacket
import org.basaltmc.wire.protocol.play.SetSubtitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitleTextPacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.play.TabListPacket
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentJson

/**
 * 1.20.3 to 1.20.2, where text goes back to being a string of json.
 *
 * 1.20.3 started sending components as NBT. Below it every one of them is a string, so each packet
 * that carries text is written again with the same component serialized the old way.
 *
 * The ones covered are the ones this server sends: chat, the three title lines, the tab list, the
 * title of a screen and the reason on a kick. A boss bar or a scoreboard is not renamed here, and
 * that is why neither reaches a client this old at all rather than reaching it broken.
 */
internal class Link764(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            SystemChatPacket.ID -> {
                val chat = decode(packet) { SystemChatPacket.read(it) }
                encode(packet) { output ->
                    writeJson(output, chat.content)
                    output.writeBoolean(chat.isActionBar)
                }
            }
            SetTitleTextPacket.ID -> {
                val title = decode(packet) { SetTitleTextPacket.read(it) }
                encode(packet) { writeJson(it, title.text) }
            }
            SetSubtitleTextPacket.ID -> {
                val subtitle = decode(packet) { SetSubtitleTextPacket.read(it) }
                encode(packet) { writeJson(it, subtitle.text) }
            }
            SetActionBarTextPacket.ID -> {
                val bar = decode(packet) { SetActionBarTextPacket.read(it) }
                encode(packet) { writeJson(it, bar.text) }
            }
            PlayDisconnectPacket.ID -> {
                val disconnect = decode(packet) { PlayDisconnectPacket.read(it) }
                encode(packet) { writeJson(it, disconnect.reason) }
            }
            TabListPacket.ID -> {
                val list = decode(packet) { TabListPacket.read(it) }
                encode(packet) { output ->
                    writeJson(output, list.header)
                    writeJson(output, list.footer)
                }
            }
            OpenScreenPacket.ID -> {
                val screen = decode(packet) { OpenScreenPacket.read(it) }
                encode(packet) { output ->
                    output.writeVarInt(screen.containerId)
                    output.writeVarInt(screen.menuType)
                    writeJson(output, screen.title)
                }
            }
            in TEXT_PACKETS_NOT_TRANSLATED -> return false
        }
        return true
    }

    private fun writeJson(
        output: ByteBuffer,
        component: Component,
    ) {
        output.writeProtocolString(ComponentJson.serialize(component).toString())
    }

    private companion object {
        /**
         * Packets whose text sits inside a shape of its own, which this link does not unpick.
         *
         * A boss bar, an objective and a team all carry a component in the middle of a structure
         * that changes with a mode byte. Sending them with the text still as NBT would leave the
         * client reading a length where a tag is, so they are left out until they are done
         * properly: the bar and the sidebar do not appear, and nothing else breaks.
         */
        val TEXT_PACKETS_NOT_TRANSLATED =
            setOf(
                org.basaltmc.wire.protocol.play.BossEventPacket.ID,
                org.basaltmc.wire.protocol.play.SetObjectivePacket.ID,
                org.basaltmc.wire.protocol.play.SetPlayerTeamPacket.ID,
            )
    }
}
