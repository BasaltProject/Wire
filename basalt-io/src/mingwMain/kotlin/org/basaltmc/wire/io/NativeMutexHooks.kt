@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.LongVar
import kotlinx.cinterop.alloc
import kotlinx.cinterop.free
import kotlinx.cinterop.nativeHeap
import kotlinx.cinterop.ptr
import platform.posix.pthread_mutex_destroy
import platform.posix.pthread_mutex_init
import platform.posix.pthread_mutex_lock
import platform.posix.pthread_mutex_unlock

/** winpthreads represents `pthread_mutex_t` as a plain `Long` handle here, not the opaque struct POSIX uses. */
@PublishedApi
internal actual class NativeMutexHandle(
    val value: LongVar,
)

@PublishedApi
internal actual fun createNativeMutexHandle(): NativeMutexHandle {
    val value = nativeHeap.alloc<LongVar>()
    pthread_mutex_init(value.ptr, null)
    return NativeMutexHandle(value)
}

@PublishedApi
internal actual fun lockNativeMutex(handle: NativeMutexHandle) {
    pthread_mutex_lock(handle.value.ptr)
}

@PublishedApi
internal actual fun unlockNativeMutex(handle: NativeMutexHandle) {
    pthread_mutex_unlock(handle.value.ptr)
}

internal actual fun destroyNativeMutex(handle: NativeMutexHandle) {
    pthread_mutex_destroy(handle.value.ptr)
    nativeHeap.free(handle.value)
}
