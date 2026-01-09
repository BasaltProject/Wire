#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion {
namespace {

using Carried = std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>>;

/// The same element without the attribute keys that version has never heard of.
std::optional<nbt::NbtTag> withoutAttributes(const std::optional<nbt::NbtTag>& value,
                                             const RegistryRemovals& removals) {
    if (removals.attributes.empty() || !value) {
        return value;
    }
    const auto* element = value->get<nbt::NbtCompound>();
    if (element == nullptr) {
        return value;
    }
    const nbt::NbtCompound* attributes = element->getCompound("attributes");
    if (attributes == nullptr) {
        return value;
    }

    bool anyUnknown = false;
    for (const std::string& name : attributes->names()) {
        if (removals.attributes.contains(name)) {
            anyUnknown = true;
            break;
        }
    }
    if (!anyUnknown) {
        return value;
    }

    nbt::NbtCompound kept;
    for (std::size_t i = 0; i < attributes->size(); ++i) {
        if (!removals.attributes.contains(attributes->names()[i])) {
            kept.set(attributes->names()[i], attributes->values()[i]);
        }
    }
    nbt::NbtCompound rewritten = *element;
    rewritten.set("attributes", nbt::NbtTag::ofCompound(std::move(kept)));
    return nbt::NbtTag::ofCompound(std::move(rewritten));
}

/// Takes out of a registry packet what this link's older side cannot read.
///
/// False when nothing is left worth sending, which is a registry that version does not have at all:
/// an empty one is not the same as none, and the client is better off never hearing of it.
bool stripRegistry(protocol::ProtocolState state, MutablePacket& packet, const ProtocolLink& link) {
    if (state != protocol::ProtocolState::Configuration ||
        packet.nativeId() != protocol::configuration::RegistryDataPacket::kId) {
        return true;
    }
    const RegistryRemovals& removals = link.registryRemovals();
    if (removals.empty()) {
        return true;
    }

    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const auto data = protocol::configuration::RegistryDataPacket::read(input);
    if (!data) {
        return true;
    }
    if (removals.entries.contains(data->registryId + "/*")) {
        return false;
    }

    protocol::configuration::RegistryDataPacket kept;
    kept.registryId = data->registryId;
    kept.entries.reserve(data->entries.size());
    for (const protocol::configuration::RegistryEntry& entry : data->entries) {
        if (removals.entries.contains(data->registryId + "/" + entry.key)) {
            continue;
        }
        kept.entries.push_back(
            protocol::configuration::RegistryEntry{entry.key, withoutAttributes(entry.value, removals)});
    }

    if (kept.entries.size() == data->entries.size() && removals.attributes.empty()) {
        return true;
    }
    io::ByteBuffer output;
    kept.write(output);
    packet.body = output.toVector();
    return true;
}

/// The extras of a link above are renumbered by every link below it, like any other packet.
void renumberCarried(Carried& carried, protocol::ProtocolState state, const ProtocolLink& link) {
    if (carried.empty()) {
        return;
    }
    std::size_t kept = 0;
    for (std::size_t index = 0; index < carried.size(); ++index) {
        const std::int32_t mapped = link.packets().toOlder(state, carried[index].first);
        if (mapped == PacketIdMap::kDropped) {
            continue;
        }
        carried[kept].first = mapped;
        if (kept != index) {
            carried[kept].second = std::move(carried[index].second);
        }
        ++kept;
    }
    carried.resize(kept);
}

} // namespace

void MutablePacket::reset(std::int32_t newId, std::int32_t newNativeId, std::vector<std::uint8_t> newBody) {
    id = newId;
    nativeId_ = newNativeId;
    body = std::move(newBody);
    idOverridden_ = false;
    extras.clear();
}

bool MutablePacket::takeOverride() noexcept {
    if (!idOverridden_) {
        return false;
    }
    idOverridden_ = false;
    return true;
}

const RegistryRemovals& ProtocolLink::registryRemovals() const {
    static const RegistryRemovals none;
    return none;
}

void ProtocolChain::clientbound(protocol::ProtocolState state, std::int32_t id, std::span<const std::uint8_t> body,
                                const Emit& emit) const {
    // One packet and one list per call, not one per chain: a session is written to by whatever
    // thread has something to say to it, and a translation that kept its state on the chain was
    // being torn in half by the second writer.
    MutablePacket packet;
    Carried carried;
    packet.reset(id, id, std::vector<std::uint8_t>{body.begin(), body.end()});

    if (content_ != nullptr && !content_->clientbound(state, packet)) {
        return;
    }
    // The rewriter's own extras are in this server's numbering, so they are carried before the first
    // link rather than after it: every link renumbers them, the same as the packet itself.
    if (!packet.extras.empty()) {
        carried.insert(carried.end(), std::make_move_iterator(packet.extras.begin()),
                       std::make_move_iterator(packet.extras.end()));
        packet.extras.clear();
    }

    std::int32_t current = id;
    bool dropped = false;
    for (ProtocolLink& link : links_) {
        if (!dropped && !stripRegistry(state, packet, link)) {
            dropped = true;
        }
        if (!dropped && !link.clientbound(state, packet)) {
            dropped = true;
        }
        renumberCarried(carried, state, link);
        if (!packet.extras.empty()) {
            carried.insert(carried.end(), std::make_move_iterator(packet.extras.begin()),
                           std::make_move_iterator(packet.extras.end()));
            packet.extras.clear();
        }
        if (dropped) {
            continue;
        }

        // A link that named the packet itself replaces its own table's answer, and the links below
        // go on renumbering from the name it gave.
        if (packet.takeOverride()) {
            current = packet.id;
            continue;
        }
        const std::int32_t mapped = link.packets().toOlder(state, current);
        if (mapped == PacketIdMap::kDropped) {
            dropped = true;
        } else {
            current = mapped;
            // Kept up to date so a link below can ask what this packet is called where it sits.
            packet.id = current;
        }
    }

    if (!dropped) {
        emit(current, packet.body);
    }
    for (const auto& [extraId, extraBody] : carried) {
        emit(extraId, extraBody);
    }
}

void ProtocolChain::serverbound(protocol::ProtocolState state, std::int32_t id, std::span<const std::uint8_t> body,
                                const Emit& emit) const {
    std::int32_t current = id;
    for (auto link = links_.rbegin(); link != links_.rend(); ++link) {
        const std::int32_t mapped = link->get().packets().toNative(state, current);
        if (mapped == PacketIdMap::kDropped) {
            return;
        }
        current = mapped;
    }

    MutablePacket packet;
    packet.reset(current, current, std::vector<std::uint8_t>{body.begin(), body.end()});
    for (auto link = links_.rbegin(); link != links_.rend(); ++link) {
        if (!link->get().serverbound(state, packet)) {
            return;
        }
    }
    if (content_ != nullptr && !content_->serverbound(state, packet)) {
        return;
    }

    emit(packet.id, packet.body);
    for (const auto& [extraId, extraBody] : packet.extras) {
        emit(extraId, extraBody);
    }
}

} // namespace basalt::wire::multiversion
