package org.basaltmc.wire.auth

import dev.whyoleg.cryptography.BinarySize.Companion.bits
import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.DelicateCryptographyApi
import dev.whyoleg.cryptography.algorithms.AES
import dev.whyoleg.cryptography.algorithms.RSA
import dev.whyoleg.cryptography.algorithms.SHA1
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertFalse

/**
 * Smoke test for the `cryptography-kotlin` dependency (wraps OpenSSL3/CryptoKit/JCA per target):
 * proves RSA-PKCS1 and AES-CFB8, the two primitives the online-mode login handshake needs
 * (`ClientboundHelloPacket`/`ServerboundKeyPacket`, still to be implemented), actually link and
 * round-trip correctly through the platform-native provider on this host.
 */
@OptIn(DelicateCryptographyApi::class)
class CryptographyProviderSmokeTest {
    @Test
    fun rsaPkcs1RoundTrip() {
        val rsa = CryptographyProvider.Default.get(RSA.PKCS1)
        // parity: vanilla's Crypt.java generates a 1024-bit RSA key for the session handshake.
        val keyPair = rsa.keyPairGenerator(keySize = 1024.bits, digest = SHA1).generateKeyBlocking()
        val plaintext = "shared secret".encodeToByteArray()
        val ciphertext = keyPair.publicKey.encryptor().encryptBlocking(plaintext)
        val decrypted = keyPair.privateKey.decryptor().decryptBlocking(ciphertext)
        assertContentEquals(plaintext, decrypted)
    }

    @Test
    fun aesCfb8RoundTripWithExplicitIv() {
        val aes = CryptographyProvider.Default.get(AES.CFB8)
        // parity: vanilla reuses the shared secret itself as both the AES key and the IV.
        val key = aes.keyGenerator(AES.Key.Size.B128).generateKeyBlocking()
        val iv = ByteArray(16) { it.toByte() }
        val plaintext = "packet payload bytes".encodeToByteArray()
        val cipher = key.cipher()

        val ciphertext = cipher.encryptWithIvBlocking(iv, plaintext)
        assertFalse(plaintext.contentEquals(ciphertext))

        val decrypted = cipher.decryptWithIvBlocking(iv, ciphertext)
        assertContentEquals(plaintext, decrypted)
    }
}
