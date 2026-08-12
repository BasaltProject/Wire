package org.basaltmc.wire.common.hash

/**
 * RFC 1321 MD5. Not cryptographically safe by modern standards, but required to reproduce
 * vanilla's `RandomSupport.seedFromHashOf`, which hashes structure/feature names through Guava's
 * MD5 implementation to derive a 128-bit seed.
 */
public object Md5 {
    private val shiftAmounts =
        intArrayOf(
            7,
            12,
            17,
            22,
            7,
            12,
            17,
            22,
            7,
            12,
            17,
            22,
            7,
            12,
            17,
            22,
            5,
            9,
            14,
            20,
            5,
            9,
            14,
            20,
            5,
            9,
            14,
            20,
            5,
            9,
            14,
            20,
            4,
            11,
            16,
            23,
            4,
            11,
            16,
            23,
            4,
            11,
            16,
            23,
            4,
            11,
            16,
            23,
            6,
            10,
            15,
            21,
            6,
            10,
            15,
            21,
            6,
            10,
            15,
            21,
            6,
            10,
            15,
            21,
        )

    private val constants =
        uintArrayOf(
            0xd76aa478u,
            0xe8c7b756u,
            0x242070dbu,
            0xc1bdceeeu,
            0xf57c0fafu,
            0x4787c62au,
            0xa8304613u,
            0xfd469501u,
            0x698098d8u,
            0x8b44f7afu,
            0xffff5bb1u,
            0x895cd7beu,
            0x6b901122u,
            0xfd987193u,
            0xa679438eu,
            0x49b40821u,
            0xf61e2562u,
            0xc040b340u,
            0x265e5a51u,
            0xe9b6c7aau,
            0xd62f105du,
            0x02441453u,
            0xd8a1e681u,
            0xe7d3fbc8u,
            0x21e1cde6u,
            0xc33707d6u,
            0xf4d50d87u,
            0x455a14edu,
            0xa9e3e905u,
            0xfcefa3f8u,
            0x676f02d9u,
            0x8d2a4c8au,
            0xfffa3942u,
            0x8771f681u,
            0x6d9d6122u,
            0xfde5380cu,
            0xa4beea44u,
            0x4bdecfa9u,
            0xf6bb4b60u,
            0xbebfbc70u,
            0x289b7ec6u,
            0xeaa127fau,
            0xd4ef3085u,
            0x04881d05u,
            0xd9d4d039u,
            0xe6db99e5u,
            0x1fa27cf8u,
            0xc4ac5665u,
            0xf4292244u,
            0x432aff97u,
            0xab9423a7u,
            0xfc93a039u,
            0x655b59c3u,
            0x8f0ccc92u,
            0xffeff47du,
            0x85845dd1u,
            0x6fa87e4fu,
            0xfe2ce6e0u,
            0xa3014314u,
            0x4e0811a1u,
            0xf7537e82u,
            0xbd3af235u,
            0x2ad7d2bbu,
            0xeb86d391u,
        )

    /** Returns the 16-byte MD5 digest of [input]. */
    public fun digest(input: ByteArray): ByteArray {
        val messageLengthBits = input.size.toLong() * 8L
        val paddedLength =
            run {
                var length = input.size + 1
                while (length % 64 != 56) length++
                length + 8
            }
        val padded = ByteArray(paddedLength)
        input.copyInto(padded)
        padded[input.size] = 0x80.toByte()
        for (i in 0 until 8) {
            padded[paddedLength - 8 + i] = ((messageLengthBits ushr (8 * i)) and 0xFF).toByte()
        }

        var a0 = 0x67452301u
        var b0 = 0xefcdab89u
        var c0 = 0x98badcfeu
        var d0 = 0x10325476u

        val block = UIntArray(16)
        var offset = 0
        while (offset < padded.size) {
            for (i in 0 until 16) {
                val base = offset + i * 4
                block[i] = (padded[base].toUInt() and 0xFFu) or
                    ((padded[base + 1].toUInt() and 0xFFu) shl 8) or
                    ((padded[base + 2].toUInt() and 0xFFu) shl 16) or
                    ((padded[base + 3].toUInt() and 0xFFu) shl 24)
            }

            var a = a0
            var b = b0
            var c = c0
            var d = d0

            for (i in 0 until 64) {
                val f: UInt
                val g: Int
                when {
                    i < 16 -> {
                        f = (b and c) or (b.inv() and d)
                        g = i
                    }
                    i < 32 -> {
                        f = (d and b) or (d.inv() and c)
                        g = (5 * i + 1) % 16
                    }
                    i < 48 -> {
                        f = b xor c xor d
                        g = (3 * i + 5) % 16
                    }
                    else -> {
                        f = c xor (b or d.inv())
                        g = (7 * i) % 16
                    }
                }
                val temp = d
                d = c
                c = b
                val sum = a + f + constants[i] + block[g]
                b = b + sum.rotateLeft(shiftAmounts[i])
                a = temp
            }

            a0 += a
            b0 += b
            c0 += c
            d0 += d
            offset += 64
        }

        val result = ByteArray(16)
        writeLittleEndian(result, 0, a0)
        writeLittleEndian(result, 4, b0)
        writeLittleEndian(result, 8, c0)
        writeLittleEndian(result, 12, d0)
        return result
    }

    private fun writeLittleEndian(
        target: ByteArray,
        offset: Int,
        value: UInt,
    ) {
        target[offset] = (value and 0xFFu).toByte()
        target[offset + 1] = ((value shr 8) and 0xFFu).toByte()
        target[offset + 2] = ((value shr 16) and 0xFFu).toByte()
        target[offset + 3] = ((value shr 24) and 0xFFu).toByte()
    }
}
