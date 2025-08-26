package org.basaltmc.wire.auth.forwarding

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.addJsonObject
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.put
import org.basaltmc.wire.auth.SessionProfileProperty
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * BungeeCord's IP forwarding, and the two schemes that bolt a shared secret onto it: the handshake
 * host carries `host`, address, undashed uuid and a JSON property array, NUL-separated.
 *
 * // parity: `net.md_5.bungee.connection.InitialHandler`, BungeeGuard's `BungeeCordHandshake` and
 * SafeNET's `Authenticator`.
 */
@OptIn(ExperimentalUuidApi::class)
public object LegacyForwarding {
    private const val SEPARATOR = '\u0000'

    /** Floodgate marks its own chunk in the split; the element after this one is not ours to read. */
    private const val FLOODGATE_V1 = "Geyser-Floodgate"
    private const val FLOODGATE_V2 = "^Floodgate^"

    /** Reads [host], checking the token [forwarding] asks for. The token is stripped: it is a password. */
    public fun decode(
        host: String,
        forwarding: ProxyForwarding,
    ): ForwardingResult {
        forwarding.misconfiguration()?.let { return ForwardingResult.Rejected(it) }
        val parts = split(host)
        if (parts.size < 3) {
            return ForwardingResult.Rejected(
                "the handshake carries no forwarded data (${parts.size} of at least 3 parts); " +
                    "either the proxy is not forwarding or somebody connected to this port directly",
            )
        }

        val serverHost = parts[0]
        val address = parts[1]
        val profileId =
            parseUuid(parts[2]) ?: return ForwardingResult.Rejected("forwarded uuid \"${parts[2]}\" is not a uuid")
        val properties =
            when (val fourth = parts.getOrNull(3)) {
                null, "" -> emptyList()
                else -> parseProperties(fourth) ?: return ForwardingResult.Rejected("forwarded properties are not JSON")
            }

        val kept =
            when (val checked = checkToken(properties, forwarding)) {
                is TokenCheck.Failed -> return ForwardingResult.Rejected(checked.reason)
                is TokenCheck.Passed -> checked.properties
            }
        return ForwardingResult.Accepted(ForwardedLogin(serverHost, address, profileId, name = null, properties = kept))
    }

    /** The host a proxy puts on the wire, the writing half of [decode]. The server never calls it. */
    public fun encode(
        host: String,
        address: String,
        profileId: Uuid,
        properties: List<SessionProfileProperty>,
        tokenProperty: String? = null,
        token: String? = null,
    ): String {
        val carried =
            if (tokenProperty != null && token != null) {
                properties + SessionProfileProperty(tokenProperty, token)
            } else {
                properties
            }
        val undashed = profileId.toHexString()
        return listOf(host, address, undashed, writeProperties(carried)).joinToString(SEPARATOR.toString())
    }

    /** The split, minus the chunks Floodgate inserts for the Bedrock players coming through Geyser. */
    private fun split(host: String): List<String> {
        val raw = host.split(SEPARATOR)
        val parts = ArrayList<String>(raw.size)
        var index = 0
        while (index < raw.size) {
            val part = raw[index]
            when {
                // The one that follows is Floodgate's own data, and skipping it keeps our indices right.
                part == FLOODGATE_V1 -> index++
                part.startsWith(FLOODGATE_V2) -> Unit
                else -> parts.add(part)
            }
            index++
        }
        return parts
    }

    private sealed interface TokenCheck {
        data class Passed(
            val properties: List<SessionProfileProperty>,
        ) : TokenCheck

        data class Failed(
            val reason: String,
        ) : TokenCheck
    }

    private fun checkToken(
        properties: List<SessionProfileProperty>,
        forwarding: ProxyForwarding,
    ): TokenCheck {
        if (forwarding.mode == ForwardingMode.BUNGEECORD) return TokenCheck.Passed(properties)
        val property = forwarding.tokenProperty
        val carried = properties.filter { it.name == property }
        // Exactly one: two of them is somebody trying one token per attempt inside a single login.
        if (carried.size != 1) {
            return TokenCheck.Failed(
                if (carried.isEmpty()) {
                    "the forwarded profile carries no \"$property\" property"
                } else {
                    "the forwarded profile carries ${carried.size} \"$property\" properties, expected one"
                },
            )
        }
        if (carried[0].value !in forwarding.tokens) {
            return TokenCheck.Failed("the \"$property\" property does not match any configured token")
        }
        return TokenCheck.Passed(properties.filter { it.name != property })
    }

    /** Accepts a uuid written with dashes or without, which is how BungeeCord sends it. */
    private fun parseUuid(written: String): Uuid? =
        runCatching {
            if (written.contains('-')) Uuid.parse(written) else Uuid.parseHex(written)
        }.getOrNull()

    private fun parseProperties(json: String): List<SessionProfileProperty>? =
        runCatching {
            (Json.parseToJsonElement(json) as JsonArray).map { element ->
                val property = element as JsonObject
                SessionProfileProperty(
                    name = (property["name"] as JsonPrimitive).content,
                    value = (property["value"] as JsonPrimitive).content,
                    // Bungee writes "" rather than leaving it out, and an empty signature is no signature.
                    signature = (property["signature"] as? JsonPrimitive)?.content?.takeIf { it.isNotEmpty() },
                )
            }
        }.getOrNull()

    private fun writeProperties(properties: List<SessionProfileProperty>): String =
        buildJsonArray {
            for (property in properties) {
                addJsonObject {
                    put("name", property.name)
                    put("value", property.value)
                    put("signature", property.signature.orEmpty())
                }
            }
        }.toString()
}
