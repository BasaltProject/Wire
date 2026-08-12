package org.basaltmc.wire.protocol

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class PacketFramingTest {
    @Test
    fun uncompressedRoundTrip() {
        val payload = byteArrayOf(0x05, 1, 2, 3, 4)
        val wire = ByteBuffer()
        PacketFraming.writeUncompressed(wire, payload)

        val frame = PacketFraming.tryReadFrame(wire, compressionEnabled = false)
        assertEquals(payload.toList(), frame?.toList())
        assertEquals(0, wire.readableBytes)
    }

    @Test
    fun returnsNullWhenFrameIsIncomplete() {
        val payload = ByteArray(50) { it.toByte() }
        val wire = ByteBuffer()
        PacketFraming.writeUncompressed(wire, payload)

        // Simulate a partial socket read: only the first few bytes have arrived so far.
        val partial = ByteBuffer()
        partial.writeBytes(wire.toByteArray(), length = 3)

        assertNull(PacketFraming.tryReadFrame(partial, compressionEnabled = false))
    }

    @Test
    fun compressedRoundTripBelowThresholdStaysUncompressed() {
        val payload = byteArrayOf(1, 2, 3)
        val wire = ByteBuffer()
        var compressCalls = 0
        PacketFraming.writeCompressed(wire, payload, threshold = 256, compress = {
            compressCalls++
            it
        })

        val frame = PacketFraming.tryReadFrame(wire, compressionEnabled = true, decompress = { bytes, _ -> bytes })
        assertEquals(payload.toList(), frame?.toList())
        assertEquals(0, compressCalls, "payload under threshold must not be compressed")
    }

    @Test
    fun compressedRoundTripAboveThresholdCallsCodec() {
        val payload = ByteArray(1000) { (it % 200).toByte() }
        val wire = ByteBuffer()
        val fakeCompressed = byteArrayOf(9, 9, 9)
        PacketFraming.writeCompressed(wire, payload, threshold = 10, compress = { fakeCompressed })

        val frame =
            PacketFraming.tryReadFrame(
                wire,
                compressionEnabled = true,
                decompress = { bytes, size ->
                    assertEquals(fakeCompressed.toList(), bytes.toList())
                    assertEquals(payload.size, size)
                    payload
                },
            )
        assertEquals(payload.toList(), frame?.toList())
    }

    @Test
    fun handlesMultipleFramesBackToBack() {
        val wire = ByteBuffer()
        PacketFraming.writeUncompressed(wire, byteArrayOf(1))
        PacketFraming.writeUncompressed(wire, byteArrayOf(2, 2))
        PacketFraming.writeUncompressed(wire, byteArrayOf(3, 3, 3))

        assertEquals(listOf<Byte>(1), PacketFraming.tryReadFrame(wire, false)?.toList())
        assertEquals(listOf<Byte>(2, 2), PacketFraming.tryReadFrame(wire, false)?.toList())
        assertEquals(listOf<Byte>(3, 3, 3), PacketFraming.tryReadFrame(wire, false)?.toList())
        assertNull(PacketFraming.tryReadFrame(wire, false))
    }
}
