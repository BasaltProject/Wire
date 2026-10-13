#include <basalt/wire/multiversion/java/Link757Up.hpp>
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
            out.writeBytes(in.readSpan(7));
            const std::int32_t worlds = pv::readCollectionSize(in, 1);
            pv::varInt::write(out, worlds);
            for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
                pv::writeString(out, pv::readString(in));
            }
            nbt::NbtTag codec = pv::readAnonymousNbt(in);
            nbt::NbtTag dimension = pv::readAnonymousNbt(in);
            if (in.failed()) {
                return;
            }
            prefixCodec(codec);
            prefixInfiniburn(dimension);
            pv::writeAnonymousNbt(out, codec);
            pv::writeAnonymousNbt(out, dimension);
            out.writeBytes(in.readable());
        });
        break;
    case kRespawn:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            nbt::NbtTag dimension = pv::readAnonymousNbt(in);
            if (in.failed()) {
                return;
            }
            prefixInfiniburn(dimension);
            pv::writeAnonymousNbt(out, dimension);
            out.writeBytes(in.readable());
        });
        break;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
