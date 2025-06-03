@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import platform.posix.FD_SETSIZE
import platform.posix.fd_set
import platform.posix.posix_FD_ISSET
import platform.posix.posix_FD_SET
import platform.posix.posix_FD_ZERO
import platform.posix.timeval

/** The real `select()` symbol: only bound under `platform.windows` on mingw, `platform.posix` elsewhere. */
internal expect fun rawSelect(
    maxFd: Int,
    readSet: CPointer<fd_set>,
    timeout: CPointer<timeval>?,
): Int

/**
 * Readiness-based event loop over `select()`, not `kqueue`/`epoll`/IOCP: those need custom
 * cinterop per platform, `select()` doesn't (already bundled everywhere). Caps out at
 * [FD_SETSIZE] fds (1024 on POSIX, 64 on Windows), well past what a single server needs; revisit
 * for `basalt-proxy`, which will need real per-platform bindings.
 */
public class SelectEventLoop {
    private val registeredFds = LinkedHashSet<Int>()

    public val registeredCount: Int get() = registeredFds.size

    public companion object {
        public val maxWatchableFds: Int = FD_SETSIZE
    }

    public fun register(fd: Int) {
        require(fd >= 0) { "invalid fd: $fd" }
        check(registeredFds.size < FD_SETSIZE || fd in registeredFds) {
            "cannot watch more than $FD_SETSIZE fds with select()"
        }
        registeredFds.add(fd)
    }

    public fun unregister(fd: Int) {
        registeredFds.remove(fd)
    }

    /** Blocks up to [timeoutMillis] (indefinitely if `null`) for at least one registered fd to become readable. */
    public fun poll(timeoutMillis: Int? = null): List<Int> {
        if (registeredFds.isEmpty()) return emptyList()

        memScoped {
            val set = alloc<fd_set>()
            posix_FD_ZERO(set.ptr)
            var maxFd = 0
            for (fd in registeredFds) {
                posix_FD_SET(fd, set.ptr)
                if (fd > maxFd) maxFd = fd
            }

            val timeoutPtr =
                timeoutMillis?.let { millis ->
                    val tv = alloc<timeval>()
                    tv.tv_sec = (millis / 1000).convert()
                    tv.tv_usec = ((millis % 1000) * 1000).convert()
                    tv.ptr
                }

            val result = rawSelect(maxFd + 1, set.ptr, timeoutPtr)
            if (result < 0) throw SocketException("select() failed")

            return registeredFds.filter { posix_FD_ISSET(it, set.ptr) != 0 }
        }
    }
}
