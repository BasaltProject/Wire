#include <basalt/wire/chat/ChatMessage.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <chrono>
#include <utility>

namespace basalt::wire::chat {
namespace {

using text::Component;

void putInt(std::vector<std::uint8_t>& into, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        into.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void putLong(std::vector<std::uint8_t>& into, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        into.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void putUuid(std::vector<std::uint8_t>& into, const common::Uuid& uuid) {
    putLong(into, uuid.high());
    putLong(into, uuid.low());
}

/// parity: `Instant.getEpochSecond`, which floors.
[[nodiscard]] std::int64_t epochSecond(std::int64_t millis) noexcept {
    return millis >= 0 ? millis / 1000 : -((-millis + 999) / 1000);
}

} // namespace

Component BoundChatType::decorate(const Component& content) const {
    const Component targetName = target.value_or(Component::empty());
    // parity: the decorations of the seven vanilla chat types, parameters in the order they list.
    switch (type) {
    case chatType::kEmoteCommand:
        return Component::translatable("chat.type.emote", {name, content});
    case chatType::kMsgCommandIncoming:
        return Component::translatable("commands.message.display.incoming", {name, content})
            .color(text::kGray)
            .italic(true);
    case chatType::kMsgCommandOutgoing:
        return Component::translatable("commands.message.display.outgoing", {targetName, content})
            .color(text::kGray)
            .italic(true);
    case chatType::kSayCommand:
        return Component::translatable("chat.type.announcement", {name, content});
    case chatType::kTeamMsgCommandIncoming:
        return Component::translatable("chat.type.team.text", {targetName, name, content});
    case chatType::kTeamMsgCommandOutgoing:
        return Component::translatable("chat.type.team.sent", {targetName, name, content});
    default:
        return Component::translatable("chat.type.text", {name, content});
    }
}

BoundChatType BoundChatType::withTarget(Component targetName) const {
    BoundChatType bound = *this;
    bound.target = std::move(targetName);
    return bound;
}

protocol::play::ChatTypeBound BoundChatType::toWire() const {
    return protocol::play::ChatTypeBound{.chatType = type, .name = name, .targetName = target};
}

PlayerChatMessage PlayerChatMessage::unsignedFrom(const common::Uuid& sender, std::string content, std::int64_t now) {
    PlayerChatMessage message;
    message.sender = sender;
    message.body.content = std::move(content);
    message.body.timestamp = now;
    return message;
}

PlayerChatMessage PlayerChatMessage::system(std::string content, std::int64_t now) {
    return unsignedFrom(common::Uuid{}, std::move(content), now);
}

Component PlayerChatMessage::decoratedContent() const {
    return unsignedContent.has_value() ? *unsignedContent : Component::text(body.content);
}

PlayerChatMessage PlayerChatMessage::withUnsignedContent(const Component& content) const {
    PlayerChatMessage message = *this;
    if (content == Component::text(body.content)) {
        message.unsignedContent.reset();
    } else {
        message.unsignedContent = content;
    }
    return message;
}

std::vector<std::uint8_t> PlayerChatMessage::signedPayload() const {
    std::vector<std::uint8_t> payload;
    payload.reserve(64 + body.content.size() + body.lastSeen.size() * protocol::play::kMessageSignatureBytes);
    putInt(payload, 1);
    putUuid(payload, sender);
    putUuid(payload, sessionId);
    putInt(payload, static_cast<std::uint32_t>(index));
    putLong(payload, static_cast<std::uint64_t>(body.salt));
    putLong(payload, static_cast<std::uint64_t>(epochSecond(body.timestamp)));
    putInt(payload, static_cast<std::uint32_t>(body.content.size()));
    payload.insert(payload.end(), body.content.begin(), body.content.end());
    putInt(payload, static_cast<std::uint32_t>(body.lastSeen.size()));
    for (const MessageSignature& seen : body.lastSeen) {
        payload.insert(payload.end(), seen.begin(), seen.end());
    }
    return payload;
}

std::int64_t nowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool isChatMessageIllegal(std::string_view message) {
    for (std::size_t i = 0; i < message.size(); ++i) {
        const auto byte = static_cast<unsigned char>(message[i]);
        if (byte < 0x20 || byte == 0x7F) {
            return true;
        }
        if (byte == 0xC2 && i + 1 < message.size() && static_cast<unsigned char>(message[i + 1]) == 0xA7) {
            return true;
        }
    }
    return false;
}

} // namespace basalt::wire::chat
