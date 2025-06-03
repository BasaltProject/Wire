package org.basaltmc.io

/**
 * A `pthread_mutex_t` wrapper for state shared between [NativeThread]s (Kotlin/Native's stdlib
 * has no blocking lock outside `kotlinx.coroutines.sync.Mutex`, which suspends rather than blocks
 * and doesn't fit this codebase's thread-per-connection model). The underlying handle is
 * `expect`/`actual`: on POSIX `pthread_mutex_t` is an opaque struct, but on mingw winpthreads
 * represents it as a plain `Long`, confirmed by a real Windows CI compile error, not guessed.
 */
public class NativeMutex {
    @PublishedApi
    internal val handle: NativeMutexHandle = createNativeMutexHandle()

    public inline fun <T> withLock(block: () -> T): T {
        lockNativeMutex(handle)
        try {
            return block()
        } finally {
            unlockNativeMutex(handle)
        }
    }

    public fun destroy() {
        destroyNativeMutex(handle)
    }
}

@PublishedApi
internal expect class NativeMutexHandle

@PublishedApi
internal expect fun createNativeMutexHandle(): NativeMutexHandle

@PublishedApi
internal expect fun lockNativeMutex(handle: NativeMutexHandle)

@PublishedApi
internal expect fun unlockNativeMutex(handle: NativeMutexHandle)

internal expect fun destroyNativeMutex(handle: NativeMutexHandle)
