package org.basaltmc.io

import kotlin.test.Test
import kotlin.time.TimeSource

/**
 * Micro-benchmark for [ByteBuffer], not a correctness test: reports throughput to stdout so a
 * future change to the hot read/write path (packet codecs are built on this buffer) has a
 * number to compare against. No JMH on Kotlin/Native, so this is a plain wall-clock loop with no
 * pass/fail threshold; test binaries here are unoptimized debug builds, so treat the numbers as
 * indicative of relative regressions, not absolute performance.
 */
class ByteBufferBenchmark {
    @Test
    fun reportIntReadWriteThroughput() {
        val iterations = 500_000
        val buffer = ByteBuffer(iterations * 4)

        val writeStart = TimeSource.Monotonic.markNow()
        repeat(iterations) { buffer.writeInt(it) }
        val writeElapsed = writeStart.elapsedNow()

        val readStart = TimeSource.Monotonic.markNow()
        repeat(iterations) { buffer.readInt() }
        val readElapsed = readStart.elapsedNow()

        println("ByteBuffer.writeInt: $iterations ops in $writeElapsed")
        println("ByteBuffer.readInt: $iterations ops in $readElapsed")
    }

    @Test
    fun reportBulkBytesThroughput() {
        val chunkSize = 4096
        val chunks = 5_000
        val payload = ByteArray(chunkSize) { it.toByte() }
        val buffer = ByteBuffer(chunkSize * chunks)

        val writeStart = TimeSource.Monotonic.markNow()
        repeat(chunks) { buffer.writeBytes(payload) }
        val writeElapsed = writeStart.elapsedNow()

        val readStart = TimeSource.Monotonic.markNow()
        repeat(chunks) { buffer.readBytes(chunkSize) }
        val readElapsed = readStart.elapsedNow()

        val totalKiB = (chunkSize.toLong() * chunks) / 1024
        println("ByteBuffer.writeBytes: ${totalKiB}KiB in $writeElapsed")
        println("ByteBuffer.readBytes: ${totalKiB}KiB in $readElapsed")
    }
}
