@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.IntVar
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.value
import platform.posix.AF_INET
import platform.posix.EBADF
import platform.posix.ECONNRESET
import platform.posix.EINTR
import platform.posix.ENOTCONN
import platform.posix.EPIPE
import platform.posix.ETIMEDOUT
import platform.posix.SIGPIPE
import platform.posix.SIG_IGN
import platform.posix.SOL_SOCKET
import platform.posix.SO_REUSEADDR
import platform.posix.close
import platform.posix.errno
import platform.posix.getpeername
import platform.posix.setsockopt
import platform.posix.signal
import platform.posix.sockaddr
import platform.posix.sockaddr_in
import platform.posix.socklen_tVar

internal actual fun ensureSocketsInitialized() {
    // Writing to a socket whose peer has gone raises SIGPIPE, and its default action is to kill the
    // process **silently**: no message, no exit code anybody can read, nothing in the log. That is
    // exactly what a server does all day, since a client that alt-F4s during a broadcast is normal.
    // Ignored here so the write returns EPIPE instead and the SocketException path already in place
    // handles it. Once, at the one point every socket in the process goes through.
    signal(SIGPIPE, SIG_IGN)
}

internal actual fun closeSocket(fd: Long) {
    close(fd.convert())
}

internal actual fun enableAddressReuse(fd: Long) {
    memScoped {
        val reuse = alloc<IntVar>()
        reuse.value = 1
        setsockopt(fd.convert(), SOL_SOCKET, SO_REUSEADDR, reuse.ptr, sizeOf<IntVar>().convert())
    }
}

internal actual fun sockaddr_in.setAddress(value: UInt) {
    sin_addr.s_addr = value
}

/**
 * `getpeername` into a [sockaddr_in], which is all this server ever accepts: the listener binds
 * AF_INET, so anything reporting another family is left unanswered rather than misreported.
 */
internal actual fun peerAddress(fd: Long): UInt? =
    memScoped {
        val address = alloc<sockaddr_in>()
        val length = alloc<socklen_tVar>()
        length.value = sizeOf<sockaddr_in>().convert()
        if (getpeername(fd.convert(), address.ptr.reinterpret<sockaddr>(), length.ptr) != 0) {
            return@memScoped null
        }
        if (address.sin_family.toInt() != AF_INET) return@memScoped null
        address.sin_addr.s_addr
    }

internal actual fun lastSocketError(): Int = errno

internal actual fun socketErrorIsInterrupt(error: Int): Boolean = error == EINTR

internal actual fun socketErrorIsPeerGone(error: Int): Boolean =
    // EBADF is here because a socket closed from another thread is a connection that is gone, not a
    // fault: reaching a bare thread as an exception ended the process while it was saving.
    error == ECONNRESET || error == ENOTCONN || error == ETIMEDOUT || error == EPIPE || error == EBADF
