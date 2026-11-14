#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Link755.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kOffhandSlot = 40;
constexpr std::size_t kMaxPages = play::ServerboundEditBookPacket::kMaxPages;

constexpr std::int32_t kRemoveEntity755 = 58;

} // namespace

bool Link755::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::RemoveEntitiesPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t count = protocol::varInt::read(input);
        std::vector<std::int32_t> ids;
        ids.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            ids.push_back(protocol::varInt::read(input));
        }
        if (ids.empty()) {
            return false;
        }
        packet.overrideId(kRemoveEntity755);
        encode(packet, [&](io::ByteBuffer& out) { protocol::varInt::write(out, ids.front()); });
        for (std::size_t i = 1; i < ids.size(); ++i) {
            io::ByteBuffer one;
            protocol::varInt::write(one, ids[i]);
            packet.continued.push_back({kRemoveEntity755, play::RemoveEntitiesPacket::kId, one.toVector()});
        }
        break;
    }
    case play::ContainerSetSlotPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t containerId = input.readByte();
        static_cast<void>(protocol::varInt::read(input));
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(containerId);
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::ContainerSetContentPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t containerId = input.readByte();
        static_cast<void>(protocol::varInt::read(input));
        const std::int32_t count = protocol::varInt::read(input);
        io::ByteBuffer items;
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            static_cast<void>(
                legacyStacks::copyFlagged(input, items, legacyStacks::Root::kNamed, legacyStacks::Root::kNamed));
        }
        const std::vector<std::uint8_t> slots = items.toVector();
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(containerId);
            output.writeShort(static_cast<std::int16_t>(count));
            output.writeBytes(slots);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link755::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::ServerboundContainerClickPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        if (input.failed()) {
            return false;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            protocol::varInt::write(output, 0);
            output.writeBytes(input.readable());
        });
        return true;
    }
    case play::ServerboundSetCarriedItemPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int16_t slot = input.readShort();
        if (!input.failed() && slot >= 0 && slot < play::ServerboundSetCarriedItemPacket::kHotbarSize) {
            selected_ = slot;
        }
        return true;
    }
    case play::ServerboundEditBookPacket::kId:
        return rewriteBook(packet);
    default:
        return true;
    }
}

bool Link755::rewriteBook(MutablePacket& packet) const {
    // The book went up whole until 1.17.1, which sends only its pages and title, and where it is held.
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    if (!protocol::readBoolean(input)) {
        return false;
    }
    static_cast<void>(protocol::varInt::read(input));
    static_cast<void>(input.readByte());
    const nbt::NbtTag tag = readNamedTag(input);
    const bool signing = protocol::readBoolean(input);
    const std::int32_t hand = input.readableBytes() > 0 ? protocol::varInt::read(input) : 0;
    const nbt::NbtCompound* book = tag.get<nbt::NbtCompound>();
    if (input.failed() || book == nullptr) {
        return false;
    }
    play::ServerboundEditBookPacket edit;
    edit.slot = hand == 0 ? selected_ : kOffhandSlot;
    if (const nbt::NbtList* pages = book->getList("pages")) {
        for (const nbt::NbtTag& page : *pages) {
            if (const auto* text = page.get<std::string>(); text != nullptr && edit.pages.size() < kMaxPages) {
                edit.pages.push_back(*text);
            }
        }
    }
    if (signing) {
        edit.title = std::string{book->getString("title")};
    }
    encode(packet, [&](io::ByteBuffer& output) { edit.write(output); });
    return true;
}

} // namespace basalt::wire::multiversion::java
