@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.value
import platform.darwin.KERN_SUCCESS
import platform.darwin.MACH_TASK_BASIC_INFO
import platform.darwin.integer_tVar
import platform.darwin.mach_msg_type_number_tVar
import platform.darwin.mach_task_basic_info
import platform.darwin.mach_task_self_
import platform.darwin.task_info
import platform.posix.RUSAGE_SELF
import platform.posix._SC_NPROCESSORS_ONLN
import platform.posix.getrusage
import platform.posix.rusage
import platform.posix.sysconf

/** User plus system time, which together are what the process actually cost the machine. */
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

/** `task_info(MACH_TASK_BASIC_INFO)`, the same source Activity Monitor reads its memory column from. */
public actual fun processResidentBytes(): Long? =
    memScoped {
        val info = alloc<mach_task_basic_info>()
        val count = alloc<mach_msg_type_number_tVar>()
        count.value = (sizeOf<mach_task_basic_info>() / sizeOf<integer_tVar>()).toUInt()
        val result =
            task_info(
                mach_task_self_,
                MACH_TASK_BASIC_INFO.toUInt(),
                info.ptr.reinterpret(),
                count.ptr,
            )
        if (result == KERN_SUCCESS) info.resident_size.toLong() else null
    }
