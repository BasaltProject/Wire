@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import platform.posix.WSADATA
import platform.posix.WSAECONNABORTED
import platform.posix.WSAECONNRESET
import platform.posix.WSAEINTR
import platform.posix.WSAENOTCONN
import platform.posix.WSAESHUTDOWN
import platform.posix.WSAETIMEDOUT
import platform.posix.WSAGetLastError
import platform.posix.WSAStartup
import platform.posix.closesocket

/**
 * Winsock needs `WSAStartup` before any other socket call. Called on every bind and connect rather
 * than guarded by a flag: the OS refcounts it, and nothing here runs at shutdown to pair it with
 * `WSACleanup`. `0x0202` is `MAKEWORD(2, 2)` by hand, since cinterop does not expose macros.
 */
internal actual fun ensureSocketsInitialized() {
    memScoped {
        val wsaData = alloc<WSADATA>()
        WSAStartup(0x0202.convert(), wsaData.ptr)
    }
}

internal actual fun closeSocket(fd: Long) {
    closesocket(fd.convert())
}

/**
 * Deliberately not implemented: `SO_REUSEADDR` needs `optval` as a raw pointer, but Winsock's
 * `setsockopt` declares it `const char*`, which cinterop binds as a Kotlin `String`, not a
 * pointer. See [enableAddressReuse]'s KDoc in `TcpSocket.kt` for why that's a no-op here rather
 * than a guessed workaround.
 */
internal actual fun enableAddressReuse(fd: Long) {
    // intentionally a no-op on Windows; see the expect declaration's KDoc
}

internal actual fun lastSocketError(): Int = WSAGetLastError()

internal actual fun socketErrorIsInterrupt(error: Int): Boolean = error == WSAEINTR

/** Windows has no SIGPIPE: a write to a departed peer arrives as one of these, EPIPE's equivalent. */
internal actual fun socketErrorIsPeerGone(error: Int): Boolean =
    error == WSAECONNRESET ||
        error == WSAECONNABORTED ||
        error == WSAENOTCONN ||
        error == WSAESHUTDOWN ||
        error == WSAETIMEDOUT
