package org.basaltmc.io

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class FileIOTest {
    @Test
    fun readsBackWhatItWrote() {
        val path = "basalt_fileio_test_roundtrip.txt"
        FileIO.writeText(path, "hello\nbasalt")
        assertEquals("hello\nbasalt", FileIO.readText(path))
        removeTestFile(path)
    }

    @Test
    fun reportsExistence() {
        val path = "basalt_fileio_test_exists.txt"
        assertFalse(FileIO.exists(path))
        FileIO.writeText(path, "x")
        assertTrue(FileIO.exists(path))
        removeTestFile(path)
    }

    @Test
    fun readTextReturnsNullForAMissingFile() {
        assertNull(FileIO.readText("basalt_fileio_test_does_not_exist.txt"))
    }

    @Test
    fun writeTextOverwritesExistingContent() {
        val path = "basalt_fileio_test_overwrite.txt"
        FileIO.writeText(path, "first, and much longer than what follows")
        FileIO.writeText(path, "second")
        assertEquals("second", FileIO.readText(path))
        removeTestFile(path)
    }

    @Test
    fun readsAnEmptyFile() {
        val path = "basalt_fileio_test_empty.txt"
        FileIO.writeText(path, "")
        assertEquals("", FileIO.readText(path))
        removeTestFile(path)
    }

    @Test
    fun bytesRoundTrip() {
        val path = "basalt_fileio_test_bytes.bin"
        val bytes = byteArrayOf(0, 1, 2, -1, -128, 127)
        FileIO.writeBytes(path, bytes)
        assertTrue(FileIO.readBytes(path).contentEquals(bytes))
        removeTestFile(path)
    }

    @Test
    fun readBytesReturnsNullForAMissingFile() {
        assertNull(FileIO.readBytes("basalt_fileio_test_bytes_does_not_exist.bin"))
    }

    @Test
    fun createDirectoriesMakesNestedDirsUsableForAFileWrite() {
        val dir = "basalt_fileio_test_dir/nested/deeper"
        FileIO.createDirectories(dir)
        val path = "$dir/file.txt"
        FileIO.writeText(path, "hello")
        assertEquals("hello", FileIO.readText(path))
        removeTestFile(path)
        platform.posix.remove("basalt_fileio_test_dir/nested/deeper")
        platform.posix.remove("basalt_fileio_test_dir/nested")
        platform.posix.remove("basalt_fileio_test_dir")
    }

    /** Windows' `fopen` refuses a directory, so [FileIO.exists] answers this one differently per platform. */
    @Test
    fun directoryExistsTellsDirectoriesFromFilesAndMissingPaths() {
        val dir = "basalt_fileio_test_dir_probe"
        assertFalse(FileIO.directoryExists(dir))
        FileIO.createDirectories(dir)
        assertTrue(FileIO.directoryExists(dir))

        val path = "$dir/file.txt"
        FileIO.writeText(path, "x")
        assertFalse(FileIO.directoryExists(path))
        removeTestFile(path)
        platform.posix.remove(dir)
    }

    @Test
    fun createDirectoriesIsIdempotent() {
        val dir = "basalt_fileio_test_dir_repeat"
        FileIO.createDirectories(dir)
        FileIO.createDirectories(dir)
        val path = "$dir/file.txt"
        FileIO.writeText(path, "still works")
        assertEquals("still works", FileIO.readText(path))
        removeTestFile(path)
        platform.posix.remove(dir)
    }
}

private fun removeTestFile(path: String) {
    platform.posix.remove(path)
}
