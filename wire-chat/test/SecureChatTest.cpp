#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/chat/ChatMessage.hpp>
#include <basalt/wire/chat/LastSeenValidator.hpp>
#include <basalt/wire/chat/SecureChat.hpp>

#include <doctest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace basalt::wire::chat;
namespace play = basalt::wire::protocol::play;
using basalt::wire::common::Uuid;

namespace {

std::vector<std::uint8_t> hexBytes(std::string_view hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoi(std::string{hex.substr(i, 2)}, nullptr, 16)));
    }
    return bytes;
}

MessageSignature signatureOf(std::string_view hex) {
    MessageSignature signature{};
    const std::vector<std::uint8_t> bytes = hexBytes(hex);
    std::copy(bytes.begin(), bytes.end(), signature.begin());
    return signature;
}

constexpr Uuid kProfile{0x0123456789abcdefULL, 0x0fedcba987654321ULL};
constexpr Uuid kSession{0x1111111111111111ULL, 0x2222222222222222ULL};
constexpr std::int64_t kExpiresAt = 4102444800000;
constexpr std::string_view kPlayerKey =
    "30820122300d06092a864886f70d01010105000382010f003082010a02820101008c002830c5a39c6718d0c8b354ed0d75ee"
    "d55e3ac1045c6deb0b63910c8c9c1068fc5919710544bfdf5d4dd483f9082794f2e2bc9030588768205f067ef260cb1de5b1"
    "606e507cdbb0b08ed50f3d910451f5f51ebffaf7b4b8b519c1c43a143c35e56490c33b028b32f6f2eadefc6a265bc6a7e9ed"
    "ded067d5cbc8c86259e78fd17fbe8e4d02c296596f16247f89dba7c2cede85b9f616a3bee8df6931a8c3840b5c9e1f6cd396"
    "9cae353c7a270ed8e6ebf92c0b72b33df6fceeb0ad50e1b7a970083c0ce243bfb7cf3bc54ba1a3409cedea1461cd15ab6342"
    "ac2bf179d89a785e6a5aaa9910cc501f93d33e59f4125a5084bac2d6bcf02b34cdd88a305428690203010001";
constexpr std::string_view kServicesKey =
    "30819f300d06092a864886f70d010101050003818d0030818902818100ca129f1aa1f2fb0a7dced1120763977f22373d0075"
    "65a455a38bd09a8c57e0436d3fcedcfdc64671d5f9f7d980ced30a9579d319a59a5ad138e4ac37e8be4e5d2ac9853c2030d1"
    "7c882c1e1fe3cb9551670963ed4615b2121c78afdd16032ca9c5a10327bdfce245c318fc68159d5d4a1b24fc45243e8a56e5"
    "80aee07cf5ae630203010001";
constexpr std::string_view kKeySignature =
    "b497ee91e48da0abf6c15b428fae1f47ab202ea2777be223ed1f1876141bac101ab88bfc68a311c566d285e444ca3d25eb0f"
    "161c5f8e5313b6dfd2085b8815d9ce7c160df9fd6b5861706c2127c95b46924cfb2433f1f571cc3483c7846f5b31df5c6522"
    "5a4e7e627baca64b72d830eebb9c3239549d0b03f34b7c66edc431b3";
constexpr std::string_view kFirstSignature =
    "781052fc6c1a344dbbab07312dd102d3c4f69b437764f436435bc4b478806d57dc038b252c7e403c8161c7303518df31483e"
    "c35fcc433533639e3a57f20d2e84ae1f63cdc7936b64d23e5581d3e334599c0309727452678b4200fb89f1e2238965564acc"
    "a85abea67275dc45cb9d07b323cae133c6a36081ab69314e81b6286dba9223e6fb378c10bf85b4d67c66323839c9d719fa54"
    "6f7f9e7e98a88847ff0f964e514bb9212b82e2754bcc5f17f94cbd07206fc3e9a4f71360e9f2236adfaca765d5276ddd4f88"
    "8559add2eb302b6a3dd0184b007de61d3b88f6df5bc68760076759476ad0667f37d7e8f4a49cb8d898736ebb813914004051"
    "e567ea3df6e4";
constexpr std::string_view kSecondSignature =
    "21e685ef09887126bc92d491b8865b35d74f5eb1c7369db227770cc09115540ab27e28b844c67a7b29c5df2fc480a0f8647e"
    "d7cc242fd5628452fa1a3c76e246078805aea8c441fb372608c2281852ca1096e6daa08d5956d64f1945c81ce233f9870fc3"
    "ae59bf27e3df69ee6b2428ab213b84799b00b8b160d646013bf9777d16fa5c9bfd1602c673fb44cdac33e20b46d3a7f5762e"
    "93fcc628f9d6090ce112813f232a65324fd7ce5786c6c14ceb3b65648e671b221aff40cea271db08e13807ae7d26d6a339dc"
    "50cd6425cd14ce440451b466179e1e2b5168c514a7a079f3c04631d383d28145719ae21f089e3a7dc8d45252ec9ebbf02433"
    "ea9ef2e64040";

basalt::wire::auth::ServicesKeySet services() {
    std::vector<basalt::wire::auth::RsaPublicKey> keys;
    keys.push_back(*basalt::wire::auth::RsaPublicKey::parseDer(hexBytes(kServicesKey)));
    return basalt::wire::auth::ServicesKeySet::ofProfileKeys(std::move(keys));
}

play::RemoteChatSessionData chatSession() {
    return play::RemoteChatSessionData{.sessionId = kSession,
                                       .expiresAt = kExpiresAt,
                                       .publicKey = hexBytes(kPlayerKey),
                                       .keySignature = hexBytes(kKeySignature)};
}

} // namespace

TEST_CASE("a chat session is only taken with Mojang's signature over the player's own key") {
    const basalt::wire::auth::ServicesKeySet keys = services();
    SecureChat stranger{Uuid{1, 2}, true, &keys};
    CHECK(stranger.updateSession(chatSession()) == SecureChat::SessionResult::kInvalidSignature);
    SecureChat keyless{kProfile, true, nullptr};
    CHECK(keyless.updateSession(chatSession()) == SecureChat::SessionResult::kIgnored);

    SecureChat chat{kProfile, true, &keys};
    CHECK(chat.updateSession(chatSession()) == SecureChat::SessionResult::kAccepted);
    CHECK(chat.updateSession(chatSession()) == SecureChat::SessionResult::kUnchanged);
    play::RemoteChatSessionData older = chatSession();
    older.expiresAt -= 1;
    CHECK(chat.updateSession(older) == SecureChat::SessionResult::kExpiredKey);
}

TEST_CASE("signed messages chain, acknowledge each other and break on the first bad link") {
    const basalt::wire::auth::ServicesKeySet keys = services();
    SecureChat chat{kProfile, true, &keys};
    REQUIRE(chat.updateSession(chatSession()) == SecureChat::SessionResult::kAccepted);
    const std::int64_t now = 1700000002000;

    const auto first = chat.unpack(signatureOf(kFirstSignature),
                                   SignedBody{.content = "hello", .timestamp = 1700000000123, .salt = 7}, now);
    REQUIRE(first.has_value());
    CHECK(first->index == 0);

    std::vector<play::PlayerChatPacket> sent;
    const auto capture = [&sent](const play::PlayerChatPacket& packet) { sent.push_back(packet); };
    CHECK(chat.send(*first, BoundChatType{}, capture));
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].globalIndex == 0);

    play::LastSeenUpdate update;
    update.offset = 1;
    update.setAcknowledged(LastSeenValidator::kWindow - 1);
    update.checksum = 114;
    const auto seen = chat.applyLastSeen(update);
    REQUIRE(seen.has_value());
    REQUIRE(seen->size() == 1);
    CHECK(seen->front() == signatureOf(kFirstSignature));

    const auto second =
        chat.unpack(signatureOf(kSecondSignature),
                    SignedBody{.content = "again", .timestamp = 1700000001999, .salt = -5, .lastSeen = *seen}, now);
    REQUIRE(second.has_value());
    CHECK(second->index == 1);
    CHECK(chat.send(*second, BoundChatType{}, capture));
    REQUIRE(sent.size() == 2);
    REQUIRE(sent[1].lastSeen.size() == 1);
    CHECK(sent[1].lastSeen[0].id == 0);

    const auto stale = chat.unpack(signatureOf(kSecondSignature),
                                   SignedBody{.content = "again", .timestamp = 1700000000000, .salt = -5}, now);
    CHECK(stale.error() == "chat.disabled.out_of_order_chat");
    const auto after = chat.unpack(signatureOf(kFirstSignature),
                                   SignedBody{.content = "hello", .timestamp = 1700000009000, .salt = 7}, now);
    CHECK(after.error() == "chat.disabled.chain_broken");
}

