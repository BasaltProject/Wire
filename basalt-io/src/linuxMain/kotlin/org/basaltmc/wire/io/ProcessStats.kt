@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import platform.linux.RUSAGE_SELF
import platform.linux.getrusage
import platform.linux.rusage

/**
 * User plus system time, which together are what the process actually cost the machine.
 * glibc binds `getrusage` under `platform.linux`, not `platform.posix` as the Apple headers do.
 */
public actual fun processCpuMicros(): Long? =
    memScoped {
        val usage = alloc<rusage>()
        if (getrusage(RUSAGE_SELF, usage.ptr) != 0) return@memScoped null
        val user = usage.ru_utime.tv_sec * 1_000_000L + usage.ru_utime.tv_usec
        val system = usage.ru_stime.tv_sec * 1_000_000L + usage.ru_stime.tv_usec
        user + system
    }
