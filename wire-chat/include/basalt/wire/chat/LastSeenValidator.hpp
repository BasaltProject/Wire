#pragma once

#include <basalt/wire/chat/ChatMessage.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::chat {

/// parity: `LastSeenMessagesValidator`.
class LastSeenValidator {
public:
    /// parity: `ServerGamePacketListenerImpl.lastSeenMessages`, twenty wide.
    static constexpr std::size_t kWindow = protocol::play::LastSeenUpdate::kWindow;

    LastSeenValidator();

    /// parity: `addPending`, which skips a signature sent twice in a row.
    void addPending(const MessageSignature& signature);

    [[nodiscard]] std::size_t trackedCount() const noexcept { return tracked_.size(); }

    [[nodiscard]] std::expected<void, std::string> applyOffset(std::int32_t offset);

    [[nodiscard]] std::expected<std::vector<MessageSignature>, std::string>
    applyUpdate(const protocol::play::LastSeenUpdate& update);

    /// parity: `LastSeenMessages.computeChecksum`, the Java array hashes folded as a list's.
    [[nodiscard]] static std::int8_t checksumOf(const std::vector<MessageSignature>& signatures);

private:
    struct Entry {
        MessageSignature signature{};
        bool pending = true;
    };

    std::deque<std::optional<Entry>> tracked_;
    std::optional<MessageSignature> lastPending_;
};

/// parity: `MessageSignatureCache`.
class MessageSignatureCache {
public:
    static constexpr std::size_t kCapacity = 128;

    [[nodiscard]] std::int32_t pack(const MessageSignature& signature) const;

    /// parity: `push(body, signature)`.
    void push(const std::vector<MessageSignature>& lastSeen, const std::optional<MessageSignature>& signature);

private:
    std::vector<std::optional<MessageSignature>> entries_ = std::vector<std::optional<MessageSignature>>(kCapacity);
};

} // namespace basalt::wire::chat
