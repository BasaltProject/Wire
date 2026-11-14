#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataRewriter.hpp>
#include <basalt/wire/multiversion/java/OlderPaintings.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kPainting = 94;
constexpr std::string_view kNamespace = "minecraft:";

struct Motive {
    std::string_view name;
    std::string_view title;
    std::int32_t width;
    std::int32_t height;
};

/// parity: `Motive` from 1.13 to 1.18, numbered in this order, and the title each had before.
constexpr std::array<Motive, 26> kMotives = {{
    {"kebab", "Kebab", 1, 1},
    {"aztec", "Aztec", 1, 1},
    {"alban", "Alban", 1, 1},
    {"aztec2", "Aztec2", 1, 1},
    {"bomb", "Bomb", 1, 1},
    {"plant", "Plant", 1, 1},
    {"wasteland", "Wasteland", 1, 1},
    {"pool", "Pool", 2, 1},
    {"courbet", "Courbet", 2, 1},
    {"sea", "Sea", 2, 1},
    {"sunset", "Sunset", 2, 1},
    {"creebet", "Creebet", 2, 1},
    {"wanderer", "Wanderer", 1, 2},
    {"graham", "Graham", 1, 2},
    {"match", "Match", 2, 2},
    {"bust", "Bust", 2, 2},
    {"stage", "Stage", 2, 2},
    {"void", "Void", 2, 2},
    {"skull_and_roses", "SkullAndRoses", 2, 2},
    {"wither", "Wither", 2, 2},
    {"fighters", "Fighters", 4, 2},
    {"pointer", "Pointer", 4, 4},
    {"pigscene", "Pigscene", 4, 4},
    {"burning_skull", "BurningSkull", 4, 4},
    {"skeleton", "Skeleton", 4, 3},
    {"donkey_kong", "DonkeyKong", 4, 3},
}};

/// The motive of that name, or else the first of the same size, so the old client frames it alike.
[[nodiscard]] std::int32_t motiveFor(std::string_view key, const std::optional<nbt::NbtTag>& value) {
    if (key.starts_with(kNamespace)) {
        key.remove_prefix(kNamespace.size());
    }
    const auto named = std::ranges::find(kMotives, key, &Motive::name);
    if (named != kMotives.end()) {
        return static_cast<std::int32_t>(named - kMotives.begin());
    }
    const nbt::NbtCompound* element = value.has_value() ? value->get<nbt::NbtCompound>() : nullptr;
    if (element == nullptr) {
        return 0;
    }
    const std::int32_t width = element->getInt("width");
    const std::int32_t height = element->getInt("height");
    const auto sized = std::ranges::find_if(
        kMotives, [&](const Motive& motive) { return motive.width == width && motive.height == height; });
    return sized == kMotives.end() ? 0 : static_cast<std::int32_t>(sized - kMotives.begin());
}

} // namespace

void OlderPaintings::learn(const std::vector<std::uint8_t>& registryData) {
    if (farEnd_ >= protocolVersions::kV1_19) {
        return;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(registryData);
    const auto data = protocol::configuration::RegistryDataPacket::read(input);
    if (!data.has_value() || data->registryId != "minecraft:painting_variant") {
        return;
    }
    motives_.clear();
    for (const protocol::configuration::RegistryEntry& entry : data->entries) {
        motives_.push_back(motiveFor(entry.key, entry.value));
    }
}

void OlderPaintings::hold(const MutablePacket& spawn) {
    if (farEnd_ >= protocolVersions::kV1_19) {
        return;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(spawn.body);
    const auto read = play::SpawnEntityPacket::read(input);
    if (!read.has_value() || read->entityType != kPainting) {
        return;
    }
    held_ = spawn.body;
    heldId_ = read->entityId;
}

std::optional<std::int32_t> OlderPaintings::motiveIn(const MutablePacket& packet) const {
    if (!held_.has_value() || packet.nativeId() != play::SetEntityDataPacket::kId) {
        return std::nullopt;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const auto data = EntityDataRewriter::read(input);
    if (!data.has_value() || data->entityId != heldId_) {
        return std::nullopt;
    }
    for (const play::SetEntityDataPacket::Entry& entry : data->entries) {
        const auto* variant = std::get_if<play::SetEntityDataPacket::RegistryIdValue>(&entry.value);
        if (variant == nullptr || variant->registry != play::RegistryHolder::PaintingVariant) {
            continue;
        }
        // The holder counts registry entries from one; zero would be a variant written out inline.
        const std::int32_t id = variant->value - 1;
        return id >= 0 && static_cast<std::size_t>(id) < motives_.size() ? motives_[static_cast<std::size_t>(id)]
                                                                         : firstMotive();
    }
    return firstMotive();
}

void OlderPaintings::spawn(MutablePacket& packet, std::int32_t motive) {
    io::ByteBuffer input = io::ByteBuffer::wrap(*held_);
    auto read = play::SpawnEntityPacket::read(input);
    held_.reset();
    if (!read.has_value()) {
        return;
    }
    read->entityType = motive;
    io::ByteBuffer output;
    read->write(output);
    packet.reset(play::SpawnEntityPacket::kId, play::SpawnEntityPacket::kId, output.toVector());
    packet.spawn = MutablePacket::Spawn::kPainting;
}

std::optional<MutablePacket::Continued> OlderPaintings::release(const MutablePacket& next) {
    if (!held_.has_value()) {
        return std::nullopt;
    }
    if (next.nativeId() == play::RemoveEntitiesPacket::kId) {
        io::ByteBuffer input = io::ByteBuffer::wrap(next.body);
        const auto gone = play::RemoveEntitiesPacket::read(input);
        if (gone.has_value() && std::ranges::contains(gone->entityIds, heldId_)) {
            held_.reset();
            return std::nullopt;
        }
    }
    MutablePacket released;
    spawn(released, firstMotive());
    return MutablePacket::Continued{released.id, released.nativeId(), std::move(released.body),
                                    MutablePacket::Spawn::kPainting};
}

std::string_view OlderPaintings::title(std::int32_t motive) noexcept {
    return motive >= 0 && static_cast<std::size_t>(motive) < kMotives.size()
               ? kMotives[static_cast<std::size_t>(motive)].title
               : kMotives.front().title;
}

} // namespace basalt::wire::multiversion::java
