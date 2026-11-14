#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/Link109.hpp>
#include <basalt/wire/multiversion/java/NamedBlockEntities.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

/// The sign packet 1.9.4 folded into the block entity one, numbered as 1.9.2 has it.
constexpr std::int32_t kUpdateSign = 0x46;
constexpr std::uint8_t kSignAction = 9;
constexpr std::uint8_t kConduitAction = 5;
constexpr std::uint8_t kLastAction = 6;

// parity: the block entities 1.9.2 sends an update for, and the action each one carries.
constexpr std::array<std::pair<std::string_view, std::uint8_t>, 5> kActions = {{
    {"MobSpawner", 1},
    {"Control", 2},
    {"Beacon", 3},
    {"Skull", 4},
    {"Banner", 6},
}};

[[nodiscard]] std::int32_t blockEntityDataId() {
    static const std::int32_t id = javaTranslators::packetIdAt(protocolVersions::kV1_9_2, protocol::ProtocolState::Play,
                                                               true, play::BlockEntityDataPacket::kId);
    return id;
}

[[nodiscard]] std::vector<std::uint8_t> signUpdate(const nbt::NbtCompound& sign) {
    io::ByteBuffer out;
    out.writeLong(namedBlockEntities::packBefore1_14(sign.getInt("x"), sign.getInt("y"), sign.getInt("z")));
    for (const char* line : {"Text1", "Text2", "Text3", "Text4"}) {
        protocol::writeString(out, std::string{sign.getString(line, "\"\"")});
    }
    return out.toVector();
}

[[nodiscard]] std::uint8_t actionOf(std::string_view name) {
    for (const auto& [type, action] : kActions) {
        if (type == name) {
            return action;
        }
    }
    return 0;
}

} // namespace

bool Link109::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    if (packet.nativeId() == play::BlockEntityDataPacket::kId) {
        return separateSign(packet);
    }
    if (packet.nativeId() != play::MapChunkPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::uint8_t fullChunk = input.readByte();
    const std::int32_t mask = protocol::varInt::read(input);
    const std::int32_t length = protocol::varInt::read(input);
    const std::vector<std::uint8_t> data = input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length));
    if (input.failed()) {
        return true;
    }
    // Below 1.9.4 the chunk has no block entities: a sign, a skull or a spawner follows it in a packet of its own.
    const std::int32_t count = protocol::varInt::read(input);
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        const nbt::NbtTag tag = readNamedTag(input);
        const nbt::NbtCompound* entity = tag.get<nbt::NbtCompound>();
        if (entity == nullptr) {
            continue;
        }
        const std::string_view name = entity->getString("id");
        if (name == namedBlockEntities::kSignBefore1_11) {
            packet.extras.emplace_back(kUpdateSign, signUpdate(*entity));
        } else if (const std::uint8_t action = actionOf(name); action != 0) {
            io::ByteBuffer out;
            out.writeLong(
                namedBlockEntities::packBefore1_14(entity->getInt("x"), entity->getInt("y"), entity->getInt("z")));
            out.writeByte(action);
            writeNamedNbt(out, tag);
            packet.extras.emplace_back(blockEntityDataId(), out.toVector());
        }
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        output.writeByte(fullChunk);
        protocol::varInt::write(output, mask);
        protocol::varInt::write(output, static_cast<std::int32_t>(data.size()));
        output.writeBytes(data);
    });
    return true;
}

bool Link109::separateSign(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    static_cast<void>(input.readLong());
    const std::uint8_t action = input.readByte();
    const nbt::NbtTag tag = readNamedTag(input);
    const nbt::NbtCompound* entity = tag.get<nbt::NbtCompound>();
    if (input.failed() || entity == nullptr) {
        return false;
    }
    if (action == kSignAction) {
        packet.body = signUpdate(*entity);
        packet.overrideId(kUpdateSign);
        return true;
    }
    return action <= kLastAction && action != kConduitAction;
}

} // namespace basalt::wire::multiversion::java
