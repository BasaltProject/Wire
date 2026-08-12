package org.basaltmc.wire.protocol

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.Zlib
import kotlin.test.Test
import kotlin.test.assertEquals

/** Same as [PacketFramingTest] but wired to the real `basalt-io` zlib binding, not a stub. */
class PacketFramingZlibTest {
    @Test
    fun roundTripsThroughRealZlib() {
        val payload = "The quick brown fox jumps over the lazy dog".repeat(50).encodeToByteArray()
        val wire = ByteBuffer()
        PacketFraming.writeCompressed(wire, payload, threshold = 32, compress = Zlib::compress)

        val frame = PacketFraming.tryReadFrame(wire, compressionEnabled = true, decompress = Zlib::decompress)
        assertEquals(payload.toList(), frame?.toList())
    }
}
