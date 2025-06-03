@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.nativeHeap
import kotlinx.cinterop.ptr
import platform.posix.ECHO
import platform.posix.ICANON
import platform.posix.STDIN_FILENO
import platform.posix.TCSANOW
import platform.posix.getchar
import platform.posix.isatty
import platform.posix.tcgetattr
import platform.posix.tcsetattr
import platform.posix.termios

// The terminal settings as they were before this process touched them, kept so they can be put
// back. Allocated on the native heap rather than a scope because it has to outlive the call.
private var savedTermios: termios? = null

public actual fun enableTerminalRawMode(): Boolean {
    if (isatty(STDIN_FILENO) != 1) return false
    if (savedTermios != null) return true

    val saved = nativeHeap.alloc<termios>()
    if (tcgetattr(STDIN_FILENO, saved.ptr) != 0) {
        nativeHeap.free(saved.rawPtr)
        return false
    }

    val raw = nativeHeap.alloc<termios>()
    if (tcgetattr(STDIN_FILENO, raw.ptr) != 0) {
        nativeHeap.free(saved.rawPtr)
        nativeHeap.free(raw.rawPtr)
        return false
    }
    // Only line buffering and echo go: signals (ctrl+C) and output processing stay exactly as they
    // were, so the terminal still behaves like a terminal in every other respect.
    raw.c_lflag = raw.c_lflag and (ICANON.convert<ULong>() or ECHO.convert<ULong>()).inv().convert()
    val applied = tcsetattr(STDIN_FILENO, TCSANOW, raw.ptr) == 0
    nativeHeap.free(raw.rawPtr)
    if (!applied) {
        nativeHeap.free(saved.rawPtr)
        return false
    }
    savedTermios = saved
    return true
}

public actual fun restoreTerminalMode() {
    val saved = savedTermios ?: return
    tcsetattr(STDIN_FILENO, TCSANOW, saved.ptr)
    nativeHeap.free(saved.rawPtr)
    savedTermios = null
}

public actual fun readTerminalByte(): Int = getchar()
