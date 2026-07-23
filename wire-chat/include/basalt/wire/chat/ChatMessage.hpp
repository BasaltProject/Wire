#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::chat {

using protocol::play::MessageSignature;

namespace chatType {
inline constexpr std::int32_t kChat = 0;
inline constexpr std::int32_t kEmoteCommand = 1;
inline constexpr std::int32_t kMsgCommandIncoming = 2;
inline constexpr std::int32_t kMsgCommandOutgoing = 3;
inline constexpr std::int32_t kSayCommand = 4;
inline constexpr std::int32_t kTeamMsgCommandIncoming = 5;
inline constexpr std::int32_t kTeamMsgCommandOutgoing = 6;

inline constexpr std::array<std::string_view, 7> kNames{"chat",
                                                        "emote_command",
                                                        "msg_command_incoming",
                                                        "msg_command_outgoing",
                                                        "say_command",
                                                        "team_msg_command_incoming",
                                                        "team_msg_command_outgoing"};
} // namespace chatType

/// parity: `ChatType.Bound`.
struct BoundChatType {
    std::int32_t type = chatType::kChat;
    text::Component name;
    std::optional<text::Component> target;

    /// parity: `ChatType.Bound.decorate`.
    [[nodiscard]] text::Component decorate(const text::Component& content) const;

    [[nodiscard]] BoundChatType withTarget(text::Component targetName) const;

    [[nodiscard]] protocol::play::ChatTypeBound toWire() const;
};

/// parity: `SignedMessageBody`.
struct SignedBody {
    std::string content;
    std::int64_t timestamp = 0;
    std::int64_t salt = 0;
    std::vector<MessageSignature> lastSeen;
};

/// parity: `PlayerChatMessage`, with its `SignedMessageLink` flattened in.
struct PlayerChatMessage {
    /// parity: `MESSAGE_EXPIRES_AFTER_SERVER`, five minutes.
    static constexpr std::int64_t kExpiresAfterServerMillis = 5LL * 60LL * 1000LL;

    common::Uuid sender;
    common::Uuid sessionId;
    std::int32_t index = 0;
    std::optional<MessageSignature> signature;
    SignedBody body;
    std::optional<text::Component> unsignedContent;

    /// parity: `PlayerChatMessage.unsigned`: no salt, nothing seen, stamped now.
    [[nodiscard]] static PlayerChatMessage unsignedFrom(const common::Uuid& sender, std::string content,
                                                        std::int64_t now);

    /// parity: `PlayerChatMessage.system`, the nil sender, which relays as disguised chat.
    [[nodiscard]] static PlayerChatMessage system(std::string content, std::int64_t now);

    [[nodiscard]] bool isSystem() const noexcept { return sender.isNil(); }

    [[nodiscard]] bool hasSignature() const noexcept { return signature.has_value(); }

    [[nodiscard]] text::Component decoratedContent() const;

    /// parity: `withUnsignedContent`.
    [[nodiscard]] PlayerChatMessage withUnsignedContent(const text::Component& content) const;

    /// parity: `PlayerChatMessage.updateSignature`.
    [[nodiscard]] std::vector<std::uint8_t> signedPayload() const;

    [[nodiscard]] bool hasExpiredServer(std::int64_t now) const noexcept {
        return now > body.timestamp + kExpiresAfterServerMillis;
    }
};

/// parity: `CommandSigningContext.SignedArguments`.
using SignedArguments = std::map<std::string, PlayerChatMessage, std::less<>>;

[[nodiscard]] std::int64_t nowMillis();

/// parity: `StringUtil.isAllowedChatCharacter`.
[[nodiscard]] bool isChatMessageIllegal(std::string_view message);

} // namespace basalt::wire::chat
