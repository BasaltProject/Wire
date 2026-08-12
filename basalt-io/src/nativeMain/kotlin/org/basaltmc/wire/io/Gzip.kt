@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.usePinned
import platform.zlib.ZLIB_VERSION
import platform.zlib.Z_DEFAULT_STRATEGY
import platform.zlib.Z_DEFLATED
import platform.zlib.Z_FINISH
import platform.zlib.Z_NO_FLUSH
import platform.zlib.Z_OK
import platform.zlib.Z_STREAM_END
import platform.zlib.deflate
import platform.zlib.deflateEnd
import platform.zlib.deflateInit2_
import platform.zlib.inflate
import platform.zlib.inflateEnd
import platform.zlib.inflateInit2_
import platform.zlib.z_stream

/**
 * gzip (RFC 1952), the wrapper saved `.nbt` structure files use and rotated log files are archived
 * with, distinct from [Zlib]'s RFC 1950 and [RawInflate]'s bare RFC 1951. `windowBits = 31`
 * (`15 + 16`) tells zlib to write or expect a real gzip header itself, the same
 * `inflateInit2_`/`deflateInit2_` macro-expansion trick [RawInflate] already uses, rather than this
 * project handling the gzip header/trailer by hand.
 *
 * On the way in, the uncompressed size doesn't need to be known ahead of time: RFC 1952's own
 * trailer stores it (`ISIZE`, the last 4 bytes, little-endian, mod 2^32), which is read directly
 * instead of asking the caller for it.
 */
public object Gzip {
    private const val GZIP_WINDOW_BITS = 31
    private const val DEFAULT_MEM_LEVEL = 8

    /**
     * Produces a real gzip stream, header and trailer included, so the result opens with any
     * ordinary `gzip`/`zcat`. Same `windowBits = 31` trick as [decompress], just on the deflate
     * side, rather than writing the RFC 1952 framing by hand.
     */
    public fun compress(
        data: ByteArray,
        level: Int = 6,
    ): ByteArray {
        val source = data.toUByteArray()
        // deflateBound plus the gzip header and trailer, with room to spare: this runs once per
        // log rotation, so a slightly generous buffer costs nothing worth measuring.
        val dest = UByteArray(data.size + (data.size / 2) + 128)
        return memScoped {
            val stream = alloc<z_stream>()
            val initResult =
                deflateInit2_(
                    stream.ptr,
                    level,
                    Z_DEFLATED,
                    GZIP_WINDOW_BITS,
                    DEFAULT_MEM_LEVEL,
                    Z_DEFAULT_STRATEGY,
                    ZLIB_VERSION,
                    sizeOf<z_stream>().convert(),
                )
            if (initResult != Z_OK) throw ZlibException(initResult)

            try {
                val result =
                    source.usePinned { srcPinned ->
                        dest.usePinned { destPinned ->
                            stream.next_in = if (data.isEmpty()) null else srcPinned.addressOf(0)
                            stream.avail_in = data.size.convert()
                            stream.next_out = destPinned.addressOf(0)
                            stream.avail_out = dest.size.convert()
                            deflate(stream.ptr, Z_FINISH)
                        }
                    }
                if (result != Z_STREAM_END) throw ZlibException(result)
                dest.copyOf(dest.size - stream.avail_out.toInt()).toByteArray()
            } finally {
                deflateEnd(stream.ptr)
            }
        }
    }

    public fun decompress(data: ByteArray): ByteArray {
        require(data.size >= 18) { "Input too small to be a valid gzip stream (${data.size} bytes)" }
        val trailerOffset = data.size - 4
        val uncompressedSize =
            (data[trailerOffset].toInt() and 0xFF) or
                ((data[trailerOffset + 1].toInt() and 0xFF) shl 8) or
                ((data[trailerOffset + 2].toInt() and 0xFF) shl 16) or
                ((data[trailerOffset + 3].toInt() and 0xFF) shl 24)
        if (uncompressedSize == 0) return ByteArray(0)

        val dest = UByteArray(uncompressedSize)
        val source = data.toUByteArray()
        return memScoped {
            val stream = alloc<z_stream>()
            val initResult = inflateInit2_(stream.ptr, GZIP_WINDOW_BITS, ZLIB_VERSION, sizeOf<z_stream>().convert())
            if (initResult != Z_OK) throw ZlibException(initResult)

            try {
                source.usePinned { srcPinned ->
                    dest.usePinned { destPinned ->
                        stream.next_in = srcPinned.addressOf(0)
                        stream.avail_in = data.size.convert()
                        stream.next_out = destPinned.addressOf(0)
                        stream.avail_out = uncompressedSize.convert()

                        val result = inflate(stream.ptr, Z_NO_FLUSH)
                        if (result != Z_STREAM_END && result != Z_OK) throw ZlibException(result)
                    }
                }
            } finally {
                inflateEnd(stream.ptr)
            }
            dest.toByteArray()
        }
    }
}
