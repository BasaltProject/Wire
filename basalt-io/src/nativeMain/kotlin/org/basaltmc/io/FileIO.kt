@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.convert
import kotlinx.cinterop.pointed
import kotlinx.cinterop.toKString
import kotlinx.cinterop.usePinned
import platform.posix.FILE
import platform.posix.SEEK_END
import platform.posix.SEEK_SET
import platform.posix.closedir
import platform.posix.fclose
import platform.posix.fopen
import platform.posix.fread
import platform.posix.fseek
import platform.posix.ftell
import platform.posix.fwrite
import platform.posix.opendir
import platform.posix.readdir

/**
 * An open file being written sequentially, with the ability to go back and patch what was already
 * written. Formats whose header declares the length of what follows (Linear's compressed size, for
 * one) can then be streamed straight to disk: write the header with a hole, stream the body, seek
 * back and fill the hole in. The alternative is building the whole body in memory first, which for
 * a region is tens of megabytes.
 */
public class FileWriter internal constructor(
    private val handle: CPointer<FILE>,
) {
    public fun write(
        bytes: ByteArray,
        length: Int = bytes.size,
    ) {
        if (length <= 0) return
        bytes.usePinned { pinned ->
            fwrite(pinned.addressOf(0), 1.convert(), length.convert(), handle)
        }
    }

    public fun seek(offset: Long) {
        fseek(handle, offset.convert(), SEEK_SET)
    }

    public fun close() {
        fclose(handle)
    }
}

/** Thrown for file I/O failures (missing parent directory, permissions, disk full). */
public class FileIOException(
    message: String,
) : Exception(message)

/**
 * Plain text file access for config/data files that live outside the plugin/world data model
 * (`basalt.toml`, `eula.txt`, asset caches). `fopen`/`fread`/`fwrite`/`fclose` are real `stdio.h`
 * declarations, not macros, so unlike `select()` they bind identically under `platform.posix` on
 * all three targets, no per-platform `expect`/`actual` needed.
 */
public object FileIO {
    public fun exists(path: String): Boolean {
        val handle = fopen(path, "rb") ?: return false
        fclose(handle)
        return true
    }

    /**
     * Whether [path] is a directory. [exists] cannot answer this: its `fopen` opens a directory on
     * POSIX and refuses one on Windows, so it reports the opposite answer per platform.
     */
    public fun directoryExists(path: String): Boolean {
        val dir = opendir(path) ?: return false
        closedir(dir)
        return true
    }

    /** Opens [path] for writing, truncating it. The caller closes it, ideally in a `finally`. */
    public fun openWrite(path: String): FileWriter {
        val handle = fopen(path, "wb") ?: throw FileIOException("failed to open '$path' for writing")
        return FileWriter(handle)
    }

    /** Removes [path], returning whether it was there to remove. */
    public fun delete(path: String): Boolean = platform.posix.remove(path) == 0

    /** Entry names directly inside [path] (no recursion, no `.`/`..`), empty if it is not a readable directory. */
    public fun list(path: String): List<String> {
        val dir = opendir(path) ?: return emptyList()
        try {
            val names = mutableListOf<String>()
            while (true) {
                val entry = readdir(dir) ?: break
                val name = entry.pointed.d_name.toKString()
                if (name != "." && name != "..") names += name
            }
            return names
        } finally {
            closedir(dir)
        }
    }

    /** Appends to [path], creating it if needed: what the log file writes through. */
    public fun appendText(
        path: String,
        content: String,
    ) {
        val bytes = content.encodeToByteArray()
        val handle = fopen(path, "ab") ?: throw FileIOException("failed to open '$path' for appending")
        try {
            if (bytes.isNotEmpty()) {
                bytes.usePinned { pinned ->
                    fwrite(pinned.addressOf(0), 1.convert(), bytes.size.convert(), handle)
                }
            }
        } finally {
            fclose(handle)
        }
    }

    /** Byte length without reading the contents, or `null` if [path] does not exist. */
    public fun size(path: String): Long? {
        val handle = fopen(path, "rb") ?: return null
        try {
            fseek(handle, 0, SEEK_END)
            // `convert`, not a plain return: `ftell` gives a 64-bit `Long` on POSIX and a 32-bit
            // `Int` on mingw, where `long` is half the width. Removing this "redundant" conversion
            // compiles fine everywhere except Windows.
            return ftell(handle).convert()
        } finally {
            fclose(handle)
        }
    }

    /** Returns `null` if [path] does not exist. */
    public fun readText(path: String): String? {
        val handle = fopen(path, "rb") ?: return null
        try {
            fseek(handle, 0, SEEK_END)
            val size = ftell(handle)
            fseek(handle, 0, SEEK_SET)
            if (size <= 0) return ""

            val buffer = ByteArray(size.toInt())
            val read =
                buffer.usePinned { pinned ->
                    fread(pinned.addressOf(0), 1.convert(), size.convert(), handle)
                }
            return buffer.decodeToString(0, read.toInt())
        } finally {
            fclose(handle)
        }
    }

    public fun writeText(
        path: String,
        content: String,
    ) {
        writeBytes(path, content.encodeToByteArray())
    }

    /** Returns `null` if [path] does not exist. */
    public fun readBytes(path: String): ByteArray? {
        val handle = fopen(path, "rb") ?: return null
        try {
            fseek(handle, 0, SEEK_END)
            val size = ftell(handle)
            fseek(handle, 0, SEEK_SET)
            if (size <= 0) return ByteArray(0)

            val buffer = ByteArray(size.toInt())
            buffer.usePinned { pinned ->
                fread(pinned.addressOf(0), 1.convert(), size.convert(), handle)
            }
            return buffer
        } finally {
            fclose(handle)
        }
    }

    /** Writes the first [length] bytes of [bytes], so callers can hand over a reused buffer. */
    public fun writeBytes(
        path: String,
        bytes: ByteArray,
        length: Int,
    ) {
        val handle = fopen(path, "wb") ?: throw FileIOException("failed to open '$path' for writing")
        try {
            if (length > 0) {
                bytes.usePinned { pinned ->
                    fwrite(pinned.addressOf(0), 1.convert(), length.convert(), handle)
                }
            }
        } finally {
            fclose(handle)
        }
    }

    public fun writeBytes(
        path: String,
        bytes: ByteArray,
    ) {
        val handle = fopen(path, "wb") ?: throw FileIOException("failed to open '$path' for writing")
        try {
            if (bytes.isNotEmpty()) {
                bytes.usePinned { pinned ->
                    fwrite(pinned.addressOf(0), 1.convert(), bytes.size.convert(), handle)
                }
            }
        } finally {
            fclose(handle)
        }
    }

    /**
     * `mkdir -p`: creates [path] and any missing parent directories. Ignores failures on segments
     * that already exist (the only way to tell "already there" from a real error without a
     * separate `stat` call, and a subsequent [writeBytes]/[writeText] fails loudly anyway if the
     * directory genuinely couldn't be created).
     */
    public fun createDirectories(path: String) {
        val segments = path.split('/').filter { it.isNotEmpty() }
        val prefix = if (path.startsWith("/")) "/" else ""
        var current = prefix
        for (segment in segments) {
            current += segment
            createDirectory(current)
            current += "/"
        }
    }
}

/**
 * `mkdir`'s signature differs by platform: POSIX takes a mode, mingw-w64's compat shim doesn't
 * (matching `_mkdir`'s single-argument MSVCRT semantics), the same class of ABI difference this
 * project has hit repeatedly (`select()`, `setsockopt`, `sockaddr_in`). Return value is ignored by
 * the only caller ([FileIO.createDirectories]), which treats "already exists" and "created" alike.
 */
internal expect fun createDirectory(path: String)
