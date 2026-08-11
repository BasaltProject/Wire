@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import platform.posix.fd_set
import platform.posix.select
import platform.posix.timeval

internal actual fun rawSelect(
    maxFd: Int,
    readSet: CPointer<fd_set>,
    timeout: CPointer<timeval>?,
): Int = select(maxFd, readSet, null, null, timeout)
