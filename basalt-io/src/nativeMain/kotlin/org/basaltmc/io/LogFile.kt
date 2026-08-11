package org.basaltmc.io

import org.basaltmc.common.logging.LogOutput
import org.basaltmc.common.logging.Logger
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * Mirrors the console into `<root>/latest.log`, archiving the previous run's file on the way in.
 * `latest.log` stays alone at the top of [root], with the gzipped `yyyy-MM-dd-N.log.gz` archives
 * one level down in [ARCHIVE_DIRECTORY], so the current log is not buried among hundreds of dated
 * ones. `N` counts runs on the same date and is found by probing, which needs no directory read.
 *
 * Nothing is buffered: after a crash, every line logged up to it is worth more than the syscalls
 * saved, and log volume is nothing next to chunk generation.
 */
public class LogFile private constructor(
    private val path: String,
) {
    private val mutex = NativeMutex()

    private fun append(line: String) {
        mutex.withLock { FileIO.appendText(path, line + "\n") }
    }

    public companion object {
        public const val ARCHIVE_DIRECTORY: String = "archive"
        public const val CURRENT_FILE: String = "latest.log"

        /**
         * Rotates the previous run's log, applies [keepDays], and starts mirroring into a fresh
         * `latest.log`, opening it with [header]. [keepDays] of 0 or less keeps archives forever.
         */
        @OptIn(ExperimentalTime::class)
        public fun install(
            root: String,
            keepDays: Int,
            header: String? = null,
        ): LogFile {
            val archiveRoot = "$root/$ARCHIVE_DIRECTORY"
            FileIO.createDirectories(archiveRoot)
            val path = "$root/$CURRENT_FILE"

            val previous = FileIO.readBytes(path)
            if (previous != null && previous.isNotEmpty()) {
                FileIO.writeBytes(nextArchivePath(archiveRoot, today()), Gzip.compress(previous))
            }
            FileIO.delete(path)

            if (keepDays > 0) pruneArchives(archiveRoot, keepDays)

            val file = LogFile(path)
            // The banner and version line reach the console through plain printing, before any
            // logger exists; without this the log would open mid-startup with no idea what built it.
            header?.let(file::append)
            LogOutput.addSink(file::append)
            return file
        }

        /**
         * Deletes archives whose own date is more than [keepDays] before today, read straight from
         * the file name. Dates are ISO-8601, so a plain string comparison against the cutoff orders
         * them correctly and needs no calendar arithmetic beyond the cutoff itself.
         */
        internal fun pruneArchives(
            archiveRoot: String,
            keepDays: Int,
        ) {
            val cutoff = daysBeforeToday(keepDays)
            for (name in FileIO.list(archiveRoot)) {
                if (!name.endsWith(".log.gz") || name.length < 10) continue
                if (name.substring(0, 10) < cutoff) FileIO.delete("$archiveRoot/$name")
            }
        }

        private fun nextArchivePath(
            archiveRoot: String,
            date: String,
        ): String {
            var index = 1
            while (FileIO.exists("$archiveRoot/$date-$index.log.gz")) index++
            return "$archiveRoot/$date-$index.log.gz"
        }

        @OptIn(ExperimentalTime::class)
        private fun today(): String = Logger.formatLocalDate(Clock.System.now())

        /** [days] before today as `yyyy-MM-dd`, in the same local zone the archive names use. */
        @OptIn(ExperimentalTime::class)
        internal fun daysBeforeToday(days: Int): String =
            Logger.formatLocalDate(Clock.System.now().minus(kotlin.time.Duration.parse("${days}d")))
    }
}
