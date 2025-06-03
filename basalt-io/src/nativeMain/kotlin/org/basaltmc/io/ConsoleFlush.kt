@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import platform.posix.fflush
import platform.posix.stdout

/**
 * Pushes whatever has been printed out to the terminal now.
 *
 * Standard output is line buffered, and a line editor redraws without ever writing a newline: the
 * prompt would sit in the buffer and the operator would type into what looks like a dead terminal.
 */
public fun flushConsole() {
    fflush(stdout)
}
