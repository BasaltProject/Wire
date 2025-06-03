package org.basaltmc.wire.nbt

/**
 * NBT tag names and string payloads are encoded with Java's "modified UTF-8"
 * (`java.io.DataOutput.writeUTF` / `DataInput.readUTF`), not plain UTF-8: the null character
 * is encoded as two bytes instead of one, and supplementary characters are encoded as two
 * separate 3-byte sequences (one per UTF-16 surrogate half) instead of one 4-byte sequence.
 * Reading real Mojang-written NBT (player data, chunks, structures) with plain UTF-8 would
 * silently corrupt any string containing such characters, so this is implemented explicitly
 * rather than reusing `String.encodeToByteArray()`.
 */
internal object ModifiedUtf8 {
    fun encodedLength(value: String): Int {
        var length = 0
        for (char in value) {
            val code = char.code
            length +=
                when {
                    code in 0x0001..0x007F -> 1
                    code <= 0x07FF -> 2
                    else -> 3
                }
        }
        return length
    }

    fun encode(
        value: String,
        sink: ByteWriter,
    ) {
        for (char in value) {
            val code = char.code
            when {
                code in 0x0001..0x007F -> sink.writeByte(code.toByte())
                code <= 0x07FF -> {
                    sink.writeByte((0xC0 or (code shr 6)).toByte())
                    sink.writeByte((0x80 or (code and 0x3F)).toByte())
                }
                else -> {
                    sink.writeByte((0xE0 or (code shr 12)).toByte())
                    sink.writeByte((0x80 or ((code shr 6) and 0x3F)).toByte())
                    sink.writeByte((0x80 or (code and 0x3F)).toByte())
                }
            }
        }
    }

    fun decode(
        source: ByteReader,
        byteLength: Int,
    ): String {
        val builder = StringBuilder(byteLength)
        var remaining = byteLength
        while (remaining > 0) {
            val a = source.readByte().toInt() and 0xFF
            remaining -= 1
            when {
                a and 0x80 == 0 -> builder.append(a.toChar())
                a and 0xE0 == 0xC0 -> {
                    require(remaining >= 1) { "Truncated modified UTF-8 sequence" }
                    val b = source.readByte().toInt() and 0xFF
                    remaining -= 1
                    builder.append((((a and 0x1F) shl 6) or (b and 0x3F)).toChar())
                }
                a and 0xF0 == 0xE0 -> {
                    require(remaining >= 2) { "Truncated modified UTF-8 sequence" }
                    val b = source.readByte().toInt() and 0xFF
                    val c = source.readByte().toInt() and 0xFF
                    remaining -= 2
                    builder.append((((a and 0x0F) shl 12) or ((b and 0x3F) shl 6) or (c and 0x3F)).toChar())
                }
                else -> throw NbtFormatException("Invalid modified UTF-8 leading byte: $a")
            }
        }
        return builder.toString()
    }
}
