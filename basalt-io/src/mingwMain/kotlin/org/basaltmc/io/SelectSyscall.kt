@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import platform.posix.fd_set
import platform.posix.timeval
import platform.windows.select

/** `select` isn't bound under `platform.posix` on mingw; only `platform.windows` (from `winsock2.h`) has it. */
internal actual fun rawSelect(
    maxFd: Int,
    readSet: CPointer<fd_set>,
    timeout: CPointer<timeval>?,
): Int = select(maxFd, readSet, null, null, timeout)
