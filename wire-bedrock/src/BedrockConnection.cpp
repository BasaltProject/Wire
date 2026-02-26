#include <basalt/wire/bedrock/BedrockConnection.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/common/Base64.hpp>

#include <random>
#include <utility>

namespace basalt::wire::bedrock {
namespace {

/// The packet id lives in the low ten bits; the rest names a split screen player nobody here has.
constexpr std::uint32_t kPacketIdMask = 0x3ffU;

/// Sixteen bytes of salt, which is what both ends hash with the shared secret.
constexpr std::size_t kSaltSize = 16;

std::vector<std::uint8_t> randomSalt() {
    std::random_device source;
    std::vector<std::uint8_t> salt(kSaltSize);
    for (std::uint8_t& byte : salt) {
        byte = static_cast<std::uint8_t>(source() & 0xffU);
    }
    return salt;
}

} // namespace

BedrockConnection::BedrockConnection(RakSession& transport, std::shared_ptr<const EcKeyPair> keys,
                                     std::string gameVersion, BedrockGate gate)
    : transport_(transport), keys_(std::move(keys)), gameVersion_(std::move(gameVersion)), gate_(std::move(gate)) {}

void BedrockConnection::receive(std::span<const std::uint8_t> payload) {
    if (state_ == BedrockState::Closed || payload.empty()) {
        return;
    }

    std::span<const std::uint8_t> batch = payload;
    if (cipher_) {
        if (!cipher_->decrypt(payload, decrypted_)) {
            disconnect("encryption out of step");
            return;
        }
        batch = decrypted_;
    }

    incoming_.clear();
    if (!unpackBatch(batch, batch_, incoming_)) {
        disconnect("bad batch");
        return;
    }

    for (const std::vector<std::uint8_t>& packet : incoming_) {
        io::ByteBuffer reader = io::ByteBuffer::wrap({packet.begin(), packet.end()});
        const std::uint32_t header = readVarUInt(reader);
        if (reader.failed()) {
            return;
        }
        handlePacket(static_cast<BedrockPacket>(header & kPacketIdMask), reader);
        if (state_ == BedrockState::Closed) {
            return;
        }
    }
}

void BedrockConnection::handlePacket(BedrockPacket packet, io::ByteBuffer& body) {
    switch (packet) {
    case BedrockPacket::RequestNetworkSettings:
        onRequestNetworkSettings(body);
        return;
    case BedrockPacket::Login:
        onLoginPacket(body);
        return;
    case BedrockPacket::ClientToServerHandshake:
        if (state_ != BedrockState::AwaitingEncryption) {
            return;
        }
        state_ = BedrockState::ResourcePacks;
        scratch_.clear();
        writePlayStatus(scratch_, PlayStatus::LoginSuccess);
        sendPacket(BedrockPacket::PlayStatus, scratch_.readable());
        scratch_.clear();
        writeResourcePacksInfo(scratch_);
        sendPacket(BedrockPacket::ResourcePacksInfo, scratch_.readable());
        flush();
        return;
    case BedrockPacket::ResourcePackClientResponse:
        onResourcePackResponse(body);
        return;
    case BedrockPacket::Disconnect:
        state_ = BedrockState::Closed;
        return;
    default:
        if (onPacket_) {
            onPacket_(packet, body);
        }
        return;
    }
}

void BedrockConnection::onRequestNetworkSettings(io::ByteBuffer& body) {
    if (state_ != BedrockState::AwaitingSettings) {
        return;
    }
    const auto request = readRequestNetworkSettings(body);
    if (!request) {
        disconnect("bad network settings request");
        return;
    }
    protocolVersion_ = request->protocolVersion;

    const BedrockVerdict verdict = gate_.verdict(protocolVersion_);
    if (!verdict.allowed()) {
        // The status is what draws the screen: a client told it is out of date offers to update
        // itself, and one told the server is out of date says so instead. A disconnect on its own
        // would be a message over a blank screen.
        scratch_.clear();
        writePlayStatus(scratch_, verdict.kind == BedrockVerdict::Kind::TooOld ? PlayStatus::ClientTooOld
                                                                               : PlayStatus::ServerTooOld);
        sendPacket(BedrockPacket::PlayStatus, scratch_.readable());
        flush();
        state_ = BedrockState::Closed;
        if (onRefused_) {
            onRefused_(protocolVersion_, gate_.refusalFor(verdict, gameVersion_));
        }
        return;
    }
    if (verdict.warn && onWarning_) {
        onWarning_(protocolVersion_);
    }

    scratch_.clear();
    writeNetworkSettings(scratch_, batch_.threshold, static_cast<std::uint16_t>(Compression::Zlib));
    sendPacket(BedrockPacket::NetworkSettings, scratch_.readable());
    flush();

    // Everything after the answer is compressed, the answer itself is not.
    batch_.enabled = true;
    state_ = BedrockState::AwaitingLogin;
}

void BedrockConnection::onLoginPacket(io::ByteBuffer& body) {
    if (state_ != BedrockState::AwaitingLogin) {
        return;
    }
    const auto request = readLogin(body);
    if (!request) {
        disconnect("bad login");
        return;
    }
    login_ = *request;

    const auto salt = randomSalt();
    const auto secret = keys_->sharedSecret(login_.identityPublicKey);
    if (!secret) {
        disconnect("bad identity key");
        return;
    }

    const std::string payload = R"({"salt":")" + common::base64::encode(salt) + R"("})";
    const auto jwt = keys_->signJwt(payload);
    if (!jwt) {
        disconnect("could not sign the handshake");
        return;
    }

    if (onLogin_) {
        onLogin_(login_);
        if (state_ == BedrockState::Closed) {
            return;
        }
    }

    scratch_.clear();
    writeServerToClientHandshake(scratch_, *jwt);
    sendPacket(BedrockPacket::ServerToClientHandshake, scratch_.readable());
    flush();

    cipher_ = std::make_unique<BedrockCipher>(deriveSessionKey(salt, *secret));
    state_ = BedrockState::AwaitingEncryption;
}

void BedrockConnection::onResourcePackResponse(io::ByteBuffer& body) {
    if (state_ != BedrockState::ResourcePacks) {
        return;
    }
    const auto response = readResourcePackResponse(body);
    if (!response) {
        disconnect("bad resource pack response");
        return;
    }

    switch (*response) {
    case ResourcePackResponse::HaveAllPacks:
        scratch_.clear();
        writeResourcePackStack(scratch_, gameVersion_);
        sendPacket(BedrockPacket::ResourcePackStack, scratch_.readable());
        flush();
        return;
    case ResourcePackResponse::Completed:
        state_ = BedrockState::Playing;
        if (onReady_) {
            onReady_();
        }
        return;
    case ResourcePackResponse::Refused:
        disconnect("the resource packs were refused");
        return;
    case ResourcePackResponse::SendPacks:
        // Nothing to send: this server has no packs, so the client is told the stack is empty and
        // it asks again with everything it needs.
        scratch_.clear();
        writeResourcePackStack(scratch_, gameVersion_);
        sendPacket(BedrockPacket::ResourcePackStack, scratch_.readable());
        flush();
        return;
    }
}

void BedrockConnection::sendPacket(BedrockPacket packet, std::span<const std::uint8_t> body) {
    if (state_ == BedrockState::Closed) {
        return;
    }
    io::ByteBuffer encoded{body.size() + 2};
    writeVarUInt(encoded, static_cast<std::uint32_t>(packet));
    encoded.writeBytes(body);
    outgoing_.emplace_back(encoded.readable().begin(), encoded.readable().end());
}

void BedrockConnection::flush() {
    if (outgoing_.empty()) {
        return;
    }

    io::ByteBuffer batch{512};
    const bool packed = packBatch(outgoing_, batch_, batch);
    outgoing_.clear();
    if (!packed) {
        disconnect("could not pack a batch");
        return;
    }
    sendRaw(batch.readable());
}

void BedrockConnection::sendRaw(std::span<const std::uint8_t> payload) {
    if (!cipher_) {
        transport_.sendGame(payload);
        return;
    }
    std::vector<std::uint8_t> encrypted;
    cipher_->encrypt(payload, encrypted);
    transport_.sendGame(encrypted);
}

void BedrockConnection::disconnect(std::string_view reason) {
    if (state_ == BedrockState::Closed) {
        return;
    }
    scratch_.clear();
    writeDisconnect(scratch_, reason);
    sendPacket(BedrockPacket::Disconnect, scratch_.readable());
    flush();
    state_ = BedrockState::Closed;
}

} // namespace basalt::wire::bedrock
