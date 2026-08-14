@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import platform.linux.RUSAGE_SELF
import platform.linux.getrusage
import platform.linux.rusage
import platform.posix._SC_NPROCESSORS_ONLN
import platform.posix._SC_PAGESIZE
import platform.posix.sysconf

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

/** Cores currently online, which is what a pool should be sized against rather than what exists. */
public actual fun cpuCoreCount(): Int? = sysconf(_SC_NPROCESSORS_ONLN).toInt().takeIf { it > 0 }

/** Field two of `/proc/self/statm`, resident pages, times the page size: what `top` calls RES. */
public actual fun processResidentBytes(): Long? {
    val statm = FileIO.readText("/proc/self/statm") ?: return null
    val residentPages = statm.split(' ').getOrNull(1)?.toLongOrNull() ?: return null
    val pageSize = sysconf(_SC_PAGESIZE)
    if (pageSize <= 0) return null
    return residentPages * pageSize
}
