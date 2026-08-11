package org.basaltmc.auth

import io.ktor.client.HttpClient
import io.ktor.client.engine.mock.MockEngine
import io.ktor.client.engine.mock.respond
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpStatusCode
import io.ktor.http.Parameters
import io.ktor.http.headersOf
import io.ktor.utils.io.ByteReadChannel
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

class MojangSessionServiceTest {
    private fun clientReturning(
        status: HttpStatusCode,
        body: String = "",
    ): HttpClient {
        val mockEngine =
            MockEngine {
                respond(
                    content = ByteReadChannel(body),
                    status = status,
                    headers = headersOf(HttpHeaders.ContentType, "application/json"),
                )
            }
        return HttpClient(mockEngine)
    }

    @Test
    @OptIn(ExperimentalUuidApi::class)
    fun parsesASuccessfulResponseWithProperties() {
        val body =
            """
            {
                "id": "069a79f444e94726a5befca90e38aaf5",
                "name": "Steve",
                "properties": [
                    { "name": "textures", "value": "base64stuff", "signature": "sig" },
                    { "name": "unsigned", "value": "value" }
                ]
            }
            """.trimIndent()
        val service = MojangSessionService(clientReturning(HttpStatusCode.OK, body))

        val profile = service.hasJoined("Steve", "deadbeef", clientAddress = null)

        assertEquals(Uuid.parse("069a79f4-44e9-4726-a5be-fca90e38aaf5"), profile?.id)
        assertEquals("Steve", profile?.name)
        assertEquals(
            listOf(
                SessionProfileProperty("textures", "base64stuff", "sig"),
                SessionProfileProperty("unsigned", "value"),
            ),
            profile?.properties,
        )
    }

    @Test
    fun parsesAResponseWithNoProperties() {
        val body = """{"id": "069a79f444e94726a5befca90e38aaf5", "name": "Steve"}"""
        val service = MojangSessionService(clientReturning(HttpStatusCode.OK, body))

        val profile = service.hasJoined("Steve", "deadbeef", clientAddress = null)

        assertTrue(profile?.properties?.isEmpty() == true)
    }

    @Test
    fun returnsNullWhenMojangReportsNoContent() {
        // The real API's response for an invalid/unauthenticated session.
        val service = MojangSessionService(clientReturning(HttpStatusCode.NoContent))

        val profile = service.hasJoined("NotARealSession", "deadbeef", clientAddress = null)

        assertNull(profile)
    }

    @Test
    fun sendsUsernameServerHashAndOptionalIpAsQueryParameters() {
        var capturedParams: Parameters? = null
        val mockEngine =
            MockEngine { request ->
                capturedParams = request.url.parameters
                respond(
                    content = ByteReadChannel("""{"id": "069a79f444e94726a5befca90e38aaf5", "name": "Steve"}"""),
                    status = HttpStatusCode.OK,
                    headers = headersOf(HttpHeaders.ContentType, "application/json"),
                )
            }
        val service = MojangSessionService(HttpClient(mockEngine))

        service.hasJoined("Steve", "-abc123", clientAddress = "203.0.113.5")

        assertEquals("Steve", capturedParams?.get("username"))
        assertEquals("-abc123", capturedParams?.get("serverId"))
        assertEquals("203.0.113.5", capturedParams?.get("ip"))
    }

    @Test
    fun omitsIpParameterWhenNoClientAddressIsGiven() {
        var capturedParams: Parameters? = null
        val mockEngine =
            MockEngine { request ->
                capturedParams = request.url.parameters
                respond(
                    content = ByteReadChannel("""{"id": "069a79f444e94726a5befca90e38aaf5", "name": "Steve"}"""),
                    status = HttpStatusCode.OK,
                    headers = headersOf(HttpHeaders.ContentType, "application/json"),
                )
            }
        val service = MojangSessionService(HttpClient(mockEngine))

        service.hasJoined("Steve", "-abc123", clientAddress = null)

        assertNull(capturedParams?.get("ip"))
    }
}
