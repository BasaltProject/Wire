package org.basaltmc.wire.io

/**
 * A minimal ZIP reader: enough to look up specific entries by name out of a jar (`client.jar`,
 * `server.jar`), not a general-purpose archive library. Reads the central directory once (from
 * the end of the file, per the ZIP format), then extracts individual entries on demand. Supports
 * the two compression methods every real Minecraft jar entry uses: `0` (stored) and `8` (deflate,
 * via [RawInflate]); anything else throws rather than silently returning garbage.
 */
public class ZipReader(
    private val data: ByteArray,
) {
    private data class CentralDirectoryEntry(
        val name: String,
        val compressionMethod: Int,
        val compressedSize: Int,
        val uncompressedSize: Int,
        val localHeaderOffset: Int,
    )

    private val entriesByName: Map<String, CentralDirectoryEntry> by lazy { parseCentralDirectory() }

    public val entryNames: Set<String> get() = entriesByName.keys

    public fun readEntry(name: String): ByteArray? {
        val entry = entriesByName[name] ?: return null
        val localHeaderStart = entry.localHeaderOffset
        require(data.readU32(localHeaderStart) == LOCAL_FILE_HEADER_SIGNATURE) {
            "corrupt ZIP: local file header signature mismatch for '$name'"
        }
        val nameLength = data.readU16(localHeaderStart + 26)
        val extraLength = data.readU16(localHeaderStart + 28)
        val dataStart = localHeaderStart + 30 + nameLength + extraLength
        val compressed = data.copyOfRange(dataStart, dataStart + entry.compressedSize)

        return when (entry.compressionMethod) {
            COMPRESSION_STORED -> compressed
            COMPRESSION_DEFLATE -> RawInflate.decompress(compressed, entry.uncompressedSize)
            else ->
                throw ZipFormatException(
                    "unsupported ZIP compression method ${entry.compressionMethod} for '$name'",
                )
        }
    }

    private fun parseCentralDirectory(): Map<String, CentralDirectoryEntry> {
        val eocdOffset = findEndOfCentralDirectory()
        val entryCount = data.readU16(eocdOffset + 10)
        val centralDirectoryOffset = data.readU32(eocdOffset + 16).toInt()

        val entries = LinkedHashMap<String, CentralDirectoryEntry>(entryCount)
        var offset = centralDirectoryOffset
        repeat(entryCount) {
            require(data.readU32(offset) == CENTRAL_DIRECTORY_SIGNATURE) {
                "corrupt ZIP: central directory header signature mismatch at $offset"
            }
            val compressionMethod = data.readU16(offset + 10)
            val compressedSize = data.readU32(offset + 20).toInt()
            val uncompressedSize = data.readU32(offset + 24).toInt()
            val nameLength = data.readU16(offset + 28)
            val extraLength = data.readU16(offset + 30)
            val commentLength = data.readU16(offset + 32)
            val localHeaderOffset = data.readU32(offset + 42).toInt()
            val name = data.decodeToString(offset + 46, offset + 46 + nameLength)

            entries[name] =
                CentralDirectoryEntry(name, compressionMethod, compressedSize, uncompressedSize, localHeaderOffset)
            offset += 46 + nameLength + extraLength + commentLength
        }
        return entries
    }

    /**
     * The end-of-central-directory record sits at the very end of the file, but a variable-length
     * comment field follows it, so it's found by scanning backward for its signature rather than
     * seeking to a fixed offset. Comments are capped at 65535 bytes by the format itself, so the
     * search window is bounded.
     */
    private fun findEndOfCentralDirectory(): Int {
        val searchStart = maxOf(0, data.size - EOCD_FIXED_SIZE - MAX_COMMENT_LENGTH)
        for (offset in data.size - EOCD_FIXED_SIZE downTo searchStart) {
            if (data.readU32(offset) == END_OF_CENTRAL_DIRECTORY_SIGNATURE) return offset
        }
        throw ZipFormatException("not a ZIP file: no end-of-central-directory record found")
    }

    private companion object {
        const val LOCAL_FILE_HEADER_SIGNATURE = 0x04034b50L
        const val CENTRAL_DIRECTORY_SIGNATURE = 0x02014b50L
        const val END_OF_CENTRAL_DIRECTORY_SIGNATURE = 0x06054b50L
        const val EOCD_FIXED_SIZE = 22
        const val MAX_COMMENT_LENGTH = 65535
        const val COMPRESSION_STORED = 0
        const val COMPRESSION_DEFLATE = 8
    }
}

private fun ByteArray.readU16(offset: Int): Int =
    (this[offset].toInt() and 0xFF) or ((this[offset + 1].toInt() and 0xFF) shl 8)

private fun ByteArray.readU32(offset: Int): Long =
    (this[offset].toLong() and 0xFF) or
        ((this[offset + 1].toLong() and 0xFF) shl 8) or
        ((this[offset + 2].toLong() and 0xFF) shl 16) or
        ((this[offset + 3].toLong() and 0xFF) shl 24)

/** Thrown for an archive this reader cannot make sense of. */
public class ZipFormatException(
    message: String,
) : Exception(message)
