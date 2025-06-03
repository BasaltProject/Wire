package org.basaltmc.wire.io

import kotlin.concurrent.AtomicInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.time.TimeSource

private const val TERMINATOR: Byte = '\n'.code.toByte()

/** Reads until [TERMINATOR], since a single `read()` isn't guaranteed to return a whole message. */
private fun readLine(connection: TcpConnection): ByteArray {
    val out = ArrayList<Byte>(32)
    val chunk = ByteArray(1)
    while (true) {
        val read = connection.read(chunk)
        if (read <= 0 || chunk[0] == TERMINATOR) break
        out.add(chunk[0])
    }
    return out.toByteArray()
}

class SelectEventLoopTest {
    @Test
    fun detectsReadinessOnAConnection() {
        val port = 18090
        val listener = TcpListener.bind(port)
        val eventLoop = SelectEventLoop()

        NativeThread.start {
            val connection = listener.accept()
            eventLoop.register(connection.nativeFd)
            val ready = eventLoop.poll(timeoutMillis = 5000)
            assertEquals(listOf(connection.nativeFd), ready)
            val line = readLine(connection)
            connection.write(line + TERMINATOR)
            connection.close()
        }

        val client = TcpConnection.connect("127.0.0.1", port)
        client.write("hi".encodeToByteArray() + TERMINATOR)
        val response = readLine(client)
        client.close()
        listener.close()
        assertEquals("hi", response.decodeToString())
    }

    /** Stress test for the readiness mechanism itself, not a rewrite of `ClientSession`/`BasaltServer`. */
    @Test
    fun echoesManyConcurrentConnectionsThroughASingleEventLoopThread() {
        val port = 18091
        // one slot goes to the listener socket itself; cap adapts to select()'s real per-platform limit
        val connectionCount = (SelectEventLoop.maxWatchableFds - 1).coerceAtMost(1000)
        val listener = TcpListener.bind(port, backlog = connectionCount)
        val eventLoop = SelectEventLoop()
        eventLoop.register(listener.nativeFd)

        val handledCount = AtomicInt(0)
        val connections = HashMap<Int, TcpConnection>()

        NativeThread.start {
            while (handledCount.value < connectionCount) {
                val ready = eventLoop.poll(timeoutMillis = 10_000)
                if (ready.isEmpty()) break
                for (fd in ready) {
                    if (fd == listener.nativeFd) {
                        val connection = listener.accept()
                        connections[connection.nativeFd] = connection
                        eventLoop.register(connection.nativeFd)
                    } else {
                        val connection = connections.remove(fd) ?: continue
                        eventLoop.unregister(fd)
                        val line = readLine(connection)
                        connection.write(line + TERMINATOR)
                        connection.close()
                        handledCount.addAndGet(1)
                    }
                }
            }
        }

        val succeeded = AtomicInt(0)
        val clientsDone = AtomicInt(0)
        val start = TimeSource.Monotonic.markNow()
        val clientThreads =
            List(connectionCount) { i ->
                NativeThread.start {
                    val payload = "c$i".encodeToByteArray()
                    val client = TcpConnection.connect("127.0.0.1", port)
                    client.write(payload + TERMINATOR)
                    val response = readLine(client)
                    client.close()
                    if (payload.contentEquals(response)) succeeded.addAndGet(1)
                    clientsDone.addAndGet(1)
                }
            }
        check(clientThreads.size == connectionCount)

        val deadline = TimeSource.Monotonic.markNow()
        // wait for both sides: the server writing every response and every client reading its own
        while ((handledCount.value < connectionCount || clientsDone.value < connectionCount) &&
            deadline.elapsedNow().inWholeSeconds < 30
        ) {
            // busy-wait for the event-loop thread and all client threads to finish
        }
        val elapsed = start.elapsedNow()

        listener.close()
        assertEquals(connectionCount, handledCount.value, "server did not handle every connection")
        assertEquals(connectionCount, succeeded.value, "not every client got back its own echoed payload")
        assertTrue(elapsed.inWholeSeconds < 30, "handling $connectionCount connections took too long: $elapsed")
    }
}
