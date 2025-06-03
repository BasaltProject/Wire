@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.wire.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.sizeOf
import platform.posix.AF_INET
import platform.posix.sockaddr_in

internal actual fun sockaddr_in.setAddressFamily() {
    sin_len = sizeOf<sockaddr_in>().toUByte()
    sin_family = AF_INET.toUByte()
}
