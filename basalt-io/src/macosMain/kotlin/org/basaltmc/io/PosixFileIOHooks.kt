@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.convert
import platform.posix.mkdir

internal actual fun createDirectory(path: String) {
    mkdir(path, 0x1FF.convert())
}
