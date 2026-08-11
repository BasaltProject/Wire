package org.basaltmc.io

import platform.posix.mkdir

/**
 * mingw-w64's `platform.posix.mkdir` compat shim takes only a path (matching MSVCRT's `_mkdir`),
 * not the POSIX `(path, mode)` pair: unlike `select()`/`setsockopt`, this one's the *same* function
 * name across targets but a different arity, so it needs its own `actual` rather than a shared one.
 */
internal actual fun createDirectory(path: String) {
    mkdir(path)
}
