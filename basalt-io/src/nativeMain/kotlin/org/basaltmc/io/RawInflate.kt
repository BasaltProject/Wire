@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.usePinned
import platform.zlib.ZLIB_VERSION
import platform.zlib.Z_NO_FLUSH
import platform.zlib.Z_OK
import platform.zlib.Z_STREAM_END
import platform.zlib.inflate
import platform.zlib.inflateEnd
import platform.zlib.inflateInit2_
import platform.zlib.z_stream

/**
 * Raw DEFLATE (RFC 1951, no zlib/gzip wrapper) decompression, needed for ZIP entries: unlike the
 * play protocol's packet compression (RFC 1950, [Zlib]), the ZIP format's "deflate" method stores
 * the bare deflate stream with no header or checksum.
 *
 * `inflateInit2` is a C macro (`inflateInit2_(strm, windowBits, ZLIB_VERSION, sizeof(z_stream))`),
 * not callable from Kotlin via cinterop (only real declarations are): calls the real
 * `inflateInit2_` directly instead, expanding the macro's own definition by hand. `windowBits =
 * -15` is what tells zlib "raw deflate, no wrapper" (a negative value suppresses the header zlib
 * would otherwise expect).
 */
public object RawInflate {
    private const val RAW_DEFLATE_WINDOW_BITS = -15

    public fun decompress(
        data: ByteArray,
        uncompressedSize: Int,
    ): ByteArray {
        if (uncompressedSize == 0) return ByteArray(0)
        require(data.isNotEmpty()) { "Cannot decompress empty input into $uncompressedSize bytes" }

        val dest = UByteArray(uncompressedSize)
        val source = data.toUByteArray()
        return memScoped {
            val stream = alloc<z_stream>()
            val initResult =
                inflateInit2_(stream.ptr, RAW_DEFLATE_WINDOW_BITS, ZLIB_VERSION, sizeOf<z_stream>().convert())
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
