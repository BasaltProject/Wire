#include <basalt/wire/multiversion/java/Link757.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::size_t kLoginModeBytes = 3;

/// parity: `DimensionType.infiniburn`, which became a tag reference with its `#` in 1.18.2.
void unprefixInfiniburn(nbt::NbtTag& element) {
    nbt::NbtCompound* body = element.get<nbt::NbtCompound>();
    nbt::NbtTag* burn = body != nullptr ? body->find("infiniburn") : nullptr;
    std::string* name = burn != nullptr ? burn->get<std::string>() : nullptr;
    if (name != nullptr && name->starts_with('#')) {
        name->erase(name->begin());
    }
}

void unprefixCodec(nbt::NbtTag& codec) {
    nbt::NbtCompound* root = codec.get<nbt::NbtCompound>();
    nbt::NbtTag* registry = root != nullptr ? root->find("minecraft:dimension_type") : nullptr;
    nbt::NbtCompound* types = registry != nullptr ? registry->get<nbt::NbtCompound>() : nullptr;
    nbt::NbtTag* values = types != nullptr ? types->find("value") : nullptr;
    nbt::NbtList* list = values != nullptr ? values->get<nbt::NbtList>() : nullptr;
    if (list == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < list->size(); ++index) {
        nbt::NbtCompound* entry = (*list)[index].get<nbt::NbtCompound>();
        nbt::NbtTag* element = entry != nullptr ? entry->find("element") : nullptr;
        if (element != nullptr) {
            unprefixInfiniburn(*element);
        }
    }
}

} // namespace

bool Link757::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LoginPlayPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer output;
        output.writeInt(input.readInt());
        output.writeBytes(input.readSpan(kLoginModeBytes));
        const std::int32_t count = protocol::varInt::read(input);
        protocol::varInt::write(output, count);
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            protocol::writeString(output, protocol::readString(input));
        }
        nbt::NbtTag codec = readNamedTag(input);
        unprefixCodec(codec);
        writeNamedNbt(output, codec);
        nbt::NbtTag dimension = readNamedTag(input);
        unprefixInfiniburn(dimension);
        writeNamedNbt(output, dimension);
        output.writeBytes(input.readable());
        if (!input.failed()) {
            packet.body = output.toVector();
        }
        break;
    }
    case play::RespawnPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        nbt::NbtTag dimension = readNamedTag(input);
        unprefixInfiniburn(dimension);
        if (input.failed()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            writeNamedNbt(output, dimension);
            output.writeBytes(input.readable());
        });
        break;
    }
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
