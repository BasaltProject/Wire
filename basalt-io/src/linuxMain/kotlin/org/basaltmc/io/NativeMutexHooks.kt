@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.free
import kotlinx.cinterop.nativeHeap
import kotlinx.cinterop.ptr
import platform.posix.pthread_mutex_destroy
import platform.posix.pthread_mutex_init
import platform.posix.pthread_mutex_lock
import platform.posix.pthread_mutex_t
import platform.posix.pthread_mutex_unlock

@PublishedApi
internal actual class NativeMutexHandle(
    val value: pthread_mutex_t,
)

@PublishedApi
internal actual fun createNativeMutexHandle(): NativeMutexHandle {
    val value = nativeHeap.alloc<pthread_mutex_t>()
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
