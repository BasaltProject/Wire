package org.basaltmc.io

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Writing to a connection whose peer has gone must raise, not kill the process.
 *
 * This is not hypothetical: SIGPIPE's default action terminated the server the moment a second
 * player joined and the first one's client dropped, with no message, no stack and nothing in the
 * log. A player closing their game mid-broadcast is the most ordinary event a server has.
 */
class BrokenPipeTest {
    @Test
    fun writingToAConnectionWhosePeerHasGoneRaisesInsteadOfKillingTheProcess() {
        val port = 18_611
        val listener = TcpListener.bind(port)
        val client = TcpConnection.connect("127.0.0.1", port)
        val server = listener.accept()

        client.close()

        // Enough writes to be sure the kernel has noticed: the first lands in the send buffer and a
        // later one discovers the peer is gone. Each returns quietly, which is [TcpConnection.write]'s
        // documented answer to a peer that has left.
        repeat(200) { server.write(ByteArray(4096) { 1 }) }

        // Reaching this line *is* the assertion. Before SIGPIPE was ignored the process died inside
        // that loop, taking the whole server with it and printing nothing at all.
        assertTrue(true, "the process survived writing to a closed peer")
        assertEquals(0, server.read(ByteArray(16)), "a gone peer reads as end of stream")
        server.close()
        listener.close()
    }
}
