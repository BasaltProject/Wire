@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import platform.posix.RUSAGE_SELF
import platform.posix.getrusage
import platform.posix.rusage

/** User plus system time, which together are what the process actually cost the machine. */
public actual fun processCpuMicros(): Long? =
    memScoped {
        val usage = alloc<rusage>()
        if (getrusage(RUSAGE_SELF, usage.ptr) != 0) return@memScoped null
        val user = usage.ru_utime.tv_sec * 1_000_000L + usage.ru_utime.tv_usec
        val system = usage.ru_stime.tv_sec * 1_000_000L + usage.ru_stime.tv_usec
        user + system
    }
