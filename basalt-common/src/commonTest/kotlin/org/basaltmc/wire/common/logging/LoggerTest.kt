package org.basaltmc.wire.common.logging

import kotlinx.datetime.TimeZone
import kotlinx.datetime.toLocalDateTime
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.time.Instant

class LoggerTest {
    @Test
    fun suppressesMessagesBelowMinLevel() {
        val logger = Logger("test")
        logger.minLevel = LogLevel.WARN
        var evaluated = false

        logger.info {
            evaluated = true
            "should not run"
        }

        assertFalse(evaluated)
    }

    @Test
    fun evaluatesMessagesAtOrAboveMinLevel() {
        val logger = Logger("test")
        logger.minLevel = LogLevel.DEBUG
        var evaluated = false

        logger.warn {
            evaluated = true
            "should run"
        }

        assertTrue(evaluated)
    }

    @Test
    fun defaultMinLevelIsInfo() {
        assertEquals(LogLevel.INFO, Logger("test").minLevel)
    }
}

class TimestampFormatTest {
    @Test
    fun aStoredInstantIsFormattedInTheLocalZoneLikeEveryLogLine() {
        // Compared against the same instant resolved through the time zone rather than against a
        // fixed string: the expected value depends on where this runs, and asserting a UTC one
        // would pass only on a machine set to UTC, which is exactly the bug this replaced.
        val epochSeconds = 1786344468L
        val expected = Instant.fromEpochSeconds(epochSeconds).toLocalDateTime(TimeZone.currentSystemDefault())
        val formatted = Logger.formatEpochSeconds(epochSeconds)

        assertEquals(19, formatted.length)
        assertTrue(formatted.startsWith(expected.year.toString()))
        assertTrue(formatted.contains(":${expected.minute.toString().padStart(2, '0')}:"))
        assertTrue(formatted.endsWith(expected.second.toString().padStart(2, '0')))
    }

    @Test
    fun theShapeIsAlwaysTheSameWidth() {
        // Single-digit months, days and hours are padded, so log lines stay aligned.
        assertEquals(19, Logger.formatEpochSeconds(0L).length)
        assertEquals(19, Logger.formatEpochSeconds(1786344468L).length)
    }
}
