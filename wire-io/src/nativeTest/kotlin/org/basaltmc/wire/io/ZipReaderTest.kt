package org.basaltmc.wire.io

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ZipReaderTest {
    // A real ZIP built with Python's `zipfile` module: `hello.txt` (deflate-compressed) and
    // `stored.txt` (stored, no compression), covering both compression methods a real jar uses.
    private val zipBytes = ZIP_FIXTURE_BASE64.decodeBase64()

    @Test
    fun listsEntryNames() {
        val zip = ZipReader(zipBytes)
        assertEquals(setOf("hello.txt", "stored.txt"), zip.entryNames)
    }

    @Test
    fun readsADeflateCompressedEntry() {
        val zip = ZipReader(zipBytes)
        val content = zip.readEntry("hello.txt")
        assertTrue(content != null)
        assertEquals("Hello, Basalt ZIP test! ".repeat(3), content.decodeToString())
    }

    @Test
    fun readsAStoredEntry() {
        val zip = ZipReader(zipBytes)
        val content = zip.readEntry("stored.txt")
        assertTrue(content != null)
        assertEquals("stored, no compression", content.decodeToString())
    }

    @Test
    fun returnsNullForAMissingEntry() {
        val zip = ZipReader(zipBytes)
        assertNull(zip.readEntry("does_not_exist.txt"))
    }
}

// Base64 of a real ZIP (built with Python's `zipfile` module) rather than a raw byte literal:
// short enough to read as one string, and avoids fighting ktlint's argument-wrapping rules over a
// 263-element numeric array.
private const val ZIP_FIXTURE_BASE64 =
    "UEsDBBQAAAAIAA5kB12g9joWHQAAAEgAAAAJAAAAaGVsbG8udHh080jNycnXUXBKLE7MKVGI8gxQKEktLlFU8CBRHABQSwMEFAAAAAAADmQH" +
        "Xb6mq1EWAAAAFgAAAAoAAABzdG9yZWQudHh0c3RvcmVkLCBubyBjb21wcmVzc2lvblBLAQIUAxQAAAAIAA5kB12g9joWHQAAAEgAAA" +
        "AJAAAAAAAAAAAAAACAAQAAAABoZWxsby50eHRQSwECFAMUAAAAAAAOZAddvqarURYAAAAWAAAACgAAAAAAAAAAAAAAgAFEAAAAc3Rv" +
        "cmVkLnR4dFBLBQYAAAAAAgACAG8AAACCAAAAAAA="

private fun String.decodeBase64(): ByteArray {
    val table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    val clean = trimEnd('=')
    val output = ArrayList<Byte>((clean.length * 3) / 4 + 3)
    var buffer = 0
    var bitsCollected = 0
    for (char in clean) {
        val value = table.indexOf(char)
        require(value >= 0) { "invalid base64 character: $char" }
        buffer = (buffer shl 6) or value
        bitsCollected += 6
        if (bitsCollected >= 8) {
            bitsCollected -= 8
            output.add(((buffer shr bitsCollected) and 0xFF).toByte())
        }
    }
    return output.toByteArray()
}
