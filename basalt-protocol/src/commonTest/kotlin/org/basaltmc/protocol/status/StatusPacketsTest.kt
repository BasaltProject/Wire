package org.basaltmc.protocol.status

import org.basaltmc.io.ByteBuffer
import org.basaltmc.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals

class StatusPacketsTest {
    @Test
    fun roundTripsStatusResponse() {
        val status =
            ServerStatusPayload(
                description = Component.text("A Basalt Server"),
                players =
                    ServerStatusPayload.Players(
                        max = 20,
                        online = 3,
                        sample =
                            listOf(
                                ServerStatusPayload.SamplePlayer("Steve", "069a79f4-44e9-4726-a5be-fca90e38aaf5"),
                            ),
                    ),
                version = ServerStatusPayload.Version("26.2", 776),
                favicon = byteArrayOf(1, 2, 3, 4),
                enforcesSecureChat = true,
            )
        val buffer = ByteBuffer()
        StatusResponsePacket(status).write(buffer)
        val decoded = StatusResponsePacket.read(buffer).status

        assertEquals(status.description, decoded.description)
        assertEquals(status.players, decoded.players)
        assertEquals(status.version, decoded.version)
        assertEquals(status.favicon?.toList(), decoded.favicon?.toList())
        assertEquals(status.enforcesSecureChat, decoded.enforcesSecureChat)
    }

    @Test
    fun statusResponseWithoutOptionalFields() {
        val status = ServerStatusPayload(description = Component.text("minimal"))
        val buffer = ByteBuffer()
        StatusResponsePacket(status).write(buffer)
        val decoded = StatusResponsePacket.read(buffer).status

        assertEquals(status.description, decoded.description)
        assertEquals(null, decoded.players)
        assertEquals(null, decoded.version)
        assertEquals(null, decoded.favicon)
        assertEquals(false, decoded.enforcesSecureChat)
    }

    @Test
    fun roundTripsPingPong() {
        val bufferPing = ByteBuffer()
        PingRequestPacket(1234567890123L).write(bufferPing)
        assertEquals(1234567890123L, PingRequestPacket.read(bufferPing).time)

        val bufferPong = ByteBuffer()
        PongResponsePacket(1234567890123L).write(bufferPong)
        assertEquals(1234567890123L, PongResponsePacket.read(bufferPong).time)
    }
}
