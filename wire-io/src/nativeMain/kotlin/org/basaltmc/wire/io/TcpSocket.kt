@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.usePinned
import platform.posix.AF_INET
import platform.posix.INADDR_ANY
import platform.posix.SOCK_STREAM
import platform.posix.accept
import platform.posix.bind
import platform.posix.connect
import platform.posix.listen
import platform.posix.recv
import platform.posix.send
import platform.posix.sockaddr
import platform.posix.sockaddr_in
import platform.posix.socket
import kotlin.concurrent.Volatile

/**
 * Thrown for any socket syscall failure. Wraps the raw `errno`-adjacent return code rather than
 * trying to translate it: platform error codes differ enough across POSIX/Winsock that a shared
 * translation layer belongs to a later pass.
 */
public class SocketException(
    message: String,
) : Exception(message)

/**
 * Sets `sin_family` (and, on BSD-family systems, `sin_len`) for a fresh [sockaddr_in]. Split out
 * as `expect`/`actual` because the struct genuinely differs at the ABI level, not just in the
 * bundled Kotlin binding: BSD/Darwin's `sockaddr_in` has a `u_char sin_len` field before an
 * `u_char sin_family` (4.4BSD's variable-length-record convention), while Linux's and Windows'
 * have no length field at all and `sin_family` alone is a 16-bit value.
 */
internal expect fun sockaddr_in.setAddressFamily()

/**
 * One-time per-platform socket subsystem setup: a no-op on POSIX, `WSAStartup` on Windows (Winsock
 * requires it before any other socket call). Called at the start of every [TcpListener.bind]/
 * [TcpConnection.connect], relying on `WSAStartup` being safe to call more than once (it is
 * refcounted by the OS); there is no matching `WSACleanup` anywhere yet because nothing in this
 * codebase has a process-shutdown hook to call it from.
 */
internal expect fun ensureSocketsInitialized()

/** `close()` on POSIX, `closesocket()` on Windows: a plain file descriptor doesn't work as a socket handle there. */
internal expect fun closeSocket(fd: Long)

/**
 * Enables `SO_REUSEADDR` on a freshly created listening socket, so restarting the server doesn't
 * have to wait out the previous socket's `TIME_WAIT`. Split out as `expect`/`actual` because
 * POSIX's `setsockopt` takes `optval` as `const void*` (any pointer), while Winsock's C prototype
 * takes it as `const char*`, which cinterop binds as a Kotlin `String`, not a pointer: there is no
 * direct way to pass a raw 4-byte int through that binding. The Windows `actual` is a no-op rather
 * than a guessed workaround; the only consequence is an occasional "address already in use" on a
 * fast restart there, not a functional gap.
 */
internal expect fun enableAddressReuse(fd: Long)

/** Sets the raw 32-bit address (`INADDR_ANY` or a parsed literal) on a [sockaddr_in]. Split out because
 * Windows exposes it through a nested union (`S_un.S_addr`) rather than the `s_addr` macro cinterop
 * doesn't see, unlike POSIX/BSD's direct `in_addr.s_addr` field. */
internal expect fun sockaddr_in.setAddress(value: UInt)

/** `errno` on POSIX, `WSAGetLastError()` on Windows: Winsock keeps its own error state and never sets `errno`. */
internal expect fun lastSocketError(): Int

/** A signal interrupted the call, which says nothing about the connection. */
internal expect fun socketErrorIsInterrupt(error: Int): Boolean

/** The peer is gone, which [TcpConnection.read] and [TcpConnection.write] treat as end of stream, not failure. */
internal expect fun socketErrorIsPeerGone(error: Int): Boolean

/**
 * The raw 32-bit address at the other end of a connected socket, or null if it cannot be read.
 *
 * Split per platform for the same reason [enableAddressReuse] is: `getpeername`'s length argument
 * is a `socklen_t*` on POSIX and an `int*` on Winsock, and the two bind to different Kotlin types.
 * Returning the raw value rather than a formatted string keeps the platform half to the syscall.
 */
internal expect fun peerAddress(fd: Long): UInt?

/**
 * Blocking IPv4 TCP sockets, a thread-per-connection building block rather than an event loop.
 * Literal IPv4 only, no DNS, and little-endian hosts only, which every target here is.
 *
 * The descriptor is a [Long] so POSIX's `int` and Winsock's 64-bit `SOCKET` share this class:
 * bit-reinterpreted, Winsock's all-ones `INVALID_SOCKET` lands on `-1L` like POSIX's `-1`, so the
 * `< 0` checks stay correct on both.
 */
public class TcpListener private constructor(
    private val fd: Long,
) {
    /** For registering with [SelectEventLoop]; see [TcpListener]'s own KDoc for why `Int` is safe here. */
    public val nativeFd: Int get() = fd.toInt()

    /**
     * Accepts one connection, and asks who it is from straight away.
     *
     * Straight away because this is the last moment the answer is guaranteed to be there: a client
     * that pings the server list and drops the socket the instant it has its answer is completely
     * ordinary, and by the time anything wants the address the peer can already be gone.
     */
    public fun accept(): TcpConnection {
        val clientFd = accept(fd.convert(), null, null).toLong()
        if (clientFd < 0) throw SocketException("accept() failed")
        return TcpConnection(clientFd, peerAddress(clientFd)?.let(::formatIpv4NetworkOrder))
    }

    public fun close() {
        closeSocket(fd)
    }

    public companion object {
        public fun bind(
            port: Int,
            backlog: Int = 128,
        ): TcpListener {
            ensureSocketsInitialized()
            val fd = socket(AF_INET, SOCK_STREAM, 0).toLong()
            if (fd < 0) throw SocketException("socket() failed")

            enableAddressReuse(fd)

            memScoped {
                val address = alloc<sockaddr_in>()
                address.setAddressFamily()
                address.sin_port = hostToNetworkShort(port)
                address.setAddress(INADDR_ANY)

                if (bind(fd.convert(), address.ptr.reinterpret<sockaddr>(), sizeOf<sockaddr_in>().convert()) != 0) {
                    closeSocket(fd)
                    throw SocketException("bind() failed on port $port")
                }
            }

            if (listen(fd.convert(), backlog) != 0) {
                closeSocket(fd)
                throw SocketException("listen() failed")
            }
            return TcpListener(fd)
        }
    }
}

public class TcpConnection internal constructor(
    private val fd: Long,
    /**
     * The peer's literal IPv4 address, or null when the socket was not accepted from a listener
     * (an outgoing [connect], or a test harness) and so nobody recorded one.
     */
    public val remoteAddress: String? = null,
) {
    /** For registering with [SelectEventLoop]; see [TcpListener]'s own KDoc for why `Int` is safe here. */
    public val nativeFd: Int get() = fd.toInt()

    /**
     * Whether [close] has run.
     *
     * Read before every syscall because closing and writing come from different threads: a player
     * being disconnected while a tick still had something to send reached `send()` on a descriptor
     * that no longer existed, which is an `EBADF` and, uncaught on a bare thread, the end of the
     * process.
     */
    @Volatile
    private var closed = false

    public val isClosed: Boolean get() = closed

    /**
     * Returns 0 at end of stream, including when the peer reset the connection instead of closing
     * it politely. A client that pings the server list and yanks the socket the moment it has its
     * answer is doing something completely ordinary, not failing, so it must not surface as an
     * error; `ECONNRESET` is how that arrives. `EINTR` is retried, since a signal interrupting the
     * syscall says nothing about the connection.
     */
    public fun read(
        buffer: ByteArray,
        offset: Int = 0,
        length: Int = buffer.size - offset,
    ): Int {
        if (closed) return 0
        while (true) {
            val received =
                buffer.usePinned { pinned ->
                    recv(fd.convert(), pinned.addressOf(offset), length.convert(), 0)
                }
            if (received >= 0) return received.toInt()
            val error = lastSocketError()
            when {
                socketErrorIsInterrupt(error) -> continue
                socketErrorIsPeerGone(error) -> return 0
                else -> throw SocketException("recv() failed with error $error")
            }
        }
    }

    /** Stops silently if the peer is already gone, for the same reason [read] returns 0 rather than throwing. */
    public fun write(
        buffer: ByteArray,
        offset: Int = 0,
        length: Int = buffer.size - offset,
    ) {
        if (closed) return
        var written = 0
        while (written < length) {
            val sent =
                buffer.usePinned { pinned ->
                    send(fd.convert(), pinned.addressOf(offset + written), (length - written).convert(), 0)
                }
            if (sent < 0) {
                val error = lastSocketError()
                when {
                    socketErrorIsInterrupt(error) -> continue
                    socketErrorIsPeerGone(error) -> return
                    else -> throw SocketException("send() failed with error $error")
                }
            }
            written += sent.toInt()
        }
    }

    public fun close() {
        if (closed) return
        closed = true
        closeSocket(fd)
    }

    public companion object {
        public fun connect(
            host: String,
            port: Int,
        ): TcpConnection {
            ensureSocketsInitialized()
            val fd = socket(AF_INET, SOCK_STREAM, 0).toLong()
            if (fd < 0) throw SocketException("socket() failed")

            memScoped {
                val address = alloc<sockaddr_in>()
                address.setAddressFamily()
                address.sin_port = hostToNetworkShort(port)
                address.setAddress(parseIpv4NetworkOrder(host))

                if (connect(fd.convert(), address.ptr.reinterpret<sockaddr>(), sizeOf<sockaddr_in>().convert()) != 0) {
                    closeSocket(fd)
                    throw SocketException("connect() failed to $host:$port")
                }
            }
            return TcpConnection(fd)
        }
    }
}

/** Byte-swaps [value] for a little-endian host so it lands in the field in network byte order. */
private fun hostToNetworkShort(value: Int): UShort = (((value and 0xFF) shl 8) or ((value ushr 8) and 0xFF)).toUShort()

/** The inverse of [parseIpv4NetworkOrder], and it shares that function's little-endian-host assumption. */
private fun formatIpv4NetworkOrder(value: UInt): String =
    "${value and 0xFFu}.${(value shr 8) and 0xFFu}.${(value shr 16) and 0xFFu}.${(value shr 24) and 0xFFu}"

/** Parses a literal IPv4 address (e.g. "127.0.0.1") into the little-endian-host in_addr value. */
private fun parseIpv4NetworkOrder(address: String): UInt {
    val octets = address.split(".").map { it.toUInt() }
    require(octets.size == 4 && octets.all { it <= 255u }) { "Not a literal IPv4 address: $address" }
    return octets[0] or (octets[1] shl 8) or (octets[2] shl 16) or (octets[3] shl 24)
}
