#include <basalt/wire/chat/LastSeenValidator.hpp>

#include <algorithm>
#include <deque>

namespace basalt::wire::chat {

LastSeenValidator::LastSeenValidator() : tracked_(kWindow) {}

void LastSeenValidator::addPending(const MessageSignature& signature) {
    if (lastPending_.has_value() && *lastPending_ == signature) {
        return;
    }
    tracked_.push_back(Entry{.signature = signature, .pending = true});
    lastPending_ = signature;
}

std::expected<void, std::string> LastSeenValidator::applyOffset(std::int32_t offset) {
    const auto maxOffset = static_cast<std::int64_t>(tracked_.size()) - static_cast<std::int64_t>(kWindow);
    if (offset < 0 || offset > maxOffset) {
        return std::unexpected{"Advanced last seen window by " + std::to_string(offset) +
                               " messages, but expected at most " + std::to_string(maxOffset)};
    }
    tracked_.erase(tracked_.begin(), tracked_.begin() + offset);
    return {};
}

std::expected<std::vector<MessageSignature>, std::string>
LastSeenValidator::applyUpdate(const protocol::play::LastSeenUpdate& update) {
    if (std::expected<void, std::string> moved = applyOffset(update.offset); !moved.has_value()) {
        return std::unexpected{moved.error()};
    }
    if (update.bitLength() > kWindow) {
        return std::unexpected{"Last seen update contained " + std::to_string(update.bitLength()) +
                               " messages, but maximum window size is " + std::to_string(kWindow)};
    }
    std::vector<MessageSignature> seen;
    for (std::size_t i = 0; i < kWindow; ++i) {
        std::optional<Entry>& message = tracked_[i];
        if (update.isAcknowledged(i)) {
            if (!message.has_value()) {
                return std::unexpected{"Last seen update acknowledged unknown or previously ignored message at index " +
                                       std::to_string(i)};
            }
            message->pending = false;
            seen.push_back(message->signature);
        } else {
            if (message.has_value() && !message->pending) {
                return std::unexpected{"Last seen update ignored previously acknowledged message at index " +
                                       std::to_string(i)};
            }
            message.reset();
        }
    }
    if (update.checksum != 0 && update.checksum != checksumOf(seen)) {
        return std::unexpected{
            std::string{"Checksum mismatch on last seen update: the client and server must have desynced"}};
    }
    return seen;
}

std::int8_t LastSeenValidator::checksumOf(const std::vector<MessageSignature>& signatures) {
    // Java int arithmetic, so everything wraps as unsigned.
    std::uint32_t checksum = 1;
    for (const MessageSignature& signature : signatures) {
        std::uint32_t hash = 1;
        for (const std::uint8_t byte : signature) {
            hash = 31U * hash + static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(byte)));
        }
        checksum = 31U * checksum + hash;
    }
    const auto folded = static_cast<std::int8_t>(static_cast<std::uint8_t>(checksum));
    return folded == 0 ? std::int8_t{1} : folded;
}

std::int32_t MessageSignatureCache::pack(const MessageSignature& signature) const {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].has_value() && *entries_[i] == signature) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

void MessageSignatureCache::push(const std::vector<MessageSignature>& lastSeen,
                                 const std::optional<MessageSignature>& signature) {
    std::deque<MessageSignature> queue{lastSeen.begin(), lastSeen.end()};
    if (signature.has_value()) {
        queue.push_back(*signature);
    }
    const std::vector<MessageSignature> incoming{queue.begin(), queue.end()};
    for (std::size_t i = 0; !queue.empty() && i < entries_.size(); ++i) {
        const std::optional<MessageSignature> previous = entries_[i];
        entries_[i] = queue.back();
        queue.pop_back();
        if (previous.has_value() && std::ranges::find(incoming, *previous) == incoming.end()) {
            queue.push_front(*previous);
        }
    }
}

} // namespace basalt::wire::chat
