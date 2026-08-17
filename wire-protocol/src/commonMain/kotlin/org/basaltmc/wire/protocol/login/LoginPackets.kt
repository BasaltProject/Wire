package org.basaltmc.wire.protocol.login

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readByteArray
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readUuid
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeByteArray
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeUuid
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentJson
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Serverbound, id 0x00: the first packet of login. Mirrors `ServerboundHelloPacket`. Used for
 * both the offline-mode and online-mode paths; which one follows depends on whether the server
 * sends [ClientboundHelloPacket] before [LoginCompressionPacket]/[LoginFinishedPacket].
 */
@OptIn(ExperimentalUuidApi::class)
public data class LoginStartPacket(
    val name: String,
    val profileId: Uuid,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(name, MAX_NAME_LENGTH)
        buffer.writeUuid(profileId)
    }

    public companion object {
        public const val ID: Int = 0
        private const val MAX_NAME_LENGTH = 16

        public fun read(buffer: ByteBuffer): LoginStartPacket {
            val name = buffer.readProtocolString(MAX_NAME_LENGTH)
            val profileId = buffer.readUuid()
            return LoginStartPacket(name, profileId)
        }
    }
}

/**
 * Clientbound, id 0x01: starts the online-mode key exchange. Mirrors `ClientboundHelloPacket`.
 * `serverId` is always the empty string in this version (a vestigial field from very old
 * protocol revisions); `publicKey` is the server's RSA key in X.509 `SubjectPublicKeyInfo` DER
 * form; `challenge` is 4 random bytes the client must echo back RSA-encrypted, to prove it can
 * decrypt with the matching public key.
 */
public data class HelloPacket(
    val serverId: String,
    val publicKey: ByteArray,
    val challenge: ByteArray,
    val shouldAuthenticate: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(serverId)
        buffer.writeByteArray(publicKey)
        buffer.writeByteArray(challenge)
        buffer.writeBoolean(shouldAuthenticate)
    }

    override fun equals(other: Any?): Boolean =
        other is HelloPacket &&
            serverId == other.serverId &&
            publicKey.contentEquals(other.publicKey) &&
            challenge.contentEquals(other.challenge) &&
            shouldAuthenticate == other.shouldAuthenticate

    override fun hashCode(): Int {
        var result = serverId.hashCode()
        result = 31 * result + publicKey.contentHashCode()
        result = 31 * result + challenge.contentHashCode()
        result = 31 * result + shouldAuthenticate.hashCode()
        return result
    }

    public companion object {
        public const val ID: Int = 1
        private const val MAX_SERVER_ID_LENGTH = 20

        public fun read(buffer: ByteBuffer): HelloPacket {
            val serverId = buffer.readProtocolString(MAX_SERVER_ID_LENGTH)
            val publicKey = buffer.readByteArray()
            val challenge = buffer.readByteArray()
            val shouldAuthenticate = buffer.readBoolean()
            return HelloPacket(serverId, publicKey, challenge, shouldAuthenticate)
        }
    }
}

/**
 * Serverbound, id 0x01: answers [HelloPacket]. Mirrors `ServerboundKeyPacket`. Both fields are
 * RSA-PKCS1-encrypted with the public key the server just sent: `encryptedSecret` is the AES-128
 * shared secret the connection encrypts with from here on, `encryptedChallenge` is the same
 * challenge bytes the server sent, proving the client holds a matching private key.
 */
public data class KeyPacket(
    val encryptedSecret: ByteArray,
    val encryptedChallenge: ByteArray,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeByteArray(encryptedSecret)
        buffer.writeByteArray(encryptedChallenge)
    }

    override fun equals(other: Any?): Boolean =
        other is KeyPacket &&
            encryptedSecret.contentEquals(other.encryptedSecret) &&
            encryptedChallenge.contentEquals(other.encryptedChallenge)

    override fun hashCode(): Int = 31 * encryptedSecret.contentHashCode() + encryptedChallenge.contentHashCode()

    public companion object {
        public const val ID: Int = 1

        public fun read(buffer: ByteBuffer): KeyPacket {
            val encryptedSecret = buffer.readByteArray()
            val encryptedChallenge = buffer.readByteArray()
            return KeyPacket(encryptedSecret, encryptedChallenge)
        }
    }
}

/** Clientbound, id 0x03: negotiates the compression threshold. Mirrors `ClientboundLoginCompressionPacket`. */
public data class LoginCompressionPacket(
    val threshold: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(threshold)
    }

    public companion object {
        public const val ID: Int = 3

        public fun read(buffer: ByteBuffer): LoginCompressionPacket = LoginCompressionPacket(buffer.readVarInt())
    }
}

public data class GameProfileProperty(
    val name: String,
    val value: String,
    val signature: String? = null,
)

@OptIn(ExperimentalUuidApi::class)
public data class GameProfile(
    val id: Uuid,
    val name: String,
    val properties: List<GameProfileProperty> = emptyList(),
)

/**
 * Clientbound, id 0x02: login success. Mirrors `ClientboundLoginFinishedPacket`. Notably carries
 * a `sessionId` field alongside the [GameProfile] in this version: confirmed against the
 * decompiled 26.2 jar, not present in older protocol documentation.
 */
@OptIn(ExperimentalUuidApi::class)
public data class LoginFinishedPacket(
    val gameProfile: GameProfile,
    val sessionId: Uuid,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeUuid(gameProfile.id)
        buffer.writeProtocolString(gameProfile.name, 16)
        buffer.writeVarInt(gameProfile.properties.size)
        for (property in gameProfile.properties) {
            buffer.writeProtocolString(property.name)
            buffer.writeProtocolString(property.value)
            buffer.writeBoolean(property.signature != null)
            property.signature?.let { buffer.writeProtocolString(it) }
        }
        buffer.writeUuid(sessionId)
    }

    public companion object {
        public const val ID: Int = 2

        public fun read(buffer: ByteBuffer): LoginFinishedPacket {
            val id = buffer.readUuid()
            val name = buffer.readProtocolString(16)
            val propertyCount = buffer.readVarInt()
            val properties =
                List(propertyCount) {
                    val propName = buffer.readProtocolString()
                    val propValue = buffer.readProtocolString()
                    val signature = if (buffer.readBoolean()) buffer.readProtocolString() else null
                    GameProfileProperty(propName, propValue, signature)
                }
            val sessionId = buffer.readUuid()
            return LoginFinishedPacket(GameProfile(id, name, properties), sessionId)
        }
    }
}

/** Serverbound, id 0x03: acknowledges [LoginFinishedPacket], moving the connection to CONFIGURATION. */
public object LoginAcknowledgedPacket {
    public const val ID: Int = 3
}

/** Clientbound, id 0x00: rejects the login attempt with a reason. Mirrors `ClientboundLoginDisconnectPacket`. */
public data class LoginDisconnectPacket(
    val reason: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(ComponentJson.serialize(reason).toString())
    }

    public companion object {
        public const val ID: Int = 0
    }
}
