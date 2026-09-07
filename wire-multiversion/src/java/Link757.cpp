#include <basalt/wire/multiversion/java/Link757.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {

namespace play = protocol::play;

bool Link757::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LoginPlayPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = input.readInt();
        const std::uint8_t hardcore = input.readByte();
        const std::uint8_t gamemode = input.readByte();
        const std::uint8_t previous = input.readByte();
        const std::int32_t count = protocol::varInt::read(input);
        std::vector<std::string> worlds;
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            worlds.push_back(protocol::readString(input));
        }
        const nbt::NbtTag codec = protocol::readAnonymousNbt(input);
        remember(codec);
        const std::string dimensionType = protocol::readString(input);
        if (input.failed()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeInt(entityId);
            output.writeByte(hardcore);
            output.writeByte(gamemode);
            output.writeByte(previous);
            protocol::varInt::write(output, static_cast<std::int32_t>(worlds.size()));
            for (const std::string& world : worlds) {
                protocol::writeString(output, world);
            }
            protocol::writeAnonymousNbt(output, codec);
            writeDimension(output, dimensionType);
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::RespawnPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::string dimensionType = protocol::readString(input);
        if (input.failed()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            writeDimension(output, dimensionType);
            output.writeBytes(input.readable());
        });
        break;
    }
    default:
        break;
    }
    return true;
}

void Link757::remember(const nbt::NbtTag& codec) {
    const nbt::NbtCompound* root = codec.get<nbt::NbtCompound>();
    if (root == nullptr) {
        return;
    }
    const nbt::NbtCompound* registry = root->getCompound("minecraft:dimension_type");
    if (registry == nullptr) {
        return;
    }
    const nbt::NbtList* values = registry->getList("value");
    if (values == nullptr) {
        return;
    }
    dimensions_.clear();
    for (const nbt::NbtTag& entry : *values) {
        const nbt::NbtCompound* wrapped = entry.get<nbt::NbtCompound>();
        if (wrapped == nullptr) {
            continue;
        }
        const nbt::NbtTag* name = wrapped->find("name");
        const nbt::NbtTag* element = wrapped->find("element");
        if (name == nullptr || element == nullptr) {
            continue;
        }
        if (const auto* text = name->get<std::string>()) {
            dimensions_.emplace(*text, *element);
        }
    }
}

void Link757::writeDimension(io::ByteBuffer& output, const std::string& name) const {
    const auto found = dimensions_.find(name);
    protocol::writeAnonymousNbt(output, found == dimensions_.end() ? nbt::NbtTag::ofCompound(nbt::NbtCompound{})
                                                                   : found->second);
}

} // namespace basalt::wire::multiversion::java
