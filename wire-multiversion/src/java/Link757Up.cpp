#include <basalt/wire/multiversion/java/Link757Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kLogin = 38;
constexpr std::int32_t kRemoveMobEffect = 59;
constexpr std::int32_t kRespawn = 61;
constexpr std::int32_t kUpdateMobEffect = 101;

void prefixInfiniburn(nbt::NbtTag& element) {
    nbt::NbtCompound* body = element.get<nbt::NbtCompound>();
    nbt::NbtTag* burn = body != nullptr ? body->find("infiniburn") : nullptr;
    std::string* name = burn != nullptr ? burn->get<std::string>() : nullptr;
    if (name != nullptr && !name->starts_with('#')) {
        name->insert(name->begin(), '#');
    }
}

void prefixCodec(nbt::NbtTag& codec) {
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
            prefixInfiniburn(*element);
        }
    }
}

} // namespace

bool Link757Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kUpdateMobEffect:
    case kRemoveMobEffect:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            pv::varInt::write(out, pv::varInt::read(in));
            pv::varInt::write(out, in.readByte());
            out.writeBytes(in.readable());
        });
        break;
    case kLogin:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            io::ByteBuffer head;
            head.writeBytes(in.readSpan(7));
            const std::int32_t worlds = pv::readCollectionSize(in, 1);
            pv::varInt::write(head, worlds);
            for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
                pv::writeString(head, pv::readString(in));
            }
            auto codec = readNamedNbt(in);
            auto dimension = readNamedNbt(in);
            if (in.failed()) {
                return;
            }
            prefixCodec(codec->tag);
            prefixInfiniburn(dimension->tag);
            out.writeBytes(head.readable());
            writeNamedNbt(out, codec->tag);
            writeNamedNbt(out, dimension->tag);
            out.writeBytes(in.readable());
        });
        break;
    case kRespawn:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            auto dimension = readNamedNbt(in);
            if (in.failed()) {
                return;
            }
            prefixInfiniburn(dimension->tag);
            writeNamedNbt(out, dimension->tag);
            out.writeBytes(in.readable());
        });
        break;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
