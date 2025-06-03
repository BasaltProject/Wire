package org.basaltmc.io

import kotlin.concurrent.AtomicInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.time.TimeSource

class TcpSocketTest {
    @Test
    fun bindAndListenSucceeds() {
        val listener = TcpListener.bind(port = 18080)
        listener.close()
    }

    @Test
    fun clientServerRoundTripsAMessage() {
        val port = 18081
        val listener = TcpListener.bind(port)

        NativeThread.start {
            val connection = listener.accept()
            val buffer = ByteArray(64)
            val read = connection.read(buffer)
            connection.write(buffer, length = read)
            connection.close()
        }

        val client = TcpConnection.connect("127.0.0.1", port)
        val message = "hello basalt".encodeToByteArray()
        client.write(message)

        val response = ByteArray(64)
        val received = client.read(response)
        client.close()
        listener.close()

        assertEquals(message.toList(), response.copyOf(received).toList())
    }

    @Test
    fun handlesManyConcurrentConnections() {
        val port = 18082
        val listener = TcpListener.bind(port, backlog = 256)
        val connectionCount = 100
        val acceptedCount = AtomicInt(0)

        NativeThread.start {
            repeat(connectionCount) {
                val connection = listener.accept()
                val buffer = ByteArray(16)
                val read = connection.read(buffer)
                connection.write(buffer, length = read)
                connection.close()
                acceptedCount.addAndGet(1)
            }
        }

        val start = TimeSource.Monotonic.markNow()
        repeat(connectionCount) { i ->
            val client = TcpConnection.connect("127.0.0.1", port)
            val payload = "msg$i".encodeToByteArray()
            client.write(payload)
            val response = ByteArray(16)
            val read = client.read(response)
            client.close()
            assertEquals(payload.toList(), response.copyOf(read).toList())
        }
        val elapsed = start.elapsedNow()

        // Every client round-trip already blocked until the server echoed back, so the server
        // thread is at most one close()+increment behind; give it a moment to catch up.
        val deadline = TimeSource.Monotonic.markNow()
        while (acceptedCount.value < connectionCount && deadline.elapsedNow().inWholeSeconds < 5) {
            // busy-wait briefly
        }

        listener.close()
        assertEquals(connectionCount, acceptedCount.value)
        assertTrue(elapsed.inWholeSeconds < 30, "handling $connectionCount connections took too long: $elapsed")
    }
}
