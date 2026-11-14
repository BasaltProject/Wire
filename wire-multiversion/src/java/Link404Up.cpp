#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/java/Link404Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/multiversion/java/SlotCopy.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kSpawnEntity = 0x00;
constexpr std::int32_t kSpawnPainting = 0x04;
constexpr std::int32_t kBlockBreakAnimation = 0x08;
constexpr std::int32_t kTileEntityData = 0x09;
constexpr std::int32_t kBlockAction = 0x0A;
constexpr std::int32_t kBlockChange = 0x0B;
constexpr std::int32_t kDifficulty = 0x0D;
constexpr std::int32_t kOpenWindow = 0x14;
constexpr std::int32_t kCustomPayload = 0x19;
constexpr std::int32_t kChunk = 0x22;
constexpr std::int32_t kWorldEvent = 0x23;
constexpr std::int32_t kLogin = 0x25;
constexpr std::int32_t kMap = 0x26;
constexpr std::int32_t kOpenSignEntity = 0x2C;
constexpr std::int32_t kPosition = 0x32;
constexpr std::int32_t kBed = 0x33;
constexpr std::int32_t kRespawn = 0x38;
constexpr std::int32_t kSpawnPosition = 0x49;
constexpr std::int32_t kDeclareRecipes = 0x54;
constexpr std::int32_t kTags = 0x55;

constexpr std::int32_t kChunk1_14 = 0x21;
constexpr std::int32_t kUpdateLight = 0x24;
constexpr std::int32_t kOpenHorseWindow = 0x1F;
constexpr std::int32_t kTradeList = 0x27;
constexpr std::int32_t kOpenBook = 0x2D;
constexpr std::int32_t kUpdateViewPosition = 0x40;

constexpr std::int32_t kBlockPlace = 0x29;
constexpr std::int32_t kBlockDig = 0x18;
constexpr std::int32_t kUpdateSign = 0x26;
constexpr std::int32_t kUpdateCommandBlock = 0x22;
constexpr std::int32_t kQueryBlockNbt = 0x01;
constexpr std::int32_t kUpdateStructureBlock = 0x25;
constexpr std::int32_t kCraftingBookData = 0x1B;
constexpr std::int32_t kMovePosition = 0x10;
constexpr std::int32_t kMovePositionLook = 0x11;

constexpr std::int32_t kSections = 16;
constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr int kMaxIndirectBits = 8;
constexpr int kHeightmapBits = 9;
constexpr std::size_t kColumns = 256;
constexpr std::size_t kLightBytes = 2048;
constexpr std::int32_t kViewDistance = 32;
constexpr std::int32_t kOverworld = 0;
constexpr std::int32_t kMenuGeneric3x3 = 6;

/// parity: block states a 1.13 chunk calls air: air, void air and cave air.
[[nodiscard]] bool isAir(std::int32_t state) {
    return state <= 0 || state == 8574 || state == 8575 || state == 8591 || state == 8592;
}

[[nodiscard]] std::int64_t toNewPosition(std::int64_t packed) {
    const std::int64_t x = packed >> 38;
    const std::int64_t y = (packed >> 26) & 0xFFF;
    const std::int64_t z = (packed << 38) >> 38;
    return ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF);
}

[[nodiscard]] std::int64_t toOldPosition(std::int64_t packed) {
    const std::int64_t x = packed >> 38;
    const std::int64_t y = packed & 0xFFF;
    const std::int64_t z = (packed << 26) >> 38;
    return ((x & 0x3FFFFFF) << 38) | ((y & 0xFFF) << 26) | (z & 0x3FFFFFF);
}

/// The object ids of 1.13 spawn packets, as the registry numbers the entity table starts from.
[[nodiscard]] std::int32_t registryIdOfObject(std::int32_t object, std::int32_t data) {
    switch (object) {
    case 1:
        return 5;
    case 2:
        return 32;
    case 3:
        return 0;
    case 10: {
        constexpr std::array<std::int32_t, 7> kMinecarts = {39, 40, 42, 45, 44, 43, 41};
        return kMinecarts[static_cast<std::size_t>(std::clamp(data, 0, 6))];
    }
    case 50:
        return 55;
    case 51:
        return 16;
    case 60:
        return 2;
    case 61:
        return 67;
    case 62:
        return 74;
    case 63:
        return 34;
    case 64:
        return 65;
    case 65:
        return 75;
    case 66:
        return 85;
    case 67:
        return 60;
    case 68:
        return 37;
    case 70:
        return 24;
    case 71:
        return 33;
    case 72:
        return 23;
    case 73:
        return 77;
    case 75:
        return 76;
    case 76:
        return 25;
    case 77:
        return 35;
    case 78:
        return 1;
    case 79:
        return 20;
    case 90:
        return 93;
    case 91:
        return 68;
    case 93:
        return 13;
    case 94:
        return 94;
    default:
        return object;
    }
}

[[nodiscard]] std::int32_t menuFor(std::string_view type, std::int32_t slots) {
    if (type == "minecraft:chest" || type == "minecraft:container") {
        return std::clamp(slots / 9, 1, 6) - 1;
    }
    if (type == "minecraft:dispenser" || type == "minecraft:dropper") {
        return kMenuGeneric3x3;
    }
    if (type == "minecraft:anvil")
        return 7;
    if (type == "minecraft:beacon")
        return 8;
    if (type == "minecraft:brewing_stand")
        return 10;
    if (type == "minecraft:crafting_table")
        return 11;
    if (type == "minecraft:enchanting_table")
        return 12;
    if (type == "minecraft:furnace")
        return 13;
    if (type == "minecraft:hopper")
        return 15;
    if (type == "minecraft:villager")
        return 18;
    if (type == "minecraft:shulker_box")
        return 19;
    return -1;
}

void copyContainer(io::ByteBuffer& in, io::ByteBuffer& out, std::vector<std::int32_t>& blocks, bool& air) {
    const int bits = in.readByte();
    out.writeByte(static_cast<std::uint8_t>(bits));
    std::vector<std::int32_t> palette;
    if (bits <= kMaxIndirectBits) {
        const std::int32_t entries = pv::varInt::read(in);
        pv::varInt::write(out, entries);
        for (std::int32_t index = 0; index < entries && !in.failed(); ++index) {
            palette.push_back(pv::varInt::read(in));
            pv::varInt::write(out, palette.back());
        }
    }
    const std::int32_t count = pv::varInt::read(in);
    pv::varInt::write(out, count);
    std::vector<std::int64_t> words;
    words.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), in.readableBytes()));
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        words.push_back(in.readLong());
        out.writeLong(words.back());
    }
    air = true;
    if (in.failed() || bits < 1 || bits > 32 || words.size() != compactBitStorage::longsFor(kBlocksPerSection, bits)) {
        return;
    }
    blocks = compactBitStorage::unpack(words, bits, kBlocksPerSection);
    if (!palette.empty()) {
        for (std::int32_t& value : blocks) {
            value = static_cast<std::size_t>(value) < palette.size() ? palette[static_cast<std::size_t>(value)] : 0;
        }
    }
    air = false;
}

} // namespace

void Link404Up::centreOn(double x, double z) {
    wantedX_ = static_cast<std::int32_t>(std::floor(x)) >> 4;
    wantedZ_ = static_cast<std::int32_t>(std::floor(z)) >> 4;
    centred_ = true;
}

void Link404Up::announceCentre(MutablePacket& packet) {
    if (!centred_ || (sent_ && sentX_ == wantedX_ && sentZ_ == wantedZ_)) {
        return;
    }
    io::ByteBuffer out;
    pv::varInt::write(out, wantedX_);
    pv::varInt::write(out, wantedZ_);
    packet.forwarded.emplace_back(kUpdateViewPosition, out.toVector());
    sentX_ = wantedX_;
    sentZ_ = wantedZ_;
    sent_ = true;
}

bool Link404Up::chunk(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = in.readInt();
    const std::int32_t z = in.readInt();
    const std::uint8_t full = in.readByte();
    const std::int32_t mask = pv::varInt::read(in);
    const std::int32_t length = pv::varInt::read(in);
    if (in.failed() || length < 0) {
        return false;
    }
    io::ByteBuffer sections = io::ByteBuffer::wrap(in.readBytes(static_cast<std::size_t>(length)));
    const std::span<const std::uint8_t> rest = in.readable();
    if (in.failed()) {
        return false;
    }

    io::ByteBuffer data;
    std::array<std::vector<std::uint8_t>, kSections> sky;
    std::array<std::vector<std::uint8_t>, kSections> block;
    std::vector<std::int32_t> tops(kColumns, 0);
    std::int32_t highest = -1;
    for (std::int32_t index = 0; index < kSections; ++index) {
        if ((mask & (1 << index)) == 0) {
            continue;
        }
        io::ByteBuffer container;
        std::vector<std::int32_t> blocks;
        bool air = true;
        copyContainer(sections, container, blocks, air);
        block[static_cast<std::size_t>(index)] = sections.readBytes(kLightBytes);
        if (hasSkyLight_) {
            sky[static_cast<std::size_t>(index)] = sections.readBytes(kLightBytes);
        }
        if (sections.failed()) {
            return false;
        }
        std::int32_t solid = 0;
        for (std::size_t cell = 0; cell < blocks.size(); ++cell) {
            if (isAir(blocks[cell])) {
                continue;
            }
            solid++;
            const std::size_t column = (cell & 0xFF) / 16 * 16 + (cell & 0xF);
            tops[column] = std::max(tops[column], index * 16 + static_cast<std::int32_t>(cell >> 8) + 1);
        }
        highest = index;
        data.writeShort(static_cast<std::int16_t>(solid));
        data.writeBytes(container.readable());
    }
    data.writeBytes(sections.readable());

    nbt::NbtCompound maps;
    std::vector<std::int64_t> packed = compactBitStorage::pack(tops, kHeightmapBits);
    maps.putLongArray("MOTION_BLOCKING", packed);
    maps.putLongArray("WORLD_SURFACE", std::move(packed));

    io::ByteBuffer out;
    out.writeInt(x);
    out.writeInt(z);
    out.writeByte(full);
    pv::varInt::write(out, mask);
    writeNamedNbt(out, nbt::NbtTag::ofCompound(std::move(maps)));
    pv::varInt::write(out, static_cast<std::int32_t>(data.readableBytes()));
    out.writeBytes(data.readable());
    out.writeBytes(rest);
    packet.body = out.toVector();

    std::int32_t skyMask = 0;
    std::int32_t blockMask = 0;
    io::ByteBuffer skyArrays;
    io::ByteBuffer blockArrays;
    const std::vector<std::uint8_t> bright(kLightBytes, 0xFF);
    for (std::int32_t index = 0; index < kSections + 1; ++index) {
        const bool present = index < kSections && (mask & (1 << index)) != 0;
        const std::int32_t bit = index + 1;
        if (present) {
            blockMask |= 1 << bit;
            pv::varInt::write(blockArrays, static_cast<std::int32_t>(kLightBytes));
            blockArrays.writeBytes(block[static_cast<std::size_t>(index)]);
        }
        if (!hasSkyLight_) {
            continue;
        }
        if (present) {
            skyMask |= 1 << bit;
            pv::varInt::write(skyArrays, static_cast<std::int32_t>(kLightBytes));
            skyArrays.writeBytes(sky[static_cast<std::size_t>(index)]);
        } else if (full != 0 && index > highest) {
            skyMask |= 1 << bit;
            pv::varInt::write(skyArrays, static_cast<std::int32_t>(kLightBytes));
            skyArrays.writeBytes(bright);
        }
    }
    io::ByteBuffer light;
    pv::varInt::write(light, x);
    pv::varInt::write(light, z);
    pv::varInt::write(light, skyMask);
    pv::varInt::write(light, blockMask);
    pv::varInt::write(light, 0);
    pv::varInt::write(light, 0);
    light.writeBytes(skyArrays.readable());
    light.writeBytes(blockArrays.readable());
    packet.forwarded.emplace_back(kUpdateLight, light.toVector());
    if (!centred_) {
        wantedX_ = x;
        wantedZ_ = z;
        centred_ = true;
    }
    if (!sent_ || sentX_ != wantedX_ || sentZ_ != wantedZ_) {
        io::ByteBuffer view;
        pv::varInt::write(view, wantedX_);
        pv::varInt::write(view, wantedZ_);
        packet.forwarded.insert(packet.forwarded.begin(), {kChunk1_14, std::move(packet.body)});
        packet.body = view.toVector();
        packet.overrideId(kUpdateViewPosition);
        sentX_ = wantedX_;
        sentZ_ = wantedZ_;
        sent_ = true;
    }
    return true;
}

bool Link404Up::openWindow(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::uint8_t window = in.readByte();
    const std::string type = pv::readString(in);
    const std::string title = pv::readString(in);
    const std::uint8_t slots = in.readByte();
    if (type == "EntityHorse") {
        const std::int32_t entity = in.readInt();
        io::ByteBuffer horse;
        horse.writeByte(window);
        pv::varInt::write(horse, slots);
        horse.writeInt(entity);
        packet.forwarded.emplace_back(kOpenHorseWindow, horse.toVector());
        return false;
    }
    if (in.failed()) {
        return false;
    }
    const std::int32_t menu = menuFor(type, slots);
    if (menu < 0) {
        return false;
    }
    io::ByteBuffer out;
    pv::varInt::write(out, window);
    pv::varInt::write(out, menu);
    pv::writeString(out, title);
    packet.body = out.toVector();
    return true;
}

bool Link404Up::customPayload(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::string channel = pv::readString(in);
    if (channel == "minecraft:book_open") {
        io::ByteBuffer out;
        pv::varInt::write(out, pv::varInt::read(in));
        packet.forwarded.emplace_back(kOpenBook, out.toVector());
        return false;
    }
    if (channel != "minecraft:trader_list") {
        return true;
    }
    io::ByteBuffer out;
    pv::varInt::write(out, in.readInt());
    const std::uint8_t trades = in.readByte();
    out.writeByte(trades);
    for (std::uint8_t trade = 0; trade < trades && !in.failed(); ++trade) {
        copyFlatStack(in, out);
        copyFlatStack(in, out);
        const std::uint8_t second = in.readByte();
        out.writeByte(second);
        if (second != 0) {
            copyFlatStack(in, out);
        }
        out.writeByte(in.readByte());
        out.writeInt(in.readInt());
        out.writeInt(in.readInt());
        out.writeInt(0);
        out.writeInt(0);
        out.writeFloat(0.0F);
    }
    pv::varInt::write(out, 0);
    pv::varInt::write(out, 0);
    pv::writeBoolean(out, false);
    if (in.failed()) {
        return false;
    }
    packet.forwarded.emplace_back(kTradeList, out.toVector());
    return false;
}

bool Link404Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    bool keep = true;
    switch (packet.id) {
    case kChunk:
        keep = chunk(packet);
        break;
    case kOpenWindow:
        keep = openWindow(packet);
        break;
    case kCustomPayload:
        keep = customPayload(packet);
        break;
    case kBed:
        keep = false;
        break;
    case kDeclareRecipes:
        packet.body = {0};
        break;
    case kTags:
        out.writeBytes(in.readable());
        pv::varInt::write(out, 0);
        packet.body = out.toVector();
        break;
    case kMap:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeByte(in.readByte());
        out.writeByte(in.readByte());
        pv::writeBoolean(out, false);
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        break;
    case kDifficulty:
        out.writeByte(in.readByte());
        pv::writeBoolean(out, false);
        packet.body = out.toVector();
        break;
    case kLogin: {
        out.writeInt(in.readInt());
        out.writeByte(in.readByte());
        const std::int32_t dimension = in.readInt();
        hasSkyLight_ = dimension == kOverworld;
        out.writeInt(dimension);
        static_cast<void>(in.readByte());
        out.writeByte(in.readByte());
        pv::writeString(out, pv::readString(in));
        pv::varInt::write(out, kViewDistance);
        out.writeByte(in.readByte());
        if (in.failed()) {
            return false;
        }
        packet.body = out.toVector();
        break;
    }
    case kRespawn: {
        const std::int32_t dimension = in.readInt();
        hasSkyLight_ = dimension == kOverworld;
        out.writeInt(dimension);
        static_cast<void>(in.readByte());
        out.writeByte(in.readByte());
        pv::writeString(out, pv::readString(in));
        if (in.failed()) {
            return false;
        }
        packet.body = out.toVector();
        break;
    }
    case kSpawnEntity: {
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeBytes(in.readSpan(16));
        const std::int32_t object = static_cast<std::int8_t>(in.readByte());
        const std::span<const std::uint8_t> position = in.readSpan(24 + 2);
        const std::int32_t data = in.readInt();
        if (in.failed()) {
            return false;
        }
        pv::varInt::write(out, registryIdOfObject(object, data));
        out.writeBytes(position);
        out.writeInt(data);
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        break;
    }
    case kSpawnPainting:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeBytes(in.readSpan(16));
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeLong(toNewPosition(in.readLong()));
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        break;
    case kBlockBreakAnimation:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeLong(toNewPosition(in.readLong()));
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        break;
    case kWorldEvent:
        out.writeInt(in.readInt());
        out.writeLong(toNewPosition(in.readLong()));
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        break;
    case kTileEntityData:
    case kBlockAction:
    case kBlockChange:
    case kOpenSignEntity:
    case kSpawnPosition:
        out.writeLong(toNewPosition(in.readLong()));
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        break;
    case kPosition: {
        const double x = in.readDouble();
        static_cast<void>(in.readDouble());
        const double z = in.readDouble();
        if (!in.failed()) {
            centreOn(x, z);
        }
        break;
    }
    default:
        break;
    }
    if (in.failed()) {
        return false;
    }
    announceCentre(packet);
    return keep;
}

bool Link404Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kBlockPlace: {
        const std::int32_t hand = pv::varInt::read(in);
        out.writeLong(toOldPosition(in.readLong()));
        pv::varInt::write(out, pv::varInt::read(in));
        pv::varInt::write(out, hand);
        out.writeBytes(in.readSpan(12));
        break;
    }
    case kBlockDig:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeLong(toOldPosition(in.readLong()));
        out.writeBytes(in.readable());
        break;
    case kQueryBlockNbt:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeLong(toOldPosition(in.readLong()));
        break;
    case kUpdateSign:
    case kUpdateCommandBlock:
    case kUpdateStructureBlock:
        out.writeLong(toOldPosition(in.readLong()));
        out.writeBytes(in.readable());
        break;
    case kCraftingBookData: {
        const std::int32_t type = pv::varInt::read(in);
        pv::varInt::write(out, type);
        out.writeBytes(in.readSpan(type == 1 ? 4 : in.readableBytes()));
        break;
    }
    case kMovePosition:
    case kMovePositionLook: {
        const double x = in.readDouble();
        static_cast<void>(in.readDouble());
        const double z = in.readDouble();
        if (!in.failed()) {
            centreOn(x, z);
        }
        return true;
    }
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
