package org.basaltmc.auth

import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.DelicateCryptographyApi
import dev.whyoleg.cryptography.algorithms.AES
import dev.whyoleg.cryptography.operations.Cipher

/**
 * // parity: net.minecraft.util.Crypt.getCipher, `AES/CFB8/NoPadding` with the key reused as IV.
 * Stateful across [update] calls, because the keystream is continuous for the whole connection in
 * each direction, which `cryptography-kotlin`'s whole-message `AES.CFB8` cannot express.
 *
 * So this drives the block cipher directly, per NIST SP 800-38A 6.3: encrypt the shift register,
 * XOR its first byte with the input, shift the ciphertext byte in. One AES block per byte is what
 * CFB8 costs vanilla too. [Cfb8CipherTest] checks it against real `javax.crypto` output, chunked
 * at odd boundaries.
 */
@OptIn(DelicateCryptographyApi::class)
public class Cfb8Cipher private constructor(
    private val blockCipher: Cipher,
    key: ByteArray,
) {
    private val register: ByteArray = key.copyOf()

    public fun update(
        input: ByteArray,
        offset: Int = 0,
        length: Int = input.size - offset,
        encrypting: Boolean,
    ): ByteArray {
        val output = ByteArray(length)
        for (i in 0 until length) {
            val keystreamByte = blockCipher.encryptBlocking(register)[0]
            val inputByte = input[offset + i]
            val outputByte = (inputByte.toInt() xor keystreamByte.toInt()).toByte()
            output[i] = outputByte
            val feedbackByte = if (encrypting) outputByte else inputByte
            register.copyInto(register, destinationOffset = 0, startIndex = 1, endIndex = register.size)
            register[register.size - 1] = feedbackByte
        }
        return output
    }

    public companion object {
        /** [key] is AES-128 (16 bytes); vanilla reuses it verbatim as the initial shift register. */
        public fun create(key: ByteArray): Cfb8Cipher {
            require(key.size == 16) { "AES-128 key must be 16 bytes, was ${key.size}" }
            val ecb = CryptographyProvider.Default.get(AES.ECB)
            val ecbKey = ecb.keyDecoder().decodeFromByteArrayBlocking(AES.Key.Format.RAW, key)
            val blockCipher = ecbKey.cipher(padding = false)
            return Cfb8Cipher(blockCipher, key)
        }
    }
}
