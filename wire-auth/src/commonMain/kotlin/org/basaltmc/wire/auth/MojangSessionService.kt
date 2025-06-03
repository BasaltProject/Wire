package org.basaltmc.wire.auth

import io.ktor.client.HttpClient
import io.ktor.client.request.get
import io.ktor.client.request.parameter
import io.ktor.client.statement.bodyAsText
import io.ktor.http.HttpStatusCode
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Real [SessionService], backed by Mojang's session server.
 *
 * // parity: `YggdrasilMinecraftSessionService.hasJoinedServer`. `200` + JSON body means valid;
 * anything else (Mojang uses `204`) means invalid.
 *
 * Parses JSON by hand, matching `ServerStatusPayload`/`ComponentJson` elsewhere in this project,
 * which don't use the `kotlinx-serialization` compiler plugin either. [hasJoined] blocks via
 * [runBlocking] since [SessionService] is a plain synchronous interface. [client] is injectable
 * so tests can use Ktor's `MockEngine`.
 */
public class MojangSessionService(
    private val client: HttpClient = HttpClient(),
) : SessionService {
    @OptIn(ExperimentalUuidApi::class)
    override fun hasJoined(
        username: String,
        serverHash: String,
        clientAddress: String?,
    ): SessionProfile? =
        runBlocking {
            val response =
                client.get("https://sessionserver.mojang.com/session/minecraft/hasJoined") {
                    parameter("username", username)
                    parameter("serverId", serverHash)
                    clientAddress?.let { parameter("ip", it) }
                }
            if (response.status != HttpStatusCode.OK) return@runBlocking null

            val body = Json.parseToJsonElement(response.bodyAsText()) as JsonObject
            val properties =
                (body["properties"] as? JsonArray)?.map { element ->
                    val property = element as JsonObject
                    SessionProfileProperty(
                        name = (property["name"] as JsonPrimitive).content,
                        value = (property["value"] as JsonPrimitive).content,
                        signature = (property["signature"] as? JsonPrimitive)?.content,
                    )
                } ?: emptyList()

            SessionProfile(
                id = Uuid.parse((body["id"] as JsonPrimitive).content),
                name = (body["name"] as JsonPrimitive).content,
                properties = properties,
            )
        }
}
