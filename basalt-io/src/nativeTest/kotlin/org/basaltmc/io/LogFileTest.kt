package org.basaltmc.io

import org.basaltmc.common.logging.LogOutput
import org.basaltmc.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class LogFileTest {
    private val root = "./log_file_test"
    private val archive = "$root/${LogFile.ARCHIVE_DIRECTORY}"
    private val current = "$root/${LogFile.CURRENT_FILE}"

    @BeforeTest
    fun setUp() {
        cleanUp()
    }

    @AfterTest
    fun tearDown() {
        LogOutput.clearSinks()
        cleanUp()
    }

    private fun cleanUp() {
        LogOutput.clearSinks()
        for (name in FileIO.list(archive)) FileIO.delete("$archive/$name")
        FileIO.delete(current)
        FileIO.delete(archive)
        FileIO.delete(root)
    }

    @Test
    fun writesLoggedLinesIntoLatestLog() {
        LogFile.install(root, keepDays = 0)
        Logger("Test").info { "hello from the log file" }

        val written = FileIO.readText(current)
        assertTrue(written != null && written.contains("hello from the log file"), "log was: $written")
    }

    @Test
    fun keepsLatestLogAtTheRootAndArchivesBelowIt() {
        FileIO.createDirectories(root)
        FileIO.writeText(current, "from the previous run\n")

        LogFile.install(root, keepDays = 0)

        // The file anyone actually opens stays at the top level, uncluttered by history.
        val archived = FileIO.list(archive)
        assertEquals(1, archived.size, "expected exactly one archive, got $archived")
        assertTrue(archived.single().endsWith(".log.gz"), "archive was: ${archived.single()}")
    }

    @Test
    fun archivesThePreviousRunCompressedAndReadableBack() {
        FileIO.createDirectories(root)
        FileIO.writeText(current, "from the previous run\n")

        LogFile.install(root, keepDays = 0)

        val archived = FileIO.list(archive).single()
        val bytes = FileIO.readBytes("$archive/$archived")
        assertTrue(bytes != null && bytes.size >= 2 && bytes[0] == 0x1F.toByte() && bytes[1] == 0x8B.toByte())
        assertEquals("from the previous run\n", Gzip.decompress(bytes).decodeToString())
    }

    @Test
    fun startsAFreshLatestLogEveryRun() {
        FileIO.createDirectories(root)
        FileIO.writeText(current, "from the previous run\n")

        LogFile.install(root, keepDays = 0)

        assertEquals("", FileIO.readText(current) ?: "")
    }

    @Test
    fun neverOverwritesAnEarlierArchiveFromTheSameDay() {
        FileIO.createDirectories(root)
        repeat(3) {
            FileIO.writeText(current, "run $it\n")
            LogFile.install(root, keepDays = 0)
            LogOutput.clearSinks()
        }

        assertEquals(3, FileIO.list(archive).size, "restarting three times must leave three archives")
    }

    @Test
    fun keepsAnEmptyPreviousLogOutOfTheArchive() {
        FileIO.createDirectories(root)
        FileIO.writeText(current, "")

        LogFile.install(root, keepDays = 0)

        assertEquals(emptyList(), FileIO.list(archive))
    }

    @Test
    fun prunesArchivesOlderThanTheRetentionWindow() {
        FileIO.createDirectories(archive)
        val old = "1999-01-01-1.log.gz"
        val recent = "${LogFile.daysBeforeToday(1)}-1.log.gz"
        FileIO.writeText("$archive/$old", "old")
        FileIO.writeText("$archive/$recent", "recent")

        LogFile.pruneArchives(archive, keepDays = 14)

        assertNull(FileIO.readText("$archive/$old"))
        assertEquals("recent", FileIO.readText("$archive/$recent"))
    }

    @Test
    fun leavesUnrelatedFilesInTheArchiveAlone() {
        FileIO.createDirectories(archive)
        FileIO.writeText("$archive/notes.txt", "not a log")

        LogFile.pruneArchives(archive, keepDays = 1)

        assertEquals("not a log", FileIO.readText("$archive/notes.txt"))
    }
}
