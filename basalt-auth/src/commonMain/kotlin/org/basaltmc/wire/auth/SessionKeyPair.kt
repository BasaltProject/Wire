package org.basaltmc.wire.auth

import dev.whyoleg.cryptography.BinarySize.Companion.bits
import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.DelicateCryptographyApi
import dev.whyoleg.cryptography.algorithms.RSA
import dev.whyoleg.cryptography.algorithms.SHA1

/**
 * The server's RSA keypair for the online-mode login handshake.
 *
 * // parity: `net.minecraft.util.Crypt.generateKeyPair`: 1024-bit RSA, PKCS#1 v1.5 padding for
 * encryption (`Cipher.getInstance("RSA")` defaults to `RSA/ECB/PKCS1Padding`), public key sent to
 * the client as an X.509 `SubjectPublicKeyInfo` DER blob (`PublicKey.getEncoded()`), which is
 * exactly [RSA.PublicKey.Format.DER] here. The digest algorithm chosen for key generation has no
 * bearing on PKCS#1 encryption; SHA-1 is passed only because the API requires one.
 */
@OptIn(DelicateCryptographyApi::class)
public class SessionKeyPair private constructor(
    private val keyPair: RSA.PKCS1.KeyPair,
) {
    /** X.509 `SubjectPublicKeyInfo` DER encoding, sent to the client in `ClientboundHelloPacket`. */
    public val publicKeyDer: ByteArray by lazy {
        keyPair.publicKey.encodeToByteArrayBlocking(RSA.PublicKey.Format.DER)
    }

    /** Decrypts an RSA-PKCS1-encrypted payload (the shared secret, or the login challenge) with the private key. */
    public fun decrypt(ciphertext: ByteArray): ByteArray = keyPair.privateKey.decryptor().decryptBlocking(ciphertext)

    public companion object {
        public fun generate(): SessionKeyPair {
            val rsa = CryptographyProvider.Default.get(RSA.PKCS1)
            val keyPair = rsa.keyPairGenerator(keySize = 1024.bits, digest = SHA1).generateKeyBlocking()
            return SessionKeyPair(keyPair)
        }
    }
}

/**
 * The join-server hash sent to Mojang's session server, and the ISO-8859-1 encoding vanilla uses
 * for the (always-empty in modern versions) `serverId` component of it.
 *
 * // parity: `net.minecraft.util.Crypt.digestData(serverId, publicKey, secretKey)`: SHA-1 over
 * `serverId.getBytes("ISO_8859_1")`, then `secretKey.getEncoded()`, then `publicKey.getEncoded()`,
 * formatted with [SessionHash].
 */
public object MinecraftCrypt {
    @OptIn(DelicateCryptographyApi::class)
    public fun sessionHash(
        serverId: String,
        secretKey: ByteArray,
        publicKeyDer: ByteArray,
    ): String {
        val hasher = CryptographyProvider.Default.get(SHA1).hasher()
        val digest = hasher.hashBlocking(serverId.encodeIso88591() + secretKey + publicKeyDer)
        return SessionHash.format(digest)
    }
}

private fun String.encodeIso88591(): ByteArray = ByteArray(length) { index -> this[index].code.toByte() }
