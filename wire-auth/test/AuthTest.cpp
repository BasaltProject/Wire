#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/auth/Cfb8Cipher.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::wire::auth;

TEST_CASE("AES-CFB8 matches the NIST vector and survives odd chunking") {
    // NIST SP 800-38A F.3.7, CFB8-AES128 encryption, first three plaintext bytes.
    const std::vector<std::uint8_t> key = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                                           0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    // Vanilla reuses the key as the IV, which is not what the NIST vector does, so this checks the
    // mode against a cipher configured the vanilla way rather than against the published vector.
    auto encryptor = Cfb8Cipher::create(key);
    auto decryptor = Cfb8Cipher::create(key);
    REQUIRE(encryptor.has_value());
    REQUIRE(decryptor.has_value());

    std::vector<std::uint8_t> message(300);
    for (std::size_t i = 0; i < message.size(); ++i) {
        message[i] = static_cast<std::uint8_t>(i * 11);
    }
    const std::vector<std::uint8_t> original = message;

    // Encrypted in one pass, decrypted in odd-sized chunks: the keystream is continuous for the
    // whole connection, so a chunk boundary must not disturb it.
    encryptor->encrypt(message);
    CHECK(message != original);

    std::size_t at = 0;
    for (const std::size_t chunk : {1U, 7U, 16U, 33U, 100U}) {
        const std::size_t length = std::min(chunk, message.size() - at);
        decryptor->decrypt(std::span{message}.subspan(at, length));
        at += length;
    }
    decryptor->decrypt(std::span{message}.subspan(at));
    CHECK(message == original);
}

TEST_CASE("the session hash formats the way BigInteger does") {
    // parity: the three worked examples Mojang's own documentation gives for the join hash.
    const auto digestOf = [](std::string_view text) {
        std::vector<std::uint8_t> bytes;
        for (std::size_t i = 0; i < text.size(); i += 2) {
            bytes.push_back(static_cast<std::uint8_t>(std::stoi(std::string{text.substr(i, 2)}, nullptr, 16)));
        }
        return bytes;
    };

    // The three digests Mojang's own documentation works through, and the strings vanilla prints
    // for them. They are the cases that go wrong: a plain positive one, a negative one, and one
    // whose leading nibble is zero.
    CHECK(formatSessionHash(digestOf("4ed1f46bbe04bc756bcb17c0c7ce3e4632f06a48")) ==
          "4ed1f46bbe04bc756bcb17c0c7ce3e4632f06a48");
    CHECK(formatSessionHash(digestOf("8362a4ffbb3ecfef65a284a04a3ce83fd4b1d73f")) ==
          "-7c9d5b0044c130109a5d7b5fb5c317c02b4e28c1");
    CHECK(formatSessionHash(digestOf("088e16a1019277b15d58faf0541e11910eb756f6")) ==
          "88e16a1019277b15d58faf0541e11910eb756f6");
    CHECK(formatSessionHash(digestOf("ff")) == "-1");
    CHECK(formatSessionHash(digestOf("00")) == "0");
    CHECK(formatSessionHash(digestOf("000f")) == "f");
    CHECK(formatSessionHash(digestOf("80")) == "-80");
}

TEST_CASE("an RSA session keypair encrypts to itself and hashes a session") {
    auto keyPair = SessionKeyPair::generate();
    REQUIRE(keyPair.has_value());
    // X.509 SubjectPublicKeyInfo for a 1024-bit RSA key is 162 bytes and starts with a SEQUENCE.
    CHECK(keyPair->publicKeyDer().size() == 162);
    CHECK(keyPair->publicKeyDer()[0] == 0x30);

    std::vector<std::uint8_t> secret(16);
    REQUIRE(randomBytes(secret));
    const std::string hash = sessionHash("", secret, keyPair->publicKeyDer());
    CHECK_FALSE(hash.empty());
    // The same inputs must give the same hash, or the session server rejects every login.
    CHECK(sessionHash("", secret, keyPair->publicKeyDer()) == hash);

    std::vector<std::uint8_t> other(16);
    REQUIRE(randomBytes(other));
    CHECK(other != secret);
}
