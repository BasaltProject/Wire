@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.usePinned
import kotlinx.cinterop.value
import platform.zlib.Z_BUF_ERROR
import platform.zlib.Z_OK
import platform.zlib.compress2
import platform.zlib.compressBound
import platform.zlib.uLongVar
import platform.zlib.uncompress

/** Thrown when zlib reports the destination buffer was too small; the caller can retry with more room. */
public class ZlibBufferTooSmallException : Exception("zlib destination buffer too small")

/** Thrown for genuine zlib failures (corrupt or truncated data) that a bigger buffer will not fix. */
public class ZlibException(
    public val code: Int,
) : Exception("zlib failed with code $code")

/**
 * zlib-wrapped (RFC 1950) deflate/inflate, bound to Kotlin/Native's bundled `platform.zlib`
 * cinterop (no custom cinterop definition needed: it ships with the compiler). This is the
 * compression format the play protocol uses for packet bodies; gzip (RFC 1952), used for saved
 * world files, is a separate wrapper layered on top when world persistence needs it.
 */
public object Zlib {
    public fun compress(
        data: ByteArray,
        level: Int = 6,
    ): ByteArray {
        if (data.isEmpty()) return ByteArray(0)
        val bound = compressBound(data.size.convert())
        val dest = UByteArray(bound.toInt())
        val source = data.toUByteArray()
        return memScoped {
            val destLen = alloc(bound)
            val result =
                source.usePinned { srcPinned ->
                    dest.usePinned { destPinned ->
                        compress2(
                            destPinned.addressOf(0),
                            destLen.ptr,
                            srcPinned.addressOf(0),
                            data.size.convert(),
                            level,
                        )
                    }
                }
            if (result != Z_OK) throw ZlibException(result)
            dest.copyOf(destLen.value.toInt()).toByteArray()
        }
    }

    /** Decompresses [data], given the exact uncompressed size (as the play protocol declares). */
    public fun decompress(
        data: ByteArray,
        uncompressedSize: Int,
    ): ByteArray {
        if (uncompressedSize == 0) return ByteArray(0)
        require(data.isNotEmpty()) { "Cannot decompress empty input into $uncompressedSize bytes" }
        val dest = UByteArray(uncompressedSize)
        val source = data.toUByteArray()
        return memScoped {
            val destLen = alloc<uLongVar>()
            destLen.value = uncompressedSize.convert()
            val result =
                source.usePinned { srcPinned ->
                    dest.usePinned { destPinned ->
                        uncompress(destPinned.addressOf(0), destLen.ptr, srcPinned.addressOf(0), data.size.convert())
                    }
                }
            when (result) {
                Z_OK -> dest.copyOf(destLen.value.toInt()).toByteArray()
                Z_BUF_ERROR -> throw ZlibBufferTooSmallException()
                else -> throw ZlibException(result)
            }
        }
    }

    /** Decompresses [data] without knowing the output size ahead of time, growing a scratch buffer. */
    public fun decompressUnknownSize(
        data: ByteArray,
        initialCapacity: Int = data.size * 4 + 64,
    ): ByteArray {
        var capacity = initialCapacity.coerceAtLeast(64)
        while (true) {
            try {
                return decompress(data, capacity)
            } catch (_: ZlibBufferTooSmallException) {
                capacity *= 2
                if (capacity > MAX_DECOMPRESSED_SIZE) {
                    throw ZlibException(Z_BUF_ERROR)
                }
            }
        }
    }

    private const val MAX_DECOMPRESSED_SIZE = 512 * 1024 * 1024
}
