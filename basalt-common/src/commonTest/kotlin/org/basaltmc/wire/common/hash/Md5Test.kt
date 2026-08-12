package org.basaltmc.wire.common.hash

import kotlin.test.Test
import kotlin.test.assertEquals

class Md5Test {
    private fun ByteArray.toHex(): String =
        joinToString("") { byte ->
            val v = byte.toInt() and 0xFF
            v.toString(16).padStart(2, '0')
        }

    @Test
    fun digestsKnownVectors() {
        // Reference digests cross-checked against Python's hashlib and against the real
        // Mojang RandomSupport.seedFromHashOf output extracted from the decompiled 26.2 jar.
        assertEquals("d41d8cd98f00b204e9800998ecf8427e", Md5.digest("".encodeToByteArray()).toHex())
        assertEquals(
            "41fa841117c127f4f08d9586b63845d5",
            Md5.digest("minecraft:overworld".encodeToByteArray()).toHex(),
        )
        assertEquals("5d41402abc4b2a76b9719d911017c592", Md5.digest("hello".encodeToByteArray()).toHex())
    }

    @Test
    fun digestsLongerInputAcrossBlockBoundary() {
        val input = "The quick brown fox jumps over the lazy dog"
        assertEquals("9e107d9d372bb6826bd81d3542a419d6", Md5.digest(input.encodeToByteArray()).toHex())
    }
}
