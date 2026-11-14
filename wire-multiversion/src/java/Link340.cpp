#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Link340.hpp>
#include <basalt/wire/multiversion/java/MerchantShapes.hpp>
#include <basalt/wire/multiversion/java/OlderPaintings.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::int32_t kEmptyItem = -1;
constexpr std::size_t kUuidBytes = 16;

constexpr std::int32_t kColumns = 16 * 16;

constexpr std::int32_t kItemMask = 0xFFFF;
constexpr int kDamageShift = 16;

[[nodiscard]] std::vector<std::uint8_t> oldStack(io::ByteBuffer& input) {
    io::ByteBuffer output;
    if (input.readByte() == 0) {
        output.writeShort(static_cast<std::int16_t>(kEmptyItem));
        return output.toVector();
    }
    const std::int32_t packed = protocol::varInt::read(input);
    const std::uint8_t count = input.readByte();
    output.writeShort(static_cast<std::int16_t>(packed & kItemMask));
    output.writeByte(count);
    output.writeShort(static_cast<std::int16_t>(static_cast<std::uint32_t>(packed) >> kDamageShift));
    static_cast<void>(legacyStacks::copyTag(input, output, legacyStacks::Root::kNamed, legacyStacks::Root::kNamed));
    return output.toVector();
}

/// And back: what the client sends is an id and a damage, and above this it is a name's id.
[[nodiscard]] std::vector<std::uint8_t> newStack(io::ByteBuffer& input) {
    io::ByteBuffer output;
    const std::int32_t itemId = input.readShort();
    if (itemId == kEmptyItem) {
        protocol::writeBoolean(output, false);
        return output.toVector();
    }
    const std::uint8_t count = input.readByte();
    const std::int32_t damage = input.readShort();
    protocol::writeBoolean(output, true);
    protocol::varInt::write(output, (damage << kDamageShift) | itemId);
    output.writeByte(count);
    static_cast<void>(legacyStacks::copyTag(input, output, legacyStacks::Root::kNamed, legacyStacks::Root::kNamed));
    return output.toVector();
}

bool copyOld(io::ByteBuffer& input, io::ByteBuffer& output) {
    output.writeBytes(oldStack(input));
    return !input.failed();
}

constexpr std::string_view kLegacyTradeSelect = "MC|TrSel";
constexpr std::string_view kLegacyBookEdit = "MC|BEdit";
constexpr std::string_view kLegacyBookSign = "MC|BSign";
constexpr std::string_view kLegacyItemName = "MC|ItemName";

struct ChannelName {
    std::string_view legacy;
    std::string_view named;
};

// parity: the plugin channels 1.13 gave a namespace, which the newer server would refuse in their old spelling.
constexpr std::array<ChannelName, 4> kRenamedChannels = {{
    {"MC|Brand", "minecraft:brand"},
    {"REGISTER", "minecraft:register"},
    {"UNREGISTER", "minecraft:unregister"},
    {"BungeeCord", "bungeecord:main"},
}};

/// The channels under their pre-1.13 names; the trades also carry their stacks as ids and damage.
void renameClientChannel(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string channel = protocol::readString(input);
    io::ByteBuffer output;
    if (channel == merchantShapes::kTraderList) {
        protocol::writeString(output, merchantShapes::kLegacyTraderList);
        if (merchantShapes::reshape(input, output, merchantShapes::k1_13, merchantShapes::k1_13, &copyOld)) {
            packet.body = output.toVector();
        }
        return;
    }
    const auto renamed = std::ranges::find(kRenamedChannels, channel, &ChannelName::named);
    if (input.failed() || renamed == kRenamedChannels.end()) {
        return;
    }
    protocol::writeString(output, renamed->legacy);
    output.writeBytes(input.readable());
    packet.body = output.toVector();
}

[[nodiscard]] bool validChannel(std::string_view channel) {
    const std::size_t colon = channel.find(':');
    return !channel.empty() && std::ranges::all_of(channel, [&](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':' ||
               (c == '/' && colon != std::string_view::npos);
    }) && std::ranges::count(channel, ':') <= 1;
}

void become(MutablePacket& packet, std::int32_t id, io::ByteBuffer& body) {
    packet.id = id;
    packet.becomes(id);
    packet.body = body.toVector();
}

/// Before 1.13 trades, books and names travelled on MC| channels; above they are packets of their own.
bool rewriteChannel(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string channel = protocol::readString(input);
    if (input.failed()) {
        return false;
    }
    io::ByteBuffer output;
    if (channel == kLegacyTradeSelect) {
        protocol::varInt::write(output, input.readInt());
        become(packet, play::SelectTradePacket::kId, output);
    } else if (channel == kLegacyBookEdit || channel == kLegacyBookSign) {
        output.writeBytes(newStack(input));
        protocol::writeBoolean(output, channel == kLegacyBookSign);
        protocol::varInt::write(output, 0);
        become(packet, play::ServerboundEditBookPacket::kId, output);
    } else if (channel == kLegacyItemName) {
        protocol::writeString(output, protocol::readString(input));
        become(packet, play::ServerboundRenameItemPacket::kId, output);
    } else {
        const auto renamed = std::ranges::find(kRenamedChannels, channel, &ChannelName::legacy);
        if (renamed == kRenamedChannels.end()) {
            return validChannel(channel);
        }
        protocol::writeString(output, renamed->named);
        output.writeBytes(input.readable());
        packet.body = output.toVector();
    }
    return !input.failed();
}

/// The suggestions as the words 1.12 put in place of the last one typed, a command's own name with its slash.
bool suggestionsAsWords(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    static_cast<void>(protocol::varInt::read(input));
    const std::int32_t start = protocol::varInt::read(input);
    static_cast<void>(protocol::varInt::read(input));
    const std::int32_t count = protocol::readCollectionSize(input);
    std::vector<std::string> words;
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        std::string word = protocol::readString(input);
        if (protocol::readBoolean(input)) {
            static_cast<void>(protocol::readString(input));
        }
        words.push_back(start <= 1 ? "/" + word : std::move(word));
    }
    if (input.failed()) {
        return false;
    }
    io::ByteBuffer output;
    protocol::varInt::write(output, static_cast<std::int32_t>(words.size()));
    for (const std::string& word : words) {
        protocol::writeString(output, word);
    }
    packet.body = output.toVector();
    return true;
}

/// Before 1.13 a painting's motive was named by its title.
void titlePainting(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    protocol::varInt::write(output, protocol::varInt::read(input));
    output.writeBytes(input.readSpan(kUuidBytes));
    protocol::writeString(output, OlderPaintings::title(protocol::varInt::read(input)));
    output.writeBytes(input.readable());
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

} // namespace

bool Link340::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LevelParticlesPacket::kId:
    case play::AwardStatsPacket::kId:
        return false;
    case play::CommandSuggestionsPacket::kId:
        return suggestionsAsWords(packet);
    case play::MapChunkPacket::kId:
        narrowBiomes(packet);
        break;
    case play::SpawnEntityPacket::kId:
        if (packet.spawn == MutablePacket::Spawn::kPainting) {
            titlePainting(packet);
        }
        break;
    case play::CustomPayloadPacket::kClientboundId:
        renameClientChannel(packet);
        break;
    case play::ContainerSetSlotPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int16_t slot = input.readShort();
        const std::vector<std::uint8_t> stack = oldStack(input);
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            output.writeShort(slot);
            output.writeBytes(stack);
        });
        break;
    }
    case play::ContainerSetContentPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int16_t count = input.readShort();
        io::ByteBuffer stacks;
        for (std::int16_t i = 0; i < count && !input.failed(); ++i) {
            stacks.writeBytes(oldStack(input));
        }
        const std::vector<std::uint8_t> items = stacks.toVector();
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            output.writeShort(count);
            output.writeBytes(items);
        });
        break;
    }
    case play::SetEquipmentPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int32_t slot = protocol::varInt::read(input);
        const std::vector<std::uint8_t> stack = oldStack(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            protocol::varInt::write(output, slot);
            output.writeBytes(stack);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link340::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::CustomPayloadPacket::kServerboundId:
        return rewriteChannel(packet);
    case play::CommandSuggestionRequestPacket::kId: {
        // Before 1.13 the client asked about any chat line; only a command still gets an answer.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::string line = protocol::readString(input);
        if (input.failed() || !line.starts_with('/')) {
            return false;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, 0);
            protocol::writeString(output, line);
        });
        break;
    }
    case play::ServerboundSetCreativeModeSlotPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int16_t slot = input.readShort();
        const std::vector<std::uint8_t> stack = newStack(input);
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeShort(slot);
            output.writeBytes(stack);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

void Link340::narrowBiomes(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::uint8_t fullChunk = input.readByte();
    const std::int32_t mask = protocol::varInt::read(input);
    const std::int32_t length = protocol::varInt::read(input);
    const std::vector<std::uint8_t> data = input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length));
    constexpr std::size_t kBiomeBytes = static_cast<std::size_t>(kColumns) * sizeof(std::int32_t);
    if (input.failed() || fullChunk == 0 || data.size() < kBiomeBytes) {
        return;
    }
    const std::size_t sectionBytes = data.size() - kBiomeBytes;
    io::ByteBuffer biomes =
        io::ByteBuffer::wrap({data.begin() + static_cast<std::ptrdiff_t>(sectionBytes), data.end()});
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        output.writeByte(fullChunk);
        protocol::varInt::write(output, mask);
        protocol::varInt::write(output, static_cast<std::int32_t>(sectionBytes) + kColumns);
        output.writeBytes(std::span{data}.first(sectionBytes));
        for (std::int32_t i = 0; i < kColumns; ++i) {
            output.writeByte(static_cast<std::uint8_t>(biomes.readInt()));
        }
        output.writeBytes(input.readable());
    });
}

} // namespace basalt::wire::multiversion::java
