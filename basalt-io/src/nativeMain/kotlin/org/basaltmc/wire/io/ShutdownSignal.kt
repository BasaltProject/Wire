@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.staticCFunction
import platform.posix.SIGINT
import platform.posix.SIGTERM
import platform.posix.signal
import kotlin.concurrent.AtomicInt

/**
 * Turns `Ctrl+C` and `kill` into an orderly shutdown instead of an abrupt death, saving up to a
 * whole save interval of every player. `SIGINT` is the operator, `SIGTERM` is what `systemd`,
 * Docker and every supervisor send before `SIGKILL`.
 *
 * The handler only sets a flag, since anything else it could do risks deadlocking against the
 * interrupted thread. [install] touches [requested] first, so the handler never has to initialize
 * it.
 */
public object ShutdownSignal {
    private val requested = AtomicInt(0)

    /** True once a termination signal has arrived. Polled; never acted on from the handler itself. */
    public val isRequested: Boolean get() = requested.value != 0

    public fun install() {
        // Touches the flag before installing, so it is already initialized when a signal lands.
        requested.value = 0
        val handler = staticCFunction<Int, Unit> { _ -> ShutdownSignal.markRequested() }
        signal(SIGINT, handler)
        signal(SIGTERM, handler)
    }

    private fun markRequested() {
        requested.value = 1
    }
}
