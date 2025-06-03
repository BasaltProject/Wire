package org.basaltmc.wire.protocol

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.handshake.ClientIntent
import org.basaltmc.wire.protocol.handshake.HandshakePacket
import org.basaltmc.wire.protocol.login.GameProfile
import org.basaltmc.wire.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.wire.protocol.login.LoginCompressionPacket
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.login.LoginStartPacket
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * A full offline-mode handshake to login exchange over the real wire framing (VarInt length
 * prefix, then a switch to compressed framing partway through), entirely in memory. This is the
 * closest thing to an integration test this module has without real sockets or a Mojang session;
 * the online-mode RSA/AES-CFB8 handshake has its own coverage over real sockets elsewhere.
 */
@OptIn(ExperimentalUuidApi::class)
class OfflineLoginFlowTest {
    /** Stand-in for zlib so this test does not need `basalt-io`'s native-only [org.basaltmc.wire.io.Zlib]. */
    private fun fakeCompress(data: ByteArray): ByteArray = data.reversedArray()

    private fun fakeDecompress(
        data: ByteArray,
        expectedSize: Int,
    ): ByteArray {
        assertEquals(expectedSize, data.size)
        return data.reversedArray()
    }

    @Test
    fun clientAndServerAgreeOnEveryStepOfOfflineLogin() {
        val wire = ByteBuffer() // client -> server
        val backWire = ByteBuffer() // server -> client

        // 1. Client sends Handshake(intent = LOGIN).
        val handshake = HandshakePacket(776, "localhost", 25565, ClientIntent.LOGIN)
        sendUncompressed(wire, HandshakePacket.ID, handshake::write)

        val (handshakeId, handshakeBuffer) = receiveUncompressed(wire)
        assertEquals(HandshakePacket.ID, handshakeId)
        assertEquals(handshake, HandshakePacket.read(handshakeBuffer))

        // 2. Client sends LoginStart.
        val profileId = Uuid.parse("069a79f4-44e9-4726-a5be-fca90e38aaf5")
        val loginStart = LoginStartPacket("Steve", profileId)
        sendUncompressed(wire, LoginStartPacket.ID, loginStart::write)

        val (loginStartId, loginStartBuffer) = receiveUncompressed(wire)
        assertEquals(LoginStartPacket.ID, loginStartId)
        assertEquals(loginStart, LoginStartPacket.read(loginStartBuffer))

        // 3. Server enables compression: everything after this point (including this packet
        //    itself, per vanilla) uses compressed framing.
        // Deliberately small so LoginFinished's ~40-byte body exercises the real-compression
        // branch, and the empty LoginAcknowledged exercises the below-threshold branch.
        val threshold = 20
        sendUncompressed(backWire, LoginCompressionPacket.ID) { LoginCompressionPacket(threshold).write(it) }
        val (compressionId, compressionBuffer) = receiveUncompressed(backWire)
        assertEquals(LoginCompressionPacket.ID, compressionId)
        assertEquals(threshold, LoginCompressionPacket.read(compressionBuffer).threshold)

        // 4. Server sends LoginFinished, now compressed.
        val profile = GameProfile(profileId, "Steve")
        val loginFinished = LoginFinishedPacket(profile, sessionId = Uuid.parse("11111111-2222-3333-4444-555555555555"))
        sendCompressed(backWire, LoginFinishedPacket.ID, threshold, ::fakeCompress, loginFinished::write)

        val (finishedId, finishedBuffer) = receiveCompressed(backWire, ::fakeDecompress)
        assertEquals(LoginFinishedPacket.ID, finishedId)
        assertEquals(loginFinished, LoginFinishedPacket.read(finishedBuffer))

        // 5. Client acknowledges (empty packet), also compressed now.
        sendCompressed(wire, LoginAcknowledgedPacket.ID, threshold, ::fakeCompress) { }
        val (ackId, ackBuffer) = receiveCompressed(wire, ::fakeDecompress)
        assertEquals(LoginAcknowledgedPacket.ID, ackId)
        assertEquals(0, ackBuffer.readableBytes)
    }

    private fun sendUncompressed(
        wire: ByteBuffer,
        id: Int,
        writeBody: (ByteBuffer) -> Unit = {},
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        PacketFraming.writeUncompressed(wire, body.toByteArray())
    }

    private fun sendCompressed(
        wire: ByteBuffer,
        id: Int,
        threshold: Int,
        compress: (ByteArray) -> ByteArray,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        PacketFraming.writeCompressed(wire, body.toByteArray(), threshold, compress)
    }

    private fun receiveUncompressed(wire: ByteBuffer): Pair<Int, ByteBuffer> {
        val frame =
            PacketFraming.tryReadFrame(wire, compressionEnabled = false)
                ?: error("expected a full frame to be buffered")
        val body = ByteBuffer(frame.size).also { it.writeBytes(frame) }
        return body.readVarInt() to body
    }

    private fun receiveCompressed(
        wire: ByteBuffer,
        decompress: (ByteArray, Int) -> ByteArray,
    ): Pair<Int, ByteBuffer> {
        val frame =
            PacketFraming.tryReadFrame(wire, compressionEnabled = true, decompress = decompress)
                ?: error("expected a full frame to be buffered")
        val body = ByteBuffer(frame.size).also { it.writeBytes(frame) }
        return body.readVarInt() to body
    }
}
