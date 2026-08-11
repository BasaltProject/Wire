package org.basaltmc.common.logging

import kotlinx.datetime.TimeZone
import kotlinx.datetime.toLocalDateTime
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.Instant

/**
 * A minimal, dependency-free leveled logger, writing `[yyyy-MM-dd HH:mm:ss] [LEVEL] [name]:
 * message` to stdout. Times are UTC: local time needs a `cinterop` call this project has not got
 * yet, and UTC is unambiguous meanwhile.
 *
 * Message producers are `inline`, so a call filtered out by [minLevel] builds no string and
 * allocates no closure.
 */
public class Logger(
    @PublishedApi internal val name: String,
) {
    public var minLevel: LogLevel = defaultMinLevel

    public inline fun trace(message: () -> String) {
        log(LogLevel.TRACE, message)
    }

    public inline fun debug(message: () -> String) {
        log(LogLevel.DEBUG, message)
    }

    public inline fun info(message: () -> String) {
        log(LogLevel.INFO, message)
    }

    public inline fun warn(message: () -> String) {
        log(LogLevel.WARN, message)
    }

    public inline fun error(message: () -> String) {
        log(LogLevel.ERROR, message)
    }

    public inline fun error(
        throwable: Throwable,
        message: () -> String,
    ) {
        log(LogLevel.ERROR) { "${message()}: ${throwable::class.simpleName}: ${throwable.message}" }
    }

    @PublishedApi
    internal inline fun log(
        level: LogLevel,
        message: () -> String,
    ) {
        if (level.ordinal < minLevel.ordinal) return
        LogOutput.emit(level, timestamp(), name, message())
    }

    public companion object {
        /** Applied to every [Logger] created after it is set; existing instances keep whatever [minLevel] they already had. */
        public var defaultMinLevel: LogLevel = LogLevel.INFO

        @OptIn(ExperimentalTime::class)
        @PublishedApi
        internal fun timestamp(): String = format(Clock.System.now())

        /**
         * A stored instant in the same shape as the timestamp on every log line, so a date inside a
         * message reads like the one in front of it instead of as a number nobody can decode.
         */
        @OptIn(ExperimentalTime::class)
        public fun formatEpochSeconds(epochSeconds: Long): String = format(Instant.fromEpochSeconds(epochSeconds))

        /**
         * Just the local calendar date, `yyyy-MM-dd`, for things named after a day rather than a
         * moment. It has to be the local date for the same reason the timestamps are local: an
         * archive written at one in the morning belongs to the day the operator thinks it does, not
         * to the previous one because UTC had not turned over yet.
         */
        @OptIn(ExperimentalTime::class)
        public fun formatLocalDate(instant: Instant): String = format(instant).substring(0, 10)

        /**
         * Local time, not UTC: an operator reads their own console against their own clock, and a
         * log two hours off their wall clock is a small tax paid on every single line. It is also
         * what vanilla does. The time zone is resolved per call rather than cached so that a server
         * running across a daylight-saving change keeps telling the truth.
         */
        @OptIn(ExperimentalTime::class)
        private fun format(instant: Instant): String {
            val local = instant.toLocalDateTime(TimeZone.currentSystemDefault())
            return buildString {
                append(local.year.toString().padStart(4, '0'))
                append('-')
                append(local.monthNumber.toString().padStart(2, '0'))
                append('-')
                append(local.day.toString().padStart(2, '0'))
                append(' ')
                append(local.hour.toString().padStart(2, '0'))
                append(':')
                append(local.minute.toString().padStart(2, '0'))
                append(':')
                append(local.second.toString().padStart(2, '0'))
            }
        }
    }
}
