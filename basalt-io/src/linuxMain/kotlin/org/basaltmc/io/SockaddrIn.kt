@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import platform.posix.AF_INET
import platform.posix.sockaddr_in

/**
 * Linux's `sockaddr_in` has no BSD-style `sin_len`; `sin_family` alone is a 16-bit `sa_family_t`.
 * Unverified against a real Linux build locally (this repo only builds macOS targets on this
 * host); written from the known glibc `sockaddr_in` layout and the exact CI compile error this
 * replaces (`Assignment type mismatch: actual type is 'UByte', but 'UShort' was expected` on
 * `sin_family`), not guessed. Confirm on the next Linux CI run.
 */
internal actual fun sockaddr_in.setAddressFamily() {
    sin_family = AF_INET.toUShort()
}
