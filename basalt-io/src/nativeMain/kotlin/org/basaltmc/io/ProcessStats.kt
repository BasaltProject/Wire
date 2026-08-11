package org.basaltmc.io

/**
 * CPU time this process has used, in microseconds, or null where the platform cannot say.
 *
 * Time used, not a percentage: a percentage only means something between two readings, and deciding
 * over what window is the caller's business, not this one's. Lives in this module because it is the
 * only one with per-platform source sets, and this is a syscall like every other one in here.
 */
public expect fun processCpuMicros(): Long?
