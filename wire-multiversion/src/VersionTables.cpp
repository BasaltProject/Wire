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
    if (inverted_) {
        return lookupBackward(nativeId);
    }
    return lookupForward(nativeId);
}

std::int32_t PacketIdMap::unmap(std::int32_t olderId) const {
    if (inverted_) {
        return lookupForward(olderId);
    }
    return lookupBackward(olderId);
}

std::int32_t PacketIdMap::lookupForward(std::int32_t nativeId) const {
    if (!parsed_) {
        parse();
    }
    if (nativeId < 0 || static_cast<std::size_t>(nativeId) >= forward_.size()) {
        return kDropped;
    }
    return forward_[static_cast<std::size_t>(nativeId)];
}

std::int32_t PacketIdMap::lookupBackward(std::int32_t olderId) const {
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

std::int32_t PacketIdTables::clientboundToNative(protocol::ProtocolState state, std::int32_t id) const {
    const auto found = clientbound_.find(state);
    return found == clientbound_.end() ? id : found->second.unmap(id);
}

std::int32_t PacketIdTables::serverboundToOlder(protocol::ProtocolState state, std::int32_t id) const {
    const auto found = serverbound_.find(state);
    return found == serverbound_.end() ? id : found->second.map(id);
}

PacketIdTables PacketIdTables::inverted() const {
    std::map<protocol::ProtocolState, PacketIdMap> clientbound;
    std::map<protocol::ProtocolState, PacketIdMap> serverbound;
    for (const auto& [state, map] : clientbound_) {
        clientbound.emplace(state, map.inverted());
    }
    for (const auto& [state, map] : serverbound_) {
        serverbound.emplace(state, map.inverted());
    }
    return PacketIdTables{std::move(clientbound), std::move(serverbound)};
}

VersionTables invertedTables(const VersionTables& down, std::int32_t newer) {
    VersionTables up;
    up.older = newer;
    up.packets = down.packets.inverted();
    up.blockStates = down.blockStates.inverted();
    up.block = down.block.inverted();
    up.item = down.item.inverted();
    up.particleType = down.particleType.inverted();
    up.entityType = down.entityType.inverted();
    up.attribute = down.attribute.inverted();
    up.dataComponentType = down.dataComponentType.inverted();
    up.menu = down.menu.inverted();
    up.mobEffect = down.mobEffect.inverted();
    up.soundEvent = down.soundEvent.inverted();
    up.gameEvent = down.gameEvent.inverted();
    up.slotDisplay = down.slotDisplay.inverted();
    up.playerEntityType = down.newerPlayerEntityType;
    up.newerPlayerEntityType = down.playerEntityType;
    up.blockStateDirectBits = down.blockStateDirectBits;
    return up;
}

bool PacketIdTables::hasClientbound(protocol::ProtocolState state) const { return clientbound_.contains(state); }

bool PacketIdTables::hasServerbound(protocol::ProtocolState state) const { return serverbound_.contains(state); }

} // namespace basalt::wire::multiversion
