package org.basaltmc.wire.auth.forwarding

import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.algorithms.HMAC
import dev.whyoleg.cryptography.algorithms.SHA256
import org.basaltmc.wire.auth.SessionProfileProperty
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readUuid
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeUuid
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Velocity's modern forwarding: an HMAC-SHA256 tag over a version, the player's address, uuid, name
 * and properties. Versions 2 and 3 append a chat signing key, whose bytes are ignored, not refused.
 */
@OptIn(ExperimentalUuidApi::class)
public object VelocityForwarding {
    /** The channel the query goes out on. // parity of name with Velocity's `PlayerDataForwarding.CHANNEL`. */
    public const val CHANNEL: String = "velocity:player_info"

    /** The tag is a fixed-size SHA-256 HMAC, so the payload starts right after it. */
    private const val SIGNATURE_LENGTH = 32

    public const val VERSION_DEFAULT: Int = 1
    public const val VERSION_WITH_KEY: Int = 2
    public const val VERSION_WITH_KEY_V2: Int = 3
    public const val VERSION_LAZY_SESSION: Int = 4

    /** The newest forwarding this server understands, which is what the query asks for. */
    public const val MAX_SUPPORTED_VERSION: Int = VERSION_LAZY_SESSION

    /** What goes in the login query: one byte naming the newest version we can read. */
    public fun request(): ByteArray = byteArrayOf(MAX_SUPPORTED_VERSION.toByte())

    /** Reads a proxy's answer, signature first: parsing an unsigned payload would be answering a guess. */
    public fun decode(
        data: ByteArray,
        forwarding: ProxyForwarding,
    ): ForwardingResult {
        forwarding.misconfiguration()?.let { return ForwardingResult.Rejected(it) }
        if (data.size <= SIGNATURE_LENGTH) return ForwardingResult.Rejected("the forwarded payload is too short")

        val signature = data.copyOfRange(0, SIGNATURE_LENGTH)
        val payload = data.copyOfRange(SIGNATURE_LENGTH, data.size)
        if (!verify(payload, signature, forwarding.secret)) {
            return ForwardingResult.Rejected("the forwarded payload is not signed with the configured secret")
        }

        return runCatching { readPayload(payload) }
            .getOrElse { ForwardingResult.Rejected("the forwarded payload is malformed: ${it.message}") }
    }

    private fun readPayload(payload: ByteArray): ForwardingResult {
        val buffer = ByteBuffer.wrap(payload)
        val version = buffer.readVarInt()
        if (version > MAX_SUPPORTED_VERSION) {
            return ForwardingResult.Rejected(
                "the proxy forwards version $version, and this server reads up to $MAX_SUPPORTED_VERSION",
            )
        }
        val address = buffer.readProtocolString()
        val profileId = buffer.readUuid()
        val name = buffer.readProtocolString(MAX_NAME_LENGTH)
        val properties =
            List(buffer.readVarInt()) {
                val propertyName = buffer.readProtocolString()
                val value = buffer.readProtocolString()
                val signature = if (buffer.readBoolean()) buffer.readProtocolString() else null
                SessionProfileProperty(propertyName, value, signature)
            }
        // Whatever a version 2 or 3 proxy appended is the chat signing key, which nothing reads here.
        return ForwardingResult.Accepted(ForwardedLogin(host = "", address, profileId, name, properties))
    }

    /**
     * The answer a proxy sends, signed. This is the writing half of [decode]: the server never
     * calls it, and the proxy in front of it is exactly what does.
     */
    public fun sign(
        secret: String,
        address: String,
        profileId: Uuid,
        name: String,
        properties: List<SessionProfileProperty>,
        version: Int = MAX_SUPPORTED_VERSION,
    ): ByteArray {
        val buffer = ByteBuffer()
        buffer.writeVarInt(version)
        buffer.writeProtocolString(address)
        buffer.writeUuid(profileId)
        buffer.writeProtocolString(name, MAX_NAME_LENGTH)
        buffer.writeVarInt(properties.size)
        for (property in properties) {
            buffer.writeProtocolString(property.name)
            buffer.writeProtocolString(property.value)
            buffer.writeBoolean(property.signature != null)
            property.signature?.let { buffer.writeProtocolString(it) }
        }
        val payload = buffer.toByteArray()
        return key(secret).signatureGenerator().generateSignatureBlocking(payload) + payload
    }

    private fun verify(
        payload: ByteArray,
        signature: ByteArray,
        secret: String,
    ): Boolean = key(secret).signatureVerifier().tryVerifySignatureBlocking(payload, signature)

    private fun key(secret: String): HMAC.Key =
        CryptographyProvider.Default
            .get(HMAC)
            .keyDecoder(SHA256)
            .decodeFromByteArrayBlocking(HMAC.Key.Format.RAW, secret.encodeToByteArray())

    private const val MAX_NAME_LENGTH = 16
}
