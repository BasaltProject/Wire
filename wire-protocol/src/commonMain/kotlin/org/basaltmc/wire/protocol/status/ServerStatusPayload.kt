package org.basaltmc.wire.protocol.status

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import kotlinx.serialization.json.putJsonObject
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentJson
import kotlin.io.encoding.Base64
import kotlin.io.encoding.ExperimentalEncodingApi

/**
 * The JSON body of the Server List Ping response, matching vanilla's
 * `net.minecraft.network.protocol.status.ServerStatus` record (confirmed against the decompiled
 * 26.2 server jar): `description`/`players`/`version`/`favicon`/`enforcesSecureChat`, all
 * lenient-optional except `description`.
 */
public data class ServerStatusPayload(
    val description: Component,
    val players: Players? = null,
    val version: Version? = null,
    val favicon: ByteArray? = null,
    val enforcesSecureChat: Boolean = false,
) {
    public data class Players(
        val max: Int,
        val online: Int,
        val sample: List<SamplePlayer> = emptyList(),
    )

    public data class SamplePlayer(
        val name: String,
        val id: String,
    )

    public data class Version(
        val name: String,
        val protocol: Int,
    )

    @OptIn(ExperimentalEncodingApi::class)
    public fun toJsonString(): String {
        val json =
            buildJsonObject {
                put("description", ComponentJson.serialize(description))
                players?.let { players ->
                    putJsonObject("players") {
                        put("max", players.max)
                        put("online", players.online)
                        putJsonArray("sample") {
                            players.sample.forEach { sample ->
                                addJsonObject {
                                    put("name", sample.name)
                                    put("id", sample.id)
                                }
                            }
                        }
                    }
                }
                version?.let { version ->
                    putJsonObject("version") {
                        put("name", version.name)
                        put("protocol", version.protocol)
                    }
                }
                favicon?.let { favicon ->
                    put("favicon", "data:image/png;base64," + Base64.encode(favicon))
                }
                put("enforcesSecureChat", enforcesSecureChat)
            }
        return json.toString()
    }

    public companion object {
        @OptIn(ExperimentalEncodingApi::class)
        public fun fromJsonString(text: String): ServerStatusPayload {
            val obj = Json.parseToJsonElement(text) as JsonObject
            val description = (obj["description"])?.let { ComponentJson.deserialize(it) } ?: Component.empty()
            val players =
                (obj["players"] as? JsonObject)?.let { p ->
                    Players(
                        max = (p["max"] as JsonPrimitive).content.toInt(),
                        online = (p["online"] as JsonPrimitive).content.toInt(),
                        sample =
                            (p["sample"] as? JsonArray)?.map { entry ->
                                val entryObj = entry as JsonObject
                                SamplePlayer(
                                    name = (entryObj["name"] as JsonPrimitive).content,
                                    id = (entryObj["id"] as JsonPrimitive).content,
                                )
                            } ?: emptyList(),
                    )
                }
            val version =
                (obj["version"] as? JsonObject)?.let { v ->
                    Version(
                        name = (v["name"] as JsonPrimitive).content,
                        protocol = (v["protocol"] as JsonPrimitive).content.toInt(),
                    )
                }
            val favicon =
                (obj["favicon"] as? JsonPrimitive)?.content?.let { dataUrl ->
                    val prefix = "data:image/png;base64,"
                    if (dataUrl.startsWith(prefix)) Base64.decode(dataUrl.removePrefix(prefix)) else null
                }
            val enforcesSecureChat =
                (obj["enforcesSecureChat"] as? JsonPrimitive)?.content?.toBooleanStrictOrNull() ?: false
            return ServerStatusPayload(description, players, version, favicon, enforcesSecureChat)
        }
    }
}

private fun kotlinx.serialization.json.JsonArrayBuilder.addJsonObject(
    block: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit,
) {
    add(buildJsonObject(block))
}
