#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/java/Connections340.hpp>
#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>
#include <basalt/wire/multiversion/java/Link340Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
namespace flat = flattening340;

constexpr std::int32_t kBlockEntity = 0x09;
constexpr std::int32_t kBlockAction = 0x0A;
constexpr std::int32_t kBlockChange = 0x0B;
constexpr std::int32_t kMultiBlockChange = 0x10;
constexpr std::int32_t kUnloadChunk = 0x1D;
constexpr std::int32_t kChunk = 0x20;
constexpr std::int32_t kLevelEvent = 0x21;
constexpr std::int32_t kParticles = 0x22;

constexpr std::int32_t kBlockChange113 = 0x0B;
constexpr std::int32_t kMultiBlockChange113 = 0x0F;
constexpr std::int32_t kBlockAction113 = 0x0A;

constexpr std::int32_t kBed = 26;
constexpr std::int32_t kNoteBlock = 25;
constexpr std::int32_t kFlowerPot = 140;
constexpr std::int32_t kSkull = 144;
constexpr std::int32_t kStandingBanner = 176;
constexpr std::int32_t kWallBanner = 177;
constexpr std::int32_t kRed = 14;
constexpr std::int32_t kWhite = 15;

constexpr std::int32_t kSkullAction = 4;
constexpr std::int32_t kPotAction = 5;
constexpr std::int32_t kBannerAction = 6;
constexpr std::int32_t kBedAction = 11;

constexpr std::int32_t kBlockBreakEvent = 2001;
constexpr std::int32_t kRecordEvent = 1010;

constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr std::size_t kLightBytes = 2048;
constexpr std::size_t kColumns = 256;
constexpr int kMinBits = 4;
constexpr int kMaxIndirectBits = 8;
constexpr int kDirectBits = 14;
constexpr int kSections = 16;

constexpr std::int32_t kReddust = 30;
constexpr std::int32_t kIconCrack = 36;
constexpr std::int32_t kBlockCrack = 37;
constexpr std::int32_t kBlockDust = 38;
constexpr std::int32_t kFallingDust = 46;

[[nodiscard]] bool needsEntity(std::int32_t legacy) {
    const std::int32_t id = legacy >> 4;
    return id == kBed || id == kFlowerPot || id == kSkull || id == kStandingBanner || id == kWallBanner;
}

[[nodiscard]] std::int64_t positionKey(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFF) << 38) | ((static_cast<std::int64_t>(y) & 0xFFF) << 26) |
           (static_cast<std::int64_t>(z) & 0x3FFFFFF);
}

/// What a 1.12 block entity says about its block's look, in the form `stateAt` reads it.
[[nodiscard]] std::int32_t variantOf(std::string_view id, const nbt::NbtCompound& data) {
    if (id == "minecraft:bed") {
        return std::clamp(data.getInt("color", kRed), 0, kWhite);
    }
    if (id == "minecraft:banner") {
        return kWhite - std::clamp(data.getInt("Base", kWhite), 0, kWhite);
    }
    if (id == "minecraft:skull") {
        return std::clamp<std::int32_t>(data.getByte("SkullType"), 0, 5) << 4 | (data.getByte("Rot") & 15);
    }
    if (id == "minecraft:flower_pot") {
        const std::string_view item = data.getString("Item");
        const std::int32_t value = data.getInt("Data");
        for (const flat::Potted& pot : flat::pots()) {
            if (pot.item == item && pot.data == value) {
                return pot.state;
            }
        }
        return -1;
    }
    return -1;
}

[[nodiscard]] std::string_view entityForAction(std::int32_t action) {
    switch (action) {
    case kSkullAction:
        return "minecraft:skull";
    case kPotAction:
        return "minecraft:flower_pot";
    case kBannerAction:
        return "minecraft:banner";
    case kBedAction:
        return "minecraft:bed";
    default:
        return {};
    }
}

struct Section {
    std::vector<std::int32_t> blocks;
    std::span<const std::uint8_t> blockLight;
    std::span<const std::uint8_t> skyLight;
};

[[nodiscard]] bool readSection(io::ByteBuffer& in, bool sky, Section& section) {
    const int bits = in.readByte();
    const std::int32_t entries = pv::varInt::read(in);
    std::vector<std::int32_t> palette;
    for (std::int32_t index = 0; index < entries && !in.failed(); ++index) {
        palette.push_back(pv::varInt::read(in));
    }
    const std::int32_t count = pv::varInt::read(in);
    std::vector<std::int64_t> words;
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        words.push_back(in.readLong());
    }
    section.blockLight = in.readSpan(kLightBytes);
    section.skyLight = sky ? in.readSpan(kLightBytes) : std::span<const std::uint8_t>{};
    if (in.failed() || bits < 1 || bits > 32 || words.size() != compactBitStorage::longsFor(kBlocksPerSection, bits)) {
        return false;
    }
    section.blocks = compactBitStorage::unpack(words, bits, kBlocksPerSection);
    for (std::int32_t& value : section.blocks) {
        if (!palette.empty()) {
            value = static_cast<std::size_t>(value) < palette.size() ? palette[static_cast<std::size_t>(value)] : 0;
        }
        value &= 4095;
    }
    return true;
}

void writeSection(io::ByteBuffer& out, std::span<const std::int32_t> states) {
    const std::int32_t highest = states.empty() ? 0 : *std::ranges::max_element(states);
    std::vector<std::int32_t> slot(static_cast<std::size_t>(highest) + 1, -1);
    std::vector<std::int32_t> palette;
    std::vector<std::int32_t> indexes(states.size());
    for (std::size_t cell = 0; cell < states.size(); ++cell) {
        std::int32_t& index = slot[static_cast<std::size_t>(states[cell])];
        if (index < 0) {
            index = static_cast<std::int32_t>(palette.size());
            palette.push_back(states[cell]);
        }
        indexes[cell] = index;
    }
    int bits = std::max(kMinBits, static_cast<int>(std::bit_width(palette.size() - 1)));
    const bool direct = bits > kMaxIndirectBits;
    out.writeByte(static_cast<std::uint8_t>(direct ? kDirectBits : bits));
    if (direct) {
        bits = kDirectBits;
    } else {
        pv::varInt::write(out, static_cast<std::int32_t>(palette.size()));
        for (const std::int32_t state : palette) {
            pv::varInt::write(out, state);
        }
    }
    const std::vector<std::int64_t> words =
        compactBitStorage::pack(direct ? std::vector<std::int32_t>{states.begin(), states.end()} : indexes, bits);
    pv::varInt::write(out, static_cast<std::int32_t>(words.size()));
    for (const std::int64_t word : words) {
        out.writeLong(word);
    }
}

} // namespace

std::int32_t Link340Up::stateAt(std::int64_t position, std::int32_t legacy) const {
    const std::int32_t base = content340::blockState(legacy);
    const auto found = variantAt_.find(position);
    const std::int32_t variant = found == variantAt_.end() ? -1 : found->second;
    const flat::Variants& variants = flat::variants();
    const auto at = [](const auto& list, std::int32_t index) { return list[static_cast<std::size_t>(index)]; };
    switch (legacy >> 4) {
    case kBed:
        return variant < 0 ? base : base - at(variants.beds, kRed) + at(variants.beds, variant);
    case kStandingBanner:
        return variant < 0 ? base : base - at(variants.banners, 0) + at(variants.banners, variant);
    case kWallBanner:
        return variant < 0 ? base : base - at(variants.wallBanners, 0) + at(variants.wallBanners, variant);
    case kSkull: {
        const std::int32_t type = variant < 0 ? 0 : variant >> 4;
        if ((legacy & 7) <= 1) {
            return at(variants.skulls, type) + (variant < 0 ? 0 : variant & 15);
        }
        return base - at(variants.wallSkulls, 0) + at(variants.wallSkulls, type);
    }
    case kFlowerPot:
        return variant < 0 ? base : variant;
    default:
        return base;
    }
}

void Link340Up::remember(std::int64_t position, std::int32_t legacy) {
    const auto known = legacyAt_.find(position);
    if (known != legacyAt_.end() && known->second >> 4 != legacy >> 4) {
        variantAt_.erase(position);
    }
    if (needsEntity(legacy)) {
        legacyAt_[position] = legacy;
    } else if (known != legacyAt_.end()) {
        legacyAt_.erase(known);
        variantAt_.erase(position);
    }
}

void Link340Up::forgetChunk(std::int32_t x, std::int32_t z) {
    const auto inChunk = [x, z](std::int64_t key) {
        return static_cast<std::int32_t>(key >> 38) >> 4 == x && static_cast<std::int32_t>((key << 38) >> 38) >> 4 == z;
    };
    std::erase_if(legacyAt_, [&](const auto& entry) { return inChunk(entry.first); });
    std::erase_if(variantAt_, [&](const auto& entry) { return inChunk(entry.first); });
}

std::int32_t Link340Up::shownAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t legacy) const {
    if (needsEntity(legacy)) {
        return stateAt(positionKey(x, y, z), legacy);
    }
    const std::int32_t state = content340::blockState(legacy);
    return connections340::joins(state) ? connections340::connect(world_, x, y, z, state) : state;
}

void Link340Up::refreshAround(MutablePacket& packet, std::int32_t x, std::int32_t y, std::int32_t z) const {
    constexpr std::array<std::array<std::int32_t, 3>, 6> kSides = {
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
    for (const auto& [dx, dy, dz] : kSides) {
        const std::int32_t legacy = world_.at(x + dx, y + dy, z + dz);
        if (needsEntity(legacy) || !connections340::joins(content340::blockState(legacy))) {
            continue;
        }
        io::ByteBuffer change;
        change.writeLong(positionKey(x + dx, y + dy, z + dz));
        pv::varInt::write(change, shownAt(x + dx, y + dy, z + dz, legacy));
        packet.forwarded.emplace_back(kBlockChange113, change.toVector());
    }
}

void Link340Up::refreshEdges(MutablePacket& packet, std::int32_t chunkX, std::int32_t chunkZ) const {
    struct Edge {
        std::int32_t chunkX;
        std::int32_t chunkZ;
        std::int32_t x;
        std::int32_t z;
        bool alongX;
    };

    const std::array<Edge, 4> edges = {{{chunkX - 1, chunkZ, 15, 0, false},
                                        {chunkX + 1, chunkZ, 0, 0, false},
                                        {chunkX, chunkZ - 1, 0, 15, true},
                                        {chunkX, chunkZ + 1, 0, 0, true}}};
    for (const Edge& edge : edges) {
        if (!world_.loaded(edge.chunkX, edge.chunkZ)) {
            continue;
        }
        io::ByteBuffer records;
        std::int32_t count = 0;
        for (std::int32_t step = 0; step < 16; ++step) {
            const std::int32_t localX = edge.alongX ? step : edge.x;
            const std::int32_t localZ = edge.alongX ? edge.z : step;
            const std::int32_t x = edge.chunkX * 16 + localX;
            const std::int32_t z = edge.chunkZ * 16 + localZ;
            for (std::int32_t y = 0; y < connections340::LegacyWorld::kSections * 16; ++y) {
                const std::int32_t legacy = world_.at(x, y, z);
                if (legacy == 0 || needsEntity(legacy) ||
                    !connections340::joinsSideways(content340::blockState(legacy))) {
                    continue;
                }
                records.writeByte(static_cast<std::uint8_t>(localX << 4 | localZ));
                records.writeByte(static_cast<std::uint8_t>(y));
                pv::varInt::write(records, shownAt(x, y, z, legacy));
                ++count;
            }
        }
        if (count == 0) {
            continue;
        }
        io::ByteBuffer out;
        out.writeInt(edge.chunkX);
        out.writeInt(edge.chunkZ);
        pv::varInt::write(out, count);
        out.writeBytes(records.readable());
        packet.forwarded.emplace_back(kMultiBlockChange113, out.toVector());
    }
}

bool Link340Up::chunk(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t chunkX = in.readInt();
    const std::int32_t chunkZ = in.readInt();
    const bool full = pv::readBoolean(in);
    const std::int32_t mask = pv::varInt::read(in);
    const std::int32_t length = pv::varInt::read(in);
    if (in.failed() || length < 0) {
        return false;
    }
    io::ByteBuffer data = io::ByteBuffer::wrap(in.readBytes(static_cast<std::size_t>(length)));
    const std::int32_t entities = pv::readCollectionSize(in);
    std::vector<std::span<const std::uint8_t>> kept;
    for (std::int32_t index = 0; index < entities && !in.failed(); ++index) {
        const auto entity = readNamedNbt(in);
        if (!entity) {
            return false;
        }
        const nbt::NbtCompound* tag = entity->tag.get<nbt::NbtCompound>();
        const std::string_view id = tag != nullptr ? tag->getString("id") : std::string_view{};
        if (tag != nullptr) {
            const std::int32_t variant = variantOf(id, *tag);
            if (variant >= 0) {
                variantAt_[positionKey(tag->getInt("x"), tag->getInt("y"), tag->getInt("z"))] = variant;
            }
        }
        if (id != "minecraft:flower_pot") {
            kept.push_back(entity->raw);
        }
    }
    if (in.failed()) {
        return false;
    }

    if (full || !world_.loaded(chunkX, chunkZ)) {
        world_.load(chunkX, chunkZ);
    }
    std::array<Section, kSections> read;
    for (int index = 0; index < kSections; ++index) {
        if ((mask & (1 << index)) == 0) {
            continue;
        }
        Section& section = read[static_cast<std::size_t>(index)];
        if (!readSection(data, skyLight_, section)) {
            return false;
        }
        auto stored = std::make_unique<connections340::LegacyWorld::Section>();
        std::ranges::transform(section.blocks, stored->begin(),
                               [](std::int32_t legacy) { return static_cast<std::uint16_t>(legacy); });
        world_.setSection(chunkX, chunkZ, index, std::move(stored));
    }

    io::ByteBuffer sections;
    std::vector<std::int32_t> states(kBlocksPerSection);
    for (int index = 0; index < kSections; ++index) {
        if ((mask & (1 << index)) == 0) {
            continue;
        }
        const Section& section = read[static_cast<std::size_t>(index)];
        for (std::size_t cell = 0; cell < kBlocksPerSection; ++cell) {
            const std::int32_t legacy = section.blocks[cell];
            const auto x = chunkX * 16 + static_cast<std::int32_t>(cell & 15);
            const auto y = static_cast<std::int32_t>(cell >> 8) + index * 16;
            const auto z = chunkZ * 16 + static_cast<std::int32_t>((cell >> 4) & 15);
            if (needsEntity(legacy)) {
                remember(positionKey(x, y, z), legacy);
            }
            states[cell] = shownAt(x, y, z, legacy);
        }
        writeSection(sections, states);
        sections.writeBytes(section.blockLight);
        sections.writeBytes(section.skyLight);
    }
    if (full) {
        const std::span<const std::uint8_t> biomes = data.readSpan(kColumns);
        if (data.failed()) {
            return false;
        }
        for (const std::uint8_t biome : biomes) {
            sections.writeInt(biome);
        }
    }

    io::ByteBuffer out;
    out.writeInt(chunkX);
    out.writeInt(chunkZ);
    pv::writeBoolean(out, full);
    pv::varInt::write(out, mask);
    pv::varInt::write(out, static_cast<std::int32_t>(sections.readableBytes()));
    out.writeBytes(sections.readable());
    pv::varInt::write(out, static_cast<std::int32_t>(kept.size()));
    for (const std::span<const std::uint8_t> entity : kept) {
        out.writeBytes(entity);
    }
    packet.body = out.toVector();
    refreshEdges(packet, chunkX, chunkZ);
    return true;
}

bool Link340Up::blockChange(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int64_t position = in.readLong();
    const std::int32_t legacy = pv::varInt::read(in) & 4095;
    if (in.failed()) {
        return false;
    }
    remember(position, legacy);
    const auto x = static_cast<std::int32_t>(position >> 38);
    const auto y = static_cast<std::int32_t>((position >> 26) & 0xFFF);
    const auto z = static_cast<std::int32_t>((position << 38) >> 38);
    world_.set(x, y, z, legacy);
    io::ByteBuffer out;
    out.writeLong(position);
    pv::varInt::write(out, shownAt(x, y, z, legacy));
    packet.body = out.toVector();
    refreshAround(packet, x, y, z);
    return true;
}

bool Link340Up::blockChanges(MutablePacket& packet) {
    struct Change {
        std::uint8_t horizontal;
        std::uint8_t y;
        std::int32_t x;
        std::int32_t z;
        std::int32_t legacy;
    };

    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t chunkX = in.readInt();
    const std::int32_t chunkZ = in.readInt();
    const std::int32_t count = pv::readCollectionSize(in, 3);
    std::vector<Change> changes;
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        Change change{};
        change.horizontal = in.readByte();
        change.y = in.readByte();
        change.legacy = pv::varInt::read(in) & 4095;
        change.x = chunkX * 16 + (change.horizontal >> 4);
        change.z = chunkZ * 16 + (change.horizontal & 15);
        changes.push_back(change);
    }
    if (in.failed()) {
        return false;
    }
    for (const Change& change : changes) {
        remember(positionKey(change.x, change.y, change.z), change.legacy);
        world_.set(change.x, change.y, change.z, change.legacy);
    }
    io::ByteBuffer out;
    out.writeInt(chunkX);
    out.writeInt(chunkZ);
    pv::varInt::write(out, count);
    for (const Change& change : changes) {
        out.writeByte(change.horizontal);
        out.writeByte(change.y);
        pv::varInt::write(out, shownAt(change.x, change.y, change.z, change.legacy));
    }
    packet.body = out.toVector();
    for (const Change& change : changes) {
        refreshAround(packet, change.x, change.y, change.z);
    }
    return true;
}

bool Link340Up::blockEntity(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int64_t position = in.readLong();
    const std::uint8_t action = in.readByte();
    const auto data = readNamedNbt(in);
    if (in.failed()) {
        return false;
    }
    const std::string_view kind = entityForAction(action);
    const nbt::NbtCompound* tag = data->tag.get<nbt::NbtCompound>();
    if (!kind.empty() && tag != nullptr) {
        const std::int32_t variant = variantOf(kind, *tag);
        if (variant >= 0) {
            variantAt_[position] = variant;
        }
        const auto known = legacyAt_.find(position);
        if (known != legacyAt_.end()) {
            io::ByteBuffer change;
            change.writeLong(position);
            pv::varInt::write(change, stateAt(position, known->second));
            packet.forwarded.emplace_back(kBlockChange113, change.toVector());
        }
    }
    return action != kPotAction;
}

bool Link340Up::particles(MutablePacket& packet) const {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t legacy = in.readInt();
    const std::uint8_t far = in.readByte();
    std::array<float, 7> values{};
    for (float& value : values) {
        value = in.readFloat();
    }
    const std::int32_t count = in.readInt();
    const std::int32_t particle = content340::particleId(legacy);
    if (in.failed() || particle < 0) {
        return false;
    }
    std::array<std::int32_t, 2> data{};
    const std::size_t params = legacy == kIconCrack                                                        ? 2
                               : (legacy == kBlockCrack || legacy == kBlockDust || legacy == kFallingDust) ? 1
                                                                                                           : 0;
    for (std::size_t index = 0; index < params; ++index) {
        data[index] = pv::varInt::read(in);
    }
    if (in.failed()) {
        return false;
    }
    io::ByteBuffer out;
    out.writeInt(particle);
    out.writeByte(far);
    std::array<float, 4> colour{1.0F, 0.0F, 0.0F, 1.0F};
    if (legacy == kReddust && count == 0) {
        colour = {values[3] == 0.0F ? 1.0F : values[3], values[4], values[5], 1.0F};
        values[3] = values[4] = values[5] = 0.0F;
    }
    for (const float value : values) {
        out.writeFloat(value);
    }
    out.writeInt(count);
    content340::particleData(out, legacy, data, colour);
    packet.body = out.toVector();
    return true;
}

bool Link340Up::world(MutablePacket& packet) {
    switch (packet.id) {
    case kChunk:
        return chunk(packet);
    case kBlockChange:
        return blockChange(packet);
    case kMultiBlockChange:
        return blockChanges(packet);
    case kBlockEntity:
        return blockEntity(packet);
    case kParticles:
        return particles(packet);
    case kUnloadChunk: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const std::int32_t x = in.readInt();
        const std::int32_t z = in.readInt();
        forgetChunk(x, z);
        world_.unload(x, z);
        return true;
    }
    case kBlockAction: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        const std::int64_t position = in.readLong();
        const std::uint8_t first = in.readByte();
        const std::uint8_t second = in.readByte();
        const std::int32_t legacyBlock = pv::varInt::read(in);
        const std::int32_t block = flat::blockIds().map(legacyBlock);
        if (in.failed() || block == IdMap::kNone) {
            return false;
        }
        out.writeLong(position);
        out.writeByte(first);
        out.writeByte(second);
        pv::varInt::write(out, block);
        packet.body = out.toVector();
        const std::int32_t tuned = legacyBlock == kNoteBlock ? connections340::noteBlock(first, second) : -1;
        if (tuned >= 0) {
            // parity: NoteBlock.triggerEvent plays its own state from 1.13, so the state goes first.
            io::ByteBuffer change;
            change.writeLong(position);
            pv::varInt::write(change, tuned);
            packet.forwarded.emplace_back(kBlockAction113, std::move(packet.body));
            packet.body = change.toVector();
            packet.overrideId(kBlockChange113);
        }
        return true;
    }
    case kLevelEvent: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        const std::int32_t event = in.readInt();
        out.writeInt(event);
        out.writeLong(in.readLong());
        std::int32_t data = in.readInt();
        if (event == kBlockBreakEvent) {
            data = content340::blockStateOfStateId(data);
        } else if (event == kRecordEvent && data != 0) {
            data = content340::upItem(data, 0);
        }
        out.writeInt(data);
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        return !in.failed();
    }
    default:
        return true;
    }
}

} // namespace basalt::wire::multiversion::java
