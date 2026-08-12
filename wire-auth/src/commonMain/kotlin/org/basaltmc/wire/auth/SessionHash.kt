package org.basaltmc.wire.auth

/**
 * The server hash Mojang's session server expects, formatted as vanilla's
 * `new BigInteger(digest).toString(16)`: big-endian bytes read as a signed two's-complement
 * integer, lowercase hex, no leading zeros, `-` when negative.
 *
 * // parity: ServerLoginPacketListenerImpl.handleKey. Checked against real `BigInteger` output for
 * both signs and the leading-zero byte and nibble cases (see [SessionHashTest]).
 */
public object SessionHash {
    public fun format(digest: ByteArray): String {
        require(digest.isNotEmpty()) { "digest must not be empty" }
        val negative = (digest[0].toInt() and 0x80) != 0
        val magnitude = if (negative) twosComplementNegate(digest) else digest
        val hex = toHexWithoutLeadingZeros(magnitude)
        return if (negative) "-$hex" else hex
    }

    private fun twosComplementNegate(bytes: ByteArray): ByteArray {
        val result = ByteArray(bytes.size)
        var carry = 1
        for (i in bytes.indices.reversed()) {
            val inverted = (bytes[i].toInt() xor 0xFF) and 0xFF
            val sum = inverted + carry
            result[i] = sum.toByte()
            carry = sum ushr 8
        }
        return result
    }

    private fun toHexWithoutLeadingZeros(bytes: ByteArray): String {
        val hex = StringBuilder()
        var leading = true
        for (byte in bytes) {
            val value = byte.toInt() and 0xFF
            if (leading && value == 0) continue
            hex.append(if (leading) value.toString(16) else value.toString(16).padStart(2, '0'))
            leading = false
        }
        return if (hex.isEmpty()) "0" else hex.toString()
    }
}
