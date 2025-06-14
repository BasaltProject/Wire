package org.basaltmc.wire.io

/**
 * CPU time this process has used, in microseconds, or null where the platform cannot say.
 *
 * Time used, not a percentage: a percentage only means something between two readings, and deciding
 * over what window is the caller's business, not this one's. Lives in this module because it is the
 * only one with per-platform source sets, and this is a syscall like every other one in here.
 */
public expect fun processCpuMicros(): Long?

/**
 * Cores this process may run on, or null where the platform cannot say. For sizing worker pools:
 * a constant is what puts eight generation threads on a four-core machine.
 */
public expect fun cpuCoreCount(): Int?

/**
 * Physical memory this process occupies right now, in bytes, or null where the platform cannot
 * say. The number an operator's own `top` shows, as opposed to any runtime-internal heap counter.
 */
public expect fun processResidentBytes(): Long?
