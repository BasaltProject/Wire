@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.COpaquePointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.StableRef
import kotlinx.cinterop.alloc
import kotlinx.cinterop.asStableRef
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.staticCFunction
import kotlinx.cinterop.value
import platform.posix.pthread_create
import platform.posix.pthread_detach
import platform.posix.pthread_tVar

/**
 * Fire-and-forget OS thread, backing the thread-per-connection model `TcpListener`/
 * `TcpConnection` are meant to be driven from until a real event loop replaces it (see
 * `TcpSocket.kt`). A thin wrapper over `pthread_create`, since Kotlin/Native's stdlib does not
 * expose native thread spawning directly.
 *
 * An exception escaping [block] is caught rather than left to cross back into the C callback
 * boundary: Kotlin/Native terminates the whole process when that happens, which would mean one
 * connection's failure (a bad handshake, a bind failure) takes the entire server down with it.
 *
 * Reported with its stack: the thread is gone either way, and a bare name says nothing about where.
 */
public object NativeThread {
    private val nextId = kotlin.concurrent.AtomicLong(0)

    /**
     * A number for the calling thread, stable for its whole life and unique on this process.
     *
     * Handed out on first ask rather than read from the OS: `pthread_t` is a pointer on some of the
     * five targets and an integer on others, and nothing here needs the OS's own number, only one
     * that tells two threads apart. Whoever owns a piece of state compares this.
     */
    public fun currentId(): Long {
        if (ThreadIdentity.id == 0L) ThreadIdentity.id = nextId.addAndGet(1)
        return ThreadIdentity.id
    }

    public fun start(block: () -> Unit) {
        val stableRef = StableRef.create(block)
        memScoped {
            val threadHandle = alloc<pthread_tVar>()
            val result =
                pthread_create(
                    threadHandle.ptr,
                    null,
                    staticCFunction { arg: COpaquePointer? ->
                        val ref = arg!!.asStableRef<() -> Unit>()
                        val fn = ref.get()
                        ref.dispose()
                        try {
                            fn()
                        } catch (e: Throwable) {
                            println("Uncaught exception on a native thread: " + e.stackTraceToString())
                        }
                        null
                    },
                    stableRef.asCPointer(),
                )
            if (result != 0) {
                stableRef.dispose()
                throw SocketException("pthread_create failed with code $result")
            }
            pthread_detach(threadHandle.value)
        }
    }
}

@kotlin.native.concurrent.ThreadLocal
private object ThreadIdentity {
    var id: Long = 0
}
