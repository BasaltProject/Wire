package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals

class SuggestionPacketsTest {
    @Test
    fun requestRoundTrips() {
        val buffer = ByteBuffer()
        val packet = CommandSuggestionRequestPacket(id = 7, command = "/place structure minecraft:vill")
        packet.write(buffer)
        assertEquals(packet, CommandSuggestionRequestPacket.read(buffer))
    }

    @Test
    fun suggestionsRoundTripWithAndWithoutTooltips() {
        val buffer = ByteBuffer()
        val packet =
            CommandSuggestionsPacket(
                id = 7,
                start = 17,
                length = 14,
                suggestions =
                    listOf(
                        CommandSuggestion("minecraft:village_plains"),
                        CommandSuggestion("minecraft:village_desert", Component.text("A desert village")),
                    ),
            )
        packet.write(buffer)
        assertEquals(packet, CommandSuggestionsPacket.read(buffer))
    }

    @Test
    fun emptySuggestionsRoundTrip() {
        val buffer = ByteBuffer()
        val packet = CommandSuggestionsPacket(id = 0, start = 1, length = 0, suggestions = emptyList())
        packet.write(buffer)
        assertEquals(packet, CommandSuggestionsPacket.read(buffer))
    }
}
