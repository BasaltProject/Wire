#include <basalt/wire/multiversion/java/Link756Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kForgetChunk = 29;
constexpr std::int32_t kChunk = 34;
constexpr std::int32_t kLight = 37;
constexpr std::int32_t kLogin = 38;
constexpr std::int32_t kBlockEntity = 10;
constexpr std::int32_t kRespawn = 61;
constexpr std::int32_t kClientInformation = 5;

constexpr std::size_t kBiomesPerSection = 64;
constexpr int kBiomeMaxIndirectBits = 3;
constexpr std::int32_t kSpawnerType = 8;

/// parity: `BlockEntityType` registry order, shared by 1.17.1 and 1.18.
constexpr std::array<std::string_view, 34> kBlockEntityNames = {
    "furnace",     "chest",         "trapped_chest", "ender_chest",
    "jukebox",     "dispenser",     "dropper",       "sign",
    "mob_spawner", "piston",        "brewing_stand", "enchanting_table",
    "end_portal",  "beacon",        "skull",         "daylight_detector",
    "hopper",      "comparator",    "banner",        "structure_block",
    "end_gateway", "command_block", "shulker_box",   "bed",
    "conduit",     "barrel",        "smoker",        "blast_furnace",
    "lectern",     "bell",          "jigsaw",        "campfire",
    "beehive",     "sculk_sensor"};

/// parity: 1.17.1 block entity update actions, which named no registry.
constexpr std::array<std::int32_t, 14> kUpdateActionTypes = {-1, 8, 21, 13, 14, 24, 18, 19, 20, 7, 22, 23, 30, 31};

int ceilLog2(std::size_t value) { return value <= 1 ? 0 : static_cast<int>(std::bit_width(value - 1)); }

void wrapSpawnData(nbt::NbtTag& tag) {
    nbt::NbtCompound* body = tag.get<nbt::NbtCompound>();
    nbt::NbtTag* data = body != nullptr ? body->find("SpawnData") : nullptr;
    if (data == nullptr || data->get<nbt::NbtCompound>() == nullptr) {
        return;
    }
    nbt::NbtCompound wrapper;
    wrapper.set("entity", *data);
    body->set("SpawnData", nbt::NbtTag::ofCompound(std::move(wrapper)));
}

void writeBitSetOfRange(io::ByteBuffer& out, std::int32_t bits) {
    std::vector<std::int64_t> words((static_cast<std::size_t>(bits) + 63) / 64, 0);
    for (std::int32_t bit = 0; bit < bits; ++bit) {
        words[static_cast<std::size_t>(bit) / 64] |=
            static_cast<std::int64_t>(std::uint64_t{1} << (static_cast<std::size_t>(bit) % 64));
    }
    pv::varInt::write(out, static_cast<std::int32_t>(words.size()));
    for (const std::int64_t word : words) {
        out.writeLong(word);
    }
}

void writeBiomeContainer(io::ByteBuffer& out, std::span<const std::int32_t> values, int directBits) {
    std::vector<std::int32_t> palette;
    for (const std::int32_t value : values) {
        if (std::ranges::find(palette, value) == palette.end()) {
            palette.push_back(value);
        }
    }
    if (palette.size() == 1) {
        out.writeByte(0);
        pv::varInt::write(out, palette[0]);
        pv::varInt::write(out, 0);
        return;
    }
    const int indirectBits = std::max(1, ceilLog2(palette.size()));
    const bool indirect = indirectBits <= kBiomeMaxIndirectBits;
    const int bits = indirect ? indirectBits : directBits;
    out.writeByte(static_cast<std::uint8_t>(bits));
    if (indirect) {
        pv::varInt::write(out, static_cast<std::int32_t>(palette.size()));
        for (const std::int32_t entry : palette) {
            pv::varInt::write(out, entry);
        }
    }
    const std::size_t perLong = 64 / static_cast<std::size_t>(bits);
    const std::size_t longs = (values.size() + perLong - 1) / perLong;
    pv::varInt::write(out, static_cast<std::int32_t>(longs));
    for (std::size_t word = 0; word < longs; ++word) {
        std::uint64_t packed = 0;
        for (std::size_t slot = 0; slot < perLong && word * perLong + slot < values.size(); ++slot) {
            const std::int32_t value = values[word * perLong + slot];
            const std::int32_t stored =
                indirect ? static_cast<std::int32_t>(std::ranges::find(palette, value) - palette.begin()) : value;
            packed |= static_cast<std::uint64_t>(static_cast<std::uint32_t>(stored))
                      << (slot * static_cast<std::size_t>(bits));
        }
        out.writeLong(static_cast<std::int64_t>(packed));
    }
}

void copyBlockContainer(io::ByteBuffer& in, io::ByteBuffer& out) {
    const int bits = in.readByte();
    out.writeByte(static_cast<std::uint8_t>(bits));
    if (bits <= 8) {
        const std::int32_t entries = pv::readCollectionSize(in, 1);
        pv::varInt::write(out, entries);
        for (std::int32_t index = 0; index < entries && !in.failed(); ++index) {
            pv::varInt::write(out, pv::varInt::read(in));
        }
    }
    const std::int32_t longs = pv::readCollectionSize(in, 8);
    pv::varInt::write(out, longs);
    out.writeBytes(in.readSpan(static_cast<std::size_t>(longs) * 8));
}

} // namespace

void Link756Up::readWorld(const nbt::NbtTag& dimension) {
    const nbt::NbtCompound* body = dimension.get<nbt::NbtCompound>();
    const std::int32_t height = body != nullptr ? body->getInt("height", 256) : 256;
    sections_ = std::clamp(height / 16, 1, 128);
}

void Link756Up::login(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    out.writeBytes(in.readSpan(7));
    const std::int32_t worlds = pv::readCollectionSize(in, 1);
    pv::varInt::write(out, worlds);
    for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
        pv::writeString(out, pv::readString(in));
    }
    const auto codec = readNamedNbt(in);
    const auto dimension = readNamedNbt(in);
    const std::string world = pv::readString(in);
    const std::int64_t seed = in.readLong();
    const std::int32_t maxPlayers = pv::varInt::read(in);
    const std::int32_t viewDistance = pv::varInt::read(in);
    if (in.failed()) {
        return;
    }
    const nbt::NbtCompound* root = codec->tag.get<nbt::NbtCompound>();
    const nbt::NbtCompound* biomes = root != nullptr ? root->getCompound("minecraft:worldgen/biome") : nullptr;
    const nbt::NbtList* values = biomes != nullptr ? biomes->getList("value") : nullptr;
    biomeBits_ = std::max(1, ceilLog2(values != nullptr ? values->size() : 64));
    readWorld(dimension->tag);
    loaded_.clear();
    pendingLight_.clear();
    out.writeBytes(codec->raw);
    out.writeBytes(dimension->raw);
    pv::writeString(out, world);
    out.writeLong(seed);
    pv::varInt::write(out, maxPlayers);
    pv::varInt::write(out, viewDistance);
    pv::varInt::write(out, viewDistance);
    out.writeBytes(in.readable());
    packet.body = out.toVector();
}

void Link756Up::respawn(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const auto dimension = readNamedNbt(in);
    if (!in.failed()) {
        readWorld(dimension->tag);
        loaded_.clear();
        pendingLight_.clear();
    }
}

bool Link756Up::light(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = pv::varInt::read(in);
    const std::int32_t z = pv::varInt::read(in);
    if (in.failed() || loaded_.contains({x, z})) {
        return true;
    }
    const std::span<const std::uint8_t> rest = in.readable();
    pendingLight_[{x, z}].assign(rest.begin(), rest.end());
    return false;
}

bool Link756Up::chunk(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = in.readInt();
    const std::int32_t z = in.readInt();
    std::vector<std::uint64_t> mask(static_cast<std::size_t>(pv::readCollectionSize(in, 8)));
    for (std::uint64_t& word : mask) {
        word = static_cast<std::uint64_t>(in.readLong());
    }
    const auto heightmaps = readNamedNbt(in);
    std::vector<std::int32_t> biomes(static_cast<std::size_t>(pv::readCollectionSize(in, 1)));
    for (std::int32_t& biome : biomes) {
        biome = pv::varInt::read(in);
    }
    const std::int32_t dataLength = pv::readCollectionSize(in, 1);
    io::ByteBuffer data = io::ByteBuffer::wrap(in.readBytes(static_cast<std::size_t>(dataLength)));
    const std::int32_t entities = pv::readCollectionSize(in, 1);
    io::ByteBuffer entityOut;
    std::int32_t kept = 0;
    for (std::int32_t index = 0; index < entities && !in.failed(); ++index) {
        auto entity = readNamedNbt(in);
        if (in.failed()) {
            break;
        }
        const nbt::NbtCompound* body = entity->tag.get<nbt::NbtCompound>();
        if (body == nullptr || !body->contains("x") || !body->contains("y") || !body->contains("z")) {
            continue;
        }
        std::string_view name = body->getString("id");
        name.remove_prefix(name.starts_with("minecraft:") ? 10 : 0);
        const auto found = std::ranges::find(kBlockEntityNames, name);
        if (found == kBlockEntityNames.end()) {
            continue;
        }
        const auto type = static_cast<std::int32_t>(found - kBlockEntityNames.begin());
        if (type == kSpawnerType) {
            wrapSpawnData(entity->tag);
        }
        entityOut.writeByte(static_cast<std::uint8_t>(((body->getInt("x") & 15) << 4) | (body->getInt("z") & 15)));
        entityOut.writeShort(static_cast<std::int16_t>(body->getInt("y")));
        pv::varInt::write(entityOut, type);
        writeNamedNbt(entityOut, entity->tag);
        kept++;
    }
    if (in.failed()) {
        return false;
    }
    io::ByteBuffer sectionsOut;
    for (std::int32_t section = 0; section < sections_; ++section) {
        const auto word = static_cast<std::size_t>(section) / 64;
        const bool present = word < mask.size() && ((mask[word] >> (static_cast<std::size_t>(section) % 64)) & 1) != 0;
        if (present) {
            sectionsOut.writeShort(data.readShort());
            copyBlockContainer(data, sectionsOut);
        } else {
            sectionsOut.writeShort(0);
            sectionsOut.writeByte(0);
            pv::varInt::write(sectionsOut, 0);
            pv::varInt::write(sectionsOut, 0);
        }
        std::array<std::int32_t, kBiomesPerSection> cells{};
        for (std::size_t cell = 0; cell < kBiomesPerSection; ++cell) {
            const std::size_t at = static_cast<std::size_t>(section) * kBiomesPerSection + cell;
            cells[cell] = at < biomes.size() ? std::max(biomes[at], 0) : 0;
        }
        writeBiomeContainer(sectionsOut, cells, biomeBits_);
    }
    if (data.failed()) {
        return false;
    }
    io::ByteBuffer out;
    out.writeInt(x);
    out.writeInt(z);
    out.writeBytes(heightmaps->raw);
    pv::varInt::write(out, static_cast<std::int32_t>(sectionsOut.readableBytes()));
    out.writeBytes(sectionsOut.readable());
    pv::varInt::write(out, kept);
    out.writeBytes(entityOut.readable());
    const auto cached = pendingLight_.find({x, z});
    if (cached != pendingLight_.end()) {
        out.writeBytes(cached->second);
        pendingLight_.erase(cached);
    } else {
        out.writeByte(0);
        pv::varInt::write(out, 0);
        pv::varInt::write(out, 0);
        writeBitSetOfRange(out, sections_ + 2);
        writeBitSetOfRange(out, sections_ + 2);
        pv::varInt::write(out, 0);
        pv::varInt::write(out, 0);
    }
    loaded_.insert({x, z});
    packet.body = out.toVector();
    return true;
}

void Link756Up::blockEntity(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int64_t position = in.readLong();
    const std::uint8_t action = in.readByte();
    auto tag = readNamedNbt(in);
    if (in.failed()) {
        return;
    }
    const std::int32_t type = action < kUpdateActionTypes.size() ? kUpdateActionTypes[action] : -1;
    if (type == kSpawnerType) {
        wrapSpawnData(tag->tag);
    }
    io::ByteBuffer out;
    out.writeLong(position);
    pv::varInt::write(out, type);
    writeNamedNbt(out, tag->tag);
    packet.body = out.toVector();
}

bool Link756Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kLogin:
        login(packet);
        return true;
    case kRespawn:
        respawn(packet);
        return true;
    case kLight:
        return light(packet);
    case kChunk:
        return chunk(packet);
    case kBlockEntity:
        blockEntity(packet);
        return true;
    case kForgetChunk: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const std::int32_t x = in.readInt();
        const std::int32_t z = in.readInt();
        loaded_.erase({x, z});
        pendingLight_.erase({x, z});
        return true;
    }
    default:
        return true;
    }
}

bool Link756Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Play && packet.id == kClientInformation && !packet.body.empty()) {
        packet.body.pop_back();
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
