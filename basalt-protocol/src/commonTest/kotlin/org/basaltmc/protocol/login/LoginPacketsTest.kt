package org.basaltmc.protocol.login

import org.basaltmc.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

@OptIn(ExperimentalUuidApi::class)
class LoginPacketsTest {
    @Test
    fun roundTripsLoginStart() {
        val packet = LoginStartPacket("Steve", Uuid.parse("069a79f4-44e9-4726-a5be-fca90e38aaf5"))
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, LoginStartPacket.read(buffer))
    }

    @Test
    fun roundTripsHello() {
        val packet =
            HelloPacket(
                serverId = "",
                publicKey = ByteArray(162) { it.toByte() },
                challenge = byteArrayOf(1, 2, 3, 4),
                shouldAuthenticate = true,
            )
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, HelloPacket.read(buffer))
    }

    @Test
    fun roundTripsKey() {
        val packet =
            KeyPacket(
                encryptedSecret = ByteArray(128) { (it * 3).toByte() },
                encryptedChallenge = ByteArray(128) { (it * 5).toByte() },
            )
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, KeyPacket.read(buffer))
    }

    @Test
    fun roundTripsLoginCompression() {
        val buffer = ByteBuffer()
        LoginCompressionPacket(256).write(buffer)
        assertEquals(256, LoginCompressionPacket.read(buffer).threshold)
    }

    @Test
    fun roundTripsLoginFinishedWithProperties() {
        val profile =
            GameProfile(
                id = Uuid.parse("069a79f4-44e9-4726-a5be-fca90e38aaf5"),
                name = "Steve",
                properties =
                    listOf(
                        GameProfileProperty("textures", "base64data", signature = "sig"),
                        GameProfileProperty("unsigned", "value"),
                    ),
            )
        val packet = LoginFinishedPacket(profile, sessionId = Uuid.parse("11111111-2222-3333-4444-555555555555"))
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, LoginFinishedPacket.read(buffer))
    }

    @Test
    fun roundTripsLoginFinishedWithNoProperties() {
        val profile = GameProfile(Uuid.parse("069a79f4-44e9-4726-a5be-fca90e38aaf5"), "Steve")
        val packet = LoginFinishedPacket(profile, Uuid.parse("00000000-0000-0000-0000-000000000000"))
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, LoginFinishedPacket.read(buffer))
    }
}
