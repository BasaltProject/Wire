package org.basaltmc.wire.io

import platform.posix.getchar

/**
 * Windows has no `termios`, and its console is driven through `SetConsoleMode` instead. That is not
 * wired up yet, so raw mode reports itself unavailable and the console falls back to plain line
 * reading: commands work, arrow-key history does not. A documented gap rather than a broken build,
 * and the same answer this returns on any platform without a terminal attached.
 */
public actual fun enableTerminalRawMode(): Boolean = false

public actual fun restoreTerminalMode(): Unit = Unit

public actual fun readTerminalByte(): Int = getchar()
