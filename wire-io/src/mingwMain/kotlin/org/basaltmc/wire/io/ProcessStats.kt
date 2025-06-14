@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.sizeOf
import platform.psapi.K32GetProcessMemoryInfo
import platform.psapi.PROCESS_MEMORY_COUNTERS
import platform.windows.FILETIME
import platform.windows.GetCurrentProcess
import platform.windows.GetProcessTimes
import platform.windows.GetSystemInfo
import platform.windows.SYSTEM_INFO

/**
 * `GetProcessTimes` rather than `getrusage`, which Windows does not have.
 *
 * Its two `FILETIME`s count in 100-nanosecond ticks since the process started, so kernel plus user
 * divided by ten is the same microseconds the POSIX side reports. Unverified for the same reason as
 * the other Windows actuals here: this repo only builds macOS targets locally. A wrong answer costs
 * one number in the tab list and nothing else.
 */
public actual fun processCpuMicros(): Long? =
    memScoped {
        val creation = alloc<FILETIME>()
        val exit = alloc<FILETIME>()
        val kernel = alloc<FILETIME>()
        val user = alloc<FILETIME>()
        if (GetProcessTimes(GetCurrentProcess(), creation.ptr, exit.ptr, kernel.ptr, user.ptr) == 0) {
            return@memScoped null
        }
        (ticks(kernel) + ticks(user)) / 10L
    }

private fun ticks(time: FILETIME): Long =
    (time.dwHighDateTime.toLong() shl 32) or (time.dwLowDateTime.toLong() and 0xFFFFFFFFL)

/** `GetSystemInfo`, Windows having no `sysconf`. Unverified locally, like the rest of this file. */
public actual fun cpuCoreCount(): Int? =
    memScoped {
        val info = alloc<SYSTEM_INFO>()
        GetSystemInfo(info.ptr)
        info.dwNumberOfProcessors.toInt().takeIf { it > 0 }
    }

/** `GetProcessMemoryInfo`'s working set, Task Manager's own memory column. Unverified locally. */
public actual fun processResidentBytes(): Long? =
    memScoped {
        val counters = alloc<PROCESS_MEMORY_COUNTERS>()
        counters.cb = sizeOf<PROCESS_MEMORY_COUNTERS>().toUInt()
        val result = K32GetProcessMemoryInfo(GetCurrentProcess(), counters.ptr, counters.cb)
        if (result != 0) counters.WorkingSetSize.toLong() else null
    }
