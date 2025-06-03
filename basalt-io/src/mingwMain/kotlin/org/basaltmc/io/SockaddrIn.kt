@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.IntVar
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.value
import platform.posix.AF_INET
import platform.posix.getpeername
import platform.posix.sockaddr
import platform.posix.sockaddr_in

/**
 * Windows' `sockaddr_in` has no BSD-style `sin_len` either; `sin_family` is a 16-bit `ADDRESS_FAMILY`.
 * Unverified: this repo builds only macOS targets locally, and CI is not currently running.
 * Written from the known Winsock `sockaddr_in` layout, not guessed blind: confirm on the next
 * Windows CI run.
 */
internal actual fun sockaddr_in.setAddressFamily() {
    sin_family = AF_INET.convert()
}

/**
 * Winsock's `in_addr` exposes the raw 32-bit address through a nested union (`S_un.S_addr`), not
 * the `s_addr` name: that's a `#define s_addr S_un.S_addr` macro in the real header, which
 * cinterop does not turn into a Kotlin property since it only extracts simple constant macros.
 * `S_un` is a named (not anonymous) union member, so cinterop should still generate it as a
 * regular nested property. Unverified for the same reason as [setAddressFamily] above.
 */
internal actual fun sockaddr_in.setAddress(value: UInt) {
    sin_addr.S_un.S_addr = value
}

/**
 * Winsock's `getpeername` takes its length as `int*`, not POSIX's `socklen_t*`, which is the whole
 * reason this is per-platform. Unverified for the same reason as the two above: confirm on the next
 * Windows CI run. A wrong answer here costs the server list greeting and nothing else, since the
 * address is never used to decide anything.
 */
internal actual fun peerAddress(fd: Long): UInt? =
    memScoped {
        val address = alloc<sockaddr_in>()
        val length = alloc<IntVar>()
        length.value = sizeOf<sockaddr_in>().convert()
        if (getpeername(fd.convert(), address.ptr.reinterpret<sockaddr>(), length.ptr) != 0) {
            return@memScoped null
        }
        if (address.sin_family.toInt() != AF_INET) return@memScoped null
        address.sin_addr.S_un.S_addr
    }
