@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import platform.posix.nanosleep
import platform.posix.timespec

/**
 * Blocks the calling thread for [millis]. `nanosleep` rather than `usleep`: both are POSIX, but
 * `nanosleep` is declared in `time.h`, which (unlike `unistd.h`, where `usleep` lives) is bundled
 * identically on every Basalt target including mingw, the same reasoning `select()` needed
 * `platform.windows` for.
 */
public fun sleepMillis(millis: Long) {
    memScoped {
        val spec = alloc<timespec>()
        spec.tv_sec = (millis / 1000).convert()
        spec.tv_nsec = ((millis % 1000) * 1_000_000).convert()
        nanosleep(spec.ptr, null)
    }
}
