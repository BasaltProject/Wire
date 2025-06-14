package org.basaltmc.wire.io

import com.squareup.zstd.ZSTD_e_continue
import com.squareup.zstd.ZSTD_e_end
import com.squareup.zstd.getErrorName
import com.squareup.zstd.zstdCompressor
import com.squareup.zstd.zstdDecompressor

/** Thrown for zstd failures (corrupt input, bad compression level). */
public class ZstdException(
    message: String,
) : Exception(message)

/**
 * zstd (RFC 8878), via `com.squareup.zstd:zstd-kmp`: a pure-KMP binding that compiles zstd's own
 * C source per target (no system `libzstd` to install, unlike a hand-rolled cinterop), covering
 * every native target this project builds, including macosX64. This is the compression the Linear
 * region format (`server-world`) wraps a whole region in, once per file, unlike Anvil's per-chunk
 * zlib. Only one-shot whole-buffer use is exposed here: the library's own streaming API is lower
 * level (`ZstdCompressor`/`ZstdDecompressor`, raw buffer windows), so this wraps it the same way as
 * this project's other one-shot compression wrappers ([Zlib]).
 */
public object Zstd {
    // ZSTD_cParameter from zstd.h: stable public API values, not generated bindings.
    private const val ZSTD_C_COMPRESSION_LEVEL = 100
    private const val CHUNK_SIZE = 64 * 1024

    /** [length] lets a caller compress the used prefix of a reused buffer without copying it first. */
    public fun compress(
        data: ByteArray,
        level: Int = 3,
        length: Int = data.size,
    ): ByteArray {
        if (length == 0) return ByteArray(0)
        return zstdCompressor().use { compressor ->
            checkError(compressor.setParameter(ZSTD_C_COMPRESSION_LEVEL, level))
            val chunks = mutableListOf<ByteArray>()
            var inputStart = 0
            var result: Long
            do {
                val output = ByteArray(CHUNK_SIZE)
                result =
                    compressor.compressStream2(
                        outputByteArray = output,
                        outputEnd = output.size,
                        outputStart = 0,
                        inputByteArray = data,
                        inputEnd = length,
                        inputStart = inputStart,
                        mode = ZSTD_e_end,
                    )
                checkError(result)
                inputStart += compressor.inputBytesProcessed
                if (compressor.outputBytesProcessed > 0) chunks += output.copyOf(compressor.outputBytesProcessed)
            } while (inputStart < length || result != 0L)
            concatenate(chunks)
        }
    }

    /**
     * Compresses a sequence of pieces as one stream, handing each block of output to [sink] as it
     * is produced. Never holds the input or the output whole.
     *
     * This is what lets a caller compress something far larger than anything it allocates: a Linear
     * region is written from the chunk blobs it already holds, straight to the file, instead of
     * concatenating tens of megabytes and then compressing that. Buffers that size come from `mmap`
     * on Kotlin/Native and their pages are never returned, so avoiding them entirely is worth more
     * than reusing them.
     */
    public fun compressStreaming(
        level: Int = 3,
        sink: (ByteArray, Int) -> Unit,
        pieces: (feed: (ByteArray, Int) -> Unit) -> Unit,
    ) {
        zstdCompressor().use { compressor ->
            checkError(compressor.setParameter(ZSTD_C_COMPRESSION_LEVEL, level))
            val output = ByteArray(CHUNK_SIZE)

            fun drive(
                input: ByteArray,
                length: Int,
                mode: Int,
            ) {
                var inputStart = 0
                do {
                    val result =
                        compressor.compressStream2(
                            outputByteArray = output,
                            outputEnd = output.size,
                            outputStart = 0,
                            inputByteArray = input,
                            inputEnd = length,
                            inputStart = inputStart,
                            mode = mode,
                        )
                    checkError(result)
                    inputStart += compressor.inputBytesProcessed
                    if (compressor.outputBytesProcessed > 0) sink(output, compressor.outputBytesProcessed)
                } while (inputStart < length || (mode == ZSTD_e_end && result != 0L))
            }

            pieces { bytes, length -> if (length > 0) drive(bytes, length, ZSTD_e_continue) }
            drive(EMPTY, 0, ZSTD_e_end)
        }
    }

    private val EMPTY = ByteArray(0)

    public fun decompress(data: ByteArray): ByteArray {
        if (data.isEmpty()) return ByteArray(0)
        return zstdDecompressor().use { decompressor ->
            val chunks = mutableListOf<ByteArray>()
            var inputStart = 0
            var result: Long
            do {
                val output = ByteArray(CHUNK_SIZE)
                result =
                    decompressor.decompressStream(
                        outputByteArray = output,
                        outputEnd = output.size,
                        outputStart = 0,
                        inputByteArray = data,
                        inputEnd = data.size,
                        inputStart = inputStart,
                    )
                checkError(result)
                inputStart += decompressor.inputBytesProcessed
                if (decompressor.outputBytesProcessed > 0) chunks += output.copyOf(decompressor.outputBytesProcessed)
            } while (inputStart < data.size || result != 0L)
            concatenate(chunks)
        }
    }

    private fun checkError(result: Long) {
        getErrorName(result)?.let { throw ZstdException(it) }
    }

    internal fun check(result: Long) {
        checkError(result)
    }

    private fun concatenate(chunks: List<ByteArray>): ByteArray {
        val result = ByteArray(chunks.sumOf { it.size })
        var offset = 0
        for (chunk in chunks) {
            chunk.copyInto(result, offset)
            offset += chunk.size
        }
        return result
    }
}

/**
 * Pull-style decompression over one zstd frame sitting in [input] between [inputFrom] and [inputTo]:
 * every read lands directly in the caller's buffer, so a stream of any size is consumed without a
 * whole decompressed copy ever existing. The counterpart of [Zstd.compressStreaming], for readers.
 *
 * Close it when done; a frame is only valid if [frameComplete] is true once everything expected has
 * been read.
 */
public class ZstdStreamReader(
    private val input: ByteArray,
    inputFrom: Int = 0,
    private val inputTo: Int = input.size,
) : AutoCloseable {
    private val decompressor = zstdDecompressor()
    private var inputStart = inputFrom
    private var lastResult = -1L

    /** Whether the zstd frame ended exactly where the reads stopped, with no bytes left over. */
    public val frameComplete: Boolean get() = lastResult == 0L && inputStart == inputTo

    /** Fills [dest] from [offset] with [length] decompressed bytes, or throws on a truncated stream. */
    public fun readFully(
        dest: ByteArray,
        offset: Int = 0,
        length: Int = dest.size - offset,
    ) {
        var outputStart = offset
        val outputEnd = offset + length
        while (outputStart < outputEnd) {
            val result =
                decompressor.decompressStream(
                    outputByteArray = dest,
                    outputEnd = outputEnd,
                    outputStart = outputStart,
                    inputByteArray = input,
                    inputEnd = inputTo,
                    inputStart = inputStart,
                )
            Zstd.check(result)
            lastResult = result
            val consumed = decompressor.inputBytesProcessed
            val produced = decompressor.outputBytesProcessed
            inputStart += consumed
            outputStart += produced
            if (consumed == 0 && produced == 0) throw ZstdException("truncated zstd stream")
        }
    }

    override fun close() {
        decompressor.close()
    }
}
