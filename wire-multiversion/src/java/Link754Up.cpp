#include <basalt/wire/multiversion/java/Link754Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kContainerAck = 0x11;
constexpr std::int32_t kExplode = 0x1B;
constexpr std::int32_t kChunk = 0x20;
constexpr std::int32_t kLight = 0x23;
constexpr std::int32_t kLogin = 0x24;
constexpr std::int32_t kMapData = 0x25;
constexpr std::int32_t kMoveEntity = 0x2A;
constexpr std::int32_t kCombat = 0x31;
constexpr std::int32_t kPosition = 0x34;
constexpr std::int32_t kRemoveEntities = 0x36;
constexpr std::int32_t kResourcePack = 0x38;
constexpr std::int32_t kRespawn = 0x39;
constexpr std::int32_t kBorder = 0x3D;
constexpr std::int32_t kDefaultSpawn = 0x42;
constexpr std::int32_t kTitles = 0x4F;
constexpr std::int32_t kAttributes = 0x58;
constexpr std::int32_t kTags = 0x5B;

/// parity: `ClientboundPacket` ids of 1.17 that 1.16.5 had no single packet for.
constexpr std::int32_t kPing = 0x30;
constexpr std::int32_t kClearTitles = 0x10;
constexpr std::int32_t kInitializeBorder = 0x20;
constexpr std::int32_t kCombatEnd = 0x33;
constexpr std::int32_t kCombatEnter = 0x34;
constexpr std::int32_t kCombatKill = 0x35;
constexpr std::int32_t kRemoveEntity = 0x3A;
constexpr std::int32_t kActionBar = 0x41;
constexpr std::int32_t kBorderCenter = 0x42;
constexpr std::int32_t kBorderLerp = 0x43;
constexpr std::int32_t kBorderSize = 0x44;
constexpr std::int32_t kBorderDelay = 0x45;
constexpr std::int32_t kBorderDistance = 0x46;
constexpr std::int32_t kSubtitle = 0x57;
constexpr std::int32_t kTitle = 0x59;
constexpr std::int32_t kTitleTimes = 0x5A;

constexpr std::int32_t kPlayClientInformation = 0x05;
constexpr std::int32_t kPlayClick = 0x09;
/// parity: `ServerboundPongPacket`, which 1.16.5 has no counterpart for.
constexpr std::int32_t kPlayPong = 0x1D;
constexpr std::int32_t kServerContainerAck = 0x07;

constexpr std::int32_t kWorldHeight = 256;
constexpr std::int32_t kPongInventoryBit = 1 << 30;
constexpr std::int32_t kMaxClickSlots = 128;
constexpr std::array<std::string_view, 4> kTagRegistries = {"minecraft:block", "minecraft:item", "minecraft:fluid",
                                                            "minecraft:entity_type"};

void addHeight(nbt::NbtTag& tag) {
    nbt::NbtCompound* body = tag.get<nbt::NbtCompound>();
    if (body != nullptr) {
        body->putInt("min_y", 0);
        body->putInt("height", kWorldHeight);
    }
}

void addHeights(nbt::NbtTag& codec) {
    nbt::NbtCompound* root = codec.get<nbt::NbtCompound>();
    nbt::NbtTag* registry = root != nullptr ? root->find("minecraft:dimension_type") : nullptr;
    nbt::NbtCompound* types = registry != nullptr ? registry->get<nbt::NbtCompound>() : nullptr;
    nbt::NbtTag* values = types != nullptr ? types->find("value") : nullptr;
    nbt::NbtList* list = values != nullptr ? values->get<nbt::NbtList>() : nullptr;
    for (std::size_t index = 0; list != nullptr && index < list->size(); ++index) {
        nbt::NbtCompound* entry = (*list)[index].get<nbt::NbtCompound>();
        nbt::NbtTag* element = entry != nullptr ? entry->find("element") : nullptr;
        if (element != nullptr) {
            addHeight(*element);
        }
    }
}

void copyStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::uint8_t present = in.readByte();
    out.writeByte(present);
    if (present == 0) {
        return;
    }
    pv::varInt::write(out, pv::varInt::read(in));
    out.writeByte(in.readByte());
    const auto tag = readNamedNbt(in);
    if (!in.failed()) {
        out.writeBytes(tag->raw);
    }
}

void skipStack(io::ByteBuffer& in) {
    io::ByteBuffer sink;
    copyStack(in, sink);
}

void writeBitMask(io::ByteBuffer& out, std::int32_t mask) {
    pv::varInt::write(out, 1);
    out.writeLong(static_cast<std::uint32_t>(mask));
}

void convertLight(io::ByteBuffer& in, io::ByteBuffer& out) {
    pv::varInt::write(out, pv::varInt::read(in));
    pv::varInt::write(out, pv::varInt::read(in));
    out.writeByte(in.readByte());
    const std::int32_t sky = pv::varInt::read(in);
    const std::int32_t block = pv::varInt::read(in);
    const std::int32_t emptySky = pv::varInt::read(in);
    const std::int32_t emptyBlock = pv::varInt::read(in);
    writeBitMask(out, sky);
    writeBitMask(out, block);
    writeBitMask(out, emptySky);
    writeBitMask(out, emptyBlock);
    for (const std::int32_t mask : {sky, block}) {
        std::int32_t arrays = 0;
        io::ByteBuffer body;
        for (int bit = 0; bit < 18 && !in.failed(); ++bit) {
            if (((mask >> bit) & 1) != 0) {
                const std::int32_t length = pv::readCollectionSize(in, 1);
                pv::varInt::write(body, length);
                body.writeBytes(in.readSpan(static_cast<std::size_t>(length)));
                arrays++;
            }
        }
        pv::varInt::write(out, arrays);
        out.writeBytes(body.readable());
    }
}

} // namespace

bool Link754Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kLogin: {
        out.writeBytes(in.readSpan(7));
        const std::int32_t worlds = pv::readCollectionSize(in, 1);
        pv::varInt::write(out, worlds);
        for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
            pv::writeString(out, pv::readString(in));
        }
        auto codec = readNamedNbt(in);
        auto dimension = readNamedNbt(in);
        if (in.failed()) {
            return false;
        }
        addHeights(codec->tag);
        addHeight(dimension->tag);
        writeNamedNbt(out, codec->tag);
        writeNamedNbt(out, dimension->tag);
        out.writeBytes(in.readable());
        break;
    }
    case kRespawn: {
        auto dimension = readNamedNbt(in);
        if (in.failed()) {
            return false;
        }
        addHeight(dimension->tag);
        writeNamedNbt(out, dimension->tag);
        out.writeBytes(in.readable());
        break;
    }
    case kLight:
        convertLight(in, out);
        break;
    case kChunk: {
        out.writeInt(in.readInt());
        out.writeInt(in.readInt());
        if (in.readByte() == 0) {
            return false;
        }
        writeBitMask(out, pv::varInt::read(in));
        const auto heightmaps = readNamedNbt(in);
        if (in.failed()) {
            return false;
        }
        out.writeBytes(heightmaps->raw);
        out.writeBytes(in.readable());
        break;
    }
    case kBorder: {
        static constexpr std::array<std::int32_t, 6> kIds = {kBorderSize,       kBorderLerp,  kBorderCenter,
                                                             kInitializeBorder, kBorderDelay, kBorderDistance};
        const std::int32_t type = pv::varInt::read(in);
        if (type < 0 || type >= static_cast<std::int32_t>(kIds.size())) {
            return false;
        }
        packet.overrideId(kIds[static_cast<std::size_t>(type)]);
        out.writeBytes(in.readable());
        break;
    }
    case kTitles: {
        const std::int32_t type = pv::varInt::read(in);
        switch (type) {
        case 0:
            packet.overrideId(kTitle);
            out.writeBytes(in.readable());
            break;
        case 1:
            packet.overrideId(kSubtitle);
            out.writeBytes(in.readable());
            break;
        case 2:
            packet.overrideId(kActionBar);
            out.writeBytes(in.readable());
            break;
        case 3:
            packet.overrideId(kTitleTimes);
            out.writeBytes(in.readable());
            break;
        case 4:
        case 5:
            packet.overrideId(kClearTitles);
            out.writeByte(type == 5 ? 1 : 0);
            break;
        default:
            return false;
        }
        break;
    }
    case kTags: {
        pv::varInt::write(out, 5);
        for (const std::string_view registry : kTagRegistries) {
            pv::writeString(out, std::string{registry});
            const std::int32_t tags = pv::readCollectionSize(in, 1);
            pv::varInt::write(out, tags);
            for (std::int32_t tag = 0; tag < tags && !in.failed(); ++tag) {
                pv::writeString(out, pv::readString(in));
                const std::int32_t ids = pv::readCollectionSize(in, 1);
                pv::varInt::write(out, ids);
                for (std::int32_t id = 0; id < ids && !in.failed(); ++id) {
                    pv::varInt::write(out, pv::varInt::read(in));
                }
            }
        }
        pv::writeString(out, "minecraft:game_event");
        pv::varInt::write(out, 0);
        break;
    }
    case kResourcePack:
        out.writeBytes(in.readable());
        out.writeByte(0);
        out.writeByte(0);
        break;
    case kMapData: {
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeByte(in.readByte());
        static_cast<void>(in.readByte());
        out.writeByte(in.readByte());
        const std::int32_t icons = pv::varInt::read(in);
        out.writeByte(icons != 0 ? 1 : 0);
        if (icons != 0) {
            pv::varInt::write(out, icons);
        }
        out.writeBytes(in.readable());
        break;
    }
    case kExplode:
        out.writeBytes(in.readSpan(16));
        pv::varInt::write(out, in.readInt());
        out.writeBytes(in.readable());
        break;
    case kAttributes:
        pv::varInt::write(out, pv::varInt::read(in));
        pv::varInt::write(out, in.readInt());
        out.writeBytes(in.readable());
        break;
    case kDefaultSpawn:
        out.writeBytes(in.readable());
        out.writeInt(0);
        break;
    case kPosition:
        out.writeBytes(in.readable());
        out.writeByte(0);
        break;
    case kCombat: {
        const std::int32_t type = pv::varInt::read(in);
        packet.overrideId(type == 0 ? kCombatEnter : (type == 1 ? kCombatEnd : kCombatKill));
        if (type < 0 || type > 2) {
            return false;
        }
        out.writeBytes(in.readable());
        break;
    }
    case kRemoveEntities: {
        const std::int32_t count = pv::readCollectionSize(in, 1);
        if (count == 0 || in.failed()) {
            return false;
        }
        const std::int32_t first = pv::varInt::read(in);
        for (std::int32_t index = 1; index < count && !in.failed(); ++index) {
            io::ByteBuffer one;
            pv::varInt::write(one, pv::varInt::read(in));
            packet.forwarded.emplace_back(kRemoveEntity, one.toVector());
        }
        packet.overrideId(kRemoveEntity);
        pv::varInt::write(out, first);
        break;
    }
    case kContainerAck: {
        const std::int32_t container = in.readByte();
        const std::int32_t action = static_cast<std::uint16_t>(in.readShort());
        if (in.readByte() != 0 || in.failed()) {
            return false;
        }
        packet.overrideId(kPing);
        out.writeInt(kPongInventoryBit | (container << 16) | action);
        break;
    }
    case kMoveEntity:
        return false;
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link754Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    if (packet.id == kPlayClientInformation) {
        if (!packet.body.empty()) {
            packet.body.pop_back();
        }
        return true;
    }
    if (packet.id == kPlayClick) {
        out.writeByte(in.readByte());
        out.writeShort(in.readShort());
        out.writeByte(in.readByte());
        out.writeShort(0);
        const std::int32_t mode = pv::varInt::read(in);
        pv::varInt::write(out, mode);
        const std::int32_t changed = pv::readCollectionSize(in, 1);
        for (std::int32_t index = 0; index < std::min(changed, kMaxClickSlots) && !in.failed(); ++index) {
            static_cast<void>(in.readShort());
            skipStack(in);
        }
        if (mode == 1 || mode == 5) {
            skipStack(in);
            out.writeByte(0);
        } else {
            copyStack(in, out);
        }
    } else {
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link754Up::serverboundForeign(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.id != kPlayPong) {
        return false;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t id = in.readInt();
    if (in.failed() || (id & kPongInventoryBit) == 0) {
        return false;
    }
    io::ByteBuffer out;
    out.writeByte(static_cast<std::uint8_t>((id >> 16) & 0xFF));
    out.writeShort(static_cast<std::int16_t>(id & 0xFFFF));
    out.writeByte(1);
    packet.id = kServerContainerAck;
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
