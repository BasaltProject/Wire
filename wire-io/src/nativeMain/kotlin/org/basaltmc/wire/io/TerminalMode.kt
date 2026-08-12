package org.basaltmc.wire.io

/**
 * Puts the terminal into raw mode, where a keypress arrives as it happens. In canonical mode the
 * terminal buffers a whole line and echoes what it does not understand, so an arrow key prints
 * `^[[A` instead of reaching the program: history editing needs this.
 *
 * Returns whether the mode changed. `false` means stdin is not a terminal, which is not a failure:
 * the caller falls back to plain line reading, all those cases support anyway.
 */
public expect fun enableTerminalRawMode(): Boolean

/** Puts the terminal back exactly as it was found. Safe to call even if raw mode was never entered. */
public expect fun restoreTerminalMode()

/** One byte from stdin, or -1 at end of input. In raw mode this returns as soon as a key is pressed. */
public expect fun readTerminalByte(): Int
