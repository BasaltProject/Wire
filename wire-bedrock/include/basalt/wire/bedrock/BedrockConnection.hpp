#pragma once

#include <basalt/wire/bedrock/BedrockBatch.hpp>
#include <basalt/wire/bedrock/BedrockCipher.hpp>
#include <basalt/wire/bedrock/BedrockGate.hpp>
#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/bedrock/BedrockPacketIds.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/RakSession.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::bedrock {

/// How far along the login a connection is. Everything before Playing is this class's business;
/// after it, packets belong to whoever owns the connection.
enum class BedrockState : std::uint8_t {
    AwaitingSettings,
    AwaitingLogin,
    AwaitingEncryption,
    ResourcePacks,
    Playing,
    Closed,
};

/// One Bedrock client, from the first packet to the last: batching, compression, encryption and the
/// login. It knows nothing about worlds or players, and hands everything it does not own upwards.
class BedrockConnection {
public:
    using LoginHandler = std::function<void(const LoginRequest&)>;
    using ReadyHandler = std::function<void()>;
    using PacketHandler = std::function<void(BedrockPacket, io::ByteBuffer&)>;

    BedrockConnection(RakSession& transport, std::shared_ptr<const EcKeyPair> keys, std::string gameVersion,
                      BedrockGate gate = BedrockGate{});

    [[nodiscard]] BedrockState state() const noexcept { return state_; }

    [[nodiscard]] int protocolVersion() const noexcept { return protocolVersion_; }

    [[nodiscard]] const LoginRequest& login() const noexcept { return login_; }

    [[nodiscard]] RakSession& transport() const noexcept { return transport_; }

    /// Called when the login has been read and the client is who it says it is. A handler that
    /// wants to refuse calls disconnect().
    void onLogin(LoginHandler handler) { onLogin_ = std::move(handler); }

    /// Called once the client is past the resource packs and is waiting to be told about a world.
    void onReady(ReadyHandler handler) { onReady_ = std::move(handler); }

    /// Everything this class does not answer itself.
    void onPacket(PacketHandler handler) { onPacket_ = std::move(handler); }

    /// Called when a client was turned away at the door, with the protocol it claimed and what it
    /// was told. For the log: the client already has its answer.
    void onRefused(std::function<void(int, const std::string&)> handler) { onRefused_ = std::move(handler); }

    /// Called when a client newer than this build was let in anyway.
    void onWarning(std::function<void(int)> handler) { onWarning_ = std::move(handler); }

    /// Takes one game payload off RakNet: a batch, maybe encrypted, maybe compressed.
    void receive(std::span<const std::uint8_t> payload);

    void sendPacket(BedrockPacket packet, std::span<const std::uint8_t> body);

    /// Packs what is queued into one batch and hands it to the transport. A batch is the unit both
    /// compression and encryption work on, so packets are held until here on purpose.
    void flush();

    void disconnect(std::string_view reason);

    /// Stops reading and writing without saying anything: for a caller that has already said its
    /// own goodbye and only needs this to stop.
    void close() { state_ = BedrockState::Closed; }

private:
    void handlePacket(BedrockPacket packet, io::ByteBuffer& body);
    void onRequestNetworkSettings(io::ByteBuffer& body);
    void onLoginPacket(io::ByteBuffer& body);
    void onResourcePackResponse(io::ByteBuffer& body);
    void sendRaw(std::span<const std::uint8_t> payload);

    RakSession& transport_;
    std::shared_ptr<const EcKeyPair> keys_;
    std::string gameVersion_;
    BedrockGate gate_;

    BedrockState state_ = BedrockState::AwaitingSettings;
    int protocolVersion_ = 0;
    LoginRequest login_;
    BatchSettings batch_;
    /// Made only after the handshake has gone out: the packet that carries the key cannot be
    /// encrypted with it, and everything after it must be.
    std::unique_ptr<BedrockCipher> cipher_;

    LoginHandler onLogin_;
    ReadyHandler onReady_;
    PacketHandler onPacket_;
    std::function<void(int, const std::string&)> onRefused_;
    std::function<void(int)> onWarning_;

    std::vector<std::vector<std::uint8_t>> outgoing_;
    std::vector<std::vector<std::uint8_t>> incoming_;
    std::vector<std::uint8_t> decrypted_;
    io::ByteBuffer scratch_;
};

} // namespace basalt::wire::bedrock
