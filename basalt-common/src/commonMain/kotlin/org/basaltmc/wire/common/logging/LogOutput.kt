package org.basaltmc.wire.common.logging

/**
 * Where log lines actually go. The console always gets them; anything else (a log file, a plugin
 * hook) registers through [addSink].
 *
 * Sinks are a callback rather than a direct file write because `basalt-common` has no dependencies
 * by design, so it cannot reach the filesystem itself. The file sink lives in `basalt-io`, which
 * can, and installs itself here at startup.
 *
 * Sinks receive the plain line, never the coloured one: colour is a terminal affordance, and
 * escape codes written into a log file are noise that every later reader has to strip.
 */
public object LogOutput {
    private val sinks = mutableListOf<(String) -> Unit>()

    /** Whether the console gets ANSI colour. Decided at startup, see `LogFile`'s own setup. */
    public var useColor: Boolean = false

    /**
     * Wraps every console line, for whoever else is drawing on the same terminal.
     *
     * The console command editor keeps a prompt on the bottom line and redraws it in place. A log
     * line written straight over that leaves the prompt smeared across the output and the operator
     * typing into wreckage, so the editor installs itself here to wipe its line first and put it
     * back afterwards. Left alone when nothing is editing, which is every non-interactive run.
     */
    public var aroundConsoleLine: ((write: () -> Unit) -> Unit)? = null

    /**
     * Registers an extra destination. Not synchronised: sinks are installed once during startup,
     * before anything else is logging, and the list is only read afterwards.
     */
    public fun addSink(sink: (String) -> Unit) {
        sinks.add(sink)
    }

    /** Drops every registered sink, so a shut-down file sink cannot be written to afterwards. */
    public fun clearSinks() {
        sinks.clear()
    }

    public fun emit(
        level: LogLevel,
        timestamp: String,
        name: String,
        message: String,
    ) {
        val plain = "[$timestamp] [${level.label}] [$name]: $message"
        val line = if (useColor) colored(level, timestamp, name, message) else plain
        val write = { println(line) }
        aroundConsoleLine?.invoke(write) ?: write()
        for (sink in sinks) sink(plain)
    }

    private fun colored(
        level: LogLevel,
        timestamp: String,
        name: String,
        message: String,
    ): String =
        "$DIM[$timestamp]$RESET ${level.color}[${level.label}]$RESET $DIM[$name]:$RESET ${level.messageColor}$message$RESET"

    private val LogLevel.color: String
        get() =
            when (this) {
                LogLevel.TRACE -> DIM
                LogLevel.DEBUG -> CYAN
                LogLevel.INFO -> GREEN
                LogLevel.WARN -> YELLOW
                LogLevel.ERROR -> RED
            }

    // Only warnings and errors tint the message itself; colouring every info line makes a busy
    // console harder to scan, not easier.
    private val LogLevel.messageColor: String
        get() =
            when (this) {
                LogLevel.WARN -> YELLOW
                LogLevel.ERROR -> RED
                else -> ""
            }

    // Written as an escape, not a literal ESC byte: a raw control character in source survives
    // neither editors nor copy-paste reliably.
    private const val RESET = "\u001B[0m"
    private const val DIM = "\u001B[2m"
    private const val RED = "\u001B[31m"
    private const val GREEN = "\u001B[32m"
    private const val YELLOW = "\u001B[33m"
    private const val CYAN = "\u001B[36m"
}
