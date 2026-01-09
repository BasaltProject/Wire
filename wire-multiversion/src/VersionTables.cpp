#include <basalt/wire/multiversion/VersionTables.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>

namespace basalt::wire::multiversion {
namespace {

std::int32_t toInt(std::string_view text) noexcept {
    std::int32_t value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

} // namespace

void PacketIdMap::parse() const {
    if (!encoded_.empty()) {
        std::size_t start = 0;
        const std::string_view encoded{encoded_};
        for (std::size_t i = 0; i <= encoded.size(); ++i) {
            if (i == encoded.size() || encoded[i] == ',') {
                forward_.push_back(toInt(encoded.substr(start, i - start)));
                start = i + 1;
            }
        }
    }

    const auto highest = forward_.empty() ? -1 : *std::ranges::max_element(forward_);
    backward_.assign(static_cast<std::size_t>(highest + 1), kDropped);
    for (std::size_t native = 0; native < forward_.size(); ++native) {
        const std::int32_t older = forward_[native];
        if (older >= 0) {
            backward_[static_cast<std::size_t>(older)] = static_cast<std::int32_t>(native);
        }
    }
    parsed_ = true;
}

std::int32_t PacketIdMap::map(std::int32_t nativeId) const {
    if (!parsed_) {
        parse();
    }
    if (nativeId < 0 || static_cast<std::size_t>(nativeId) >= forward_.size()) {
        return kDropped;
    }
    return forward_[static_cast<std::size_t>(nativeId)];
}

std::int32_t PacketIdMap::unmap(std::int32_t olderId) const {
    if (!parsed_) {
        parse();
    }
    if (olderId < 0 || static_cast<std::size_t>(olderId) >= backward_.size()) {
        return kDropped;
    }
    return backward_[static_cast<std::size_t>(olderId)];
}

const PacketIdMap& PacketIdMap::identity() {
    static const PacketIdMap instance{""};
    return instance;
}

std::int32_t PacketIdTables::toOlder(protocol::ProtocolState state, std::int32_t id) const {
    const auto found = clientbound_.find(state);
    return found == clientbound_.end() ? id : found->second.map(id);
}

std::int32_t PacketIdTables::toNative(protocol::ProtocolState state, std::int32_t id) const {
    const auto found = serverbound_.find(state);
    return found == serverbound_.end() ? id : found->second.unmap(id);
}

bool PacketIdTables::hasClientbound(protocol::ProtocolState state) const { return clientbound_.contains(state); }

bool PacketIdTables::hasServerbound(protocol::ProtocolState state) const { return serverbound_.contains(state); }

} // namespace basalt::wire::multiversion
