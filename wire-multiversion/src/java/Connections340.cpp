#include <basalt/wire/multiversion/java/Connections340.hpp>
#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java::connections340 {
namespace {

namespace flat = flattening340;
using namespace std::string_view_literals;

constexpr int kHeight = 256;

struct Property {
    std::string_view name;
    std::vector<std::string_view> values;
    std::int32_t stride = 1;
};

struct Block {
    std::string_view name;
    std::int32_t base;
    std::int32_t count;
    std::vector<Property> properties;
};

[[nodiscard]] std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    while (!text.empty()) {
        const std::size_t end = text.find(separator);
        parts.push_back(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    }
    return parts;
}

[[nodiscard]] const std::vector<Block>& blocks() {
    static const std::vector<Block> parsed = [] {
        std::vector<Block> out;
        for (const flat::Shape& shape : flat::shapes()) {
            Block block{shape.name, shape.base, shape.count, {}};
            for (const std::string_view one : split(shape.properties, ';')) {
                const std::size_t equals = one.find('=');
                block.properties.push_back(Property{one.substr(0, equals), split(one.substr(equals + 1), ','), 1});
            }
            std::int32_t stride = 1;
            for (auto property = block.properties.rbegin(); property != block.properties.rend(); ++property) {
                property->stride = stride;
                stride *= static_cast<std::int32_t>(property->values.size());
            }
            out.push_back(std::move(block));
        }
        return out;
    }();
    return parsed;
}

[[nodiscard]] const Block* blockOf(std::int32_t state) {
    const auto& all = blocks();
    const auto after = std::ranges::upper_bound(all, state, {}, &Block::base);
    if (after == all.begin()) {
        return nullptr;
    }
    const Block& block = *std::prev(after);
    return state < block.base + block.count ? &block : nullptr;
}

[[nodiscard]] const Block* named(std::string_view name) {
    for (const Block& block : blocks()) {
        if (block.name == name) {
            return &block;
        }
    }
    return nullptr;
}

[[nodiscard]] const Property* propertyOf(const Block& block, std::string_view name) {
    for (const Property& property : block.properties) {
        if (property.name == name) {
            return &property;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string_view get(std::int32_t state, std::string_view name) {
    const Block* block = blockOf(state);
    const Property* property = block != nullptr ? propertyOf(*block, name) : nullptr;
    if (property == nullptr) {
        return {};
    }
    const auto index = ((state - block->base) / property->stride) % static_cast<std::int32_t>(property->values.size());
    return property->values[static_cast<std::size_t>(index)];
}

[[nodiscard]] std::int32_t with(std::int32_t state, std::string_view name, std::string_view value) {
    const Block* block = blockOf(state);
    const Property* property = block != nullptr ? propertyOf(*block, name) : nullptr;
    if (property == nullptr) {
        return state;
    }
    const auto wanted = std::ranges::find(property->values, value);
    if (wanted == property->values.end()) {
        return state;
    }
    const auto size = static_cast<std::int32_t>(property->values.size());
    const std::int32_t current = ((state - block->base) / property->stride) % size;
    return state + (static_cast<std::int32_t>(wanted - property->values.begin()) - current) * property->stride;
}

[[nodiscard]] std::int32_t with(std::int32_t state, std::string_view name, bool value) {
    return with(state, name, value ? "true"sv : "false"sv);
}

[[nodiscard]] std::string_view nameOf(std::int32_t state) {
    const Block* block = blockOf(state);
    return block != nullptr ? block->name : std::string_view{};
}

struct Direction {
    std::string_view name;
    std::int32_t dx;
    std::int32_t dz;
};

constexpr std::array<Direction, 4> kHorizontal = {{{"north", 0, -1}, {"east", 1, 0}, {"south", 0, 1}, {"west", -1, 0}}};

[[nodiscard]] std::size_t indexOf(std::string_view facing) {
    for (std::size_t index = 0; index < kHorizontal.size(); ++index) {
        if (kHorizontal[index].name == facing) {
            return index;
        }
    }
    return 0;
}

[[nodiscard]] std::string_view clockwise(std::string_view facing) {
    return kHorizontal[(indexOf(facing) + 1) % 4].name;
}

[[nodiscard]] std::string_view counterClockwise(std::string_view facing) {
    return kHorizontal[(indexOf(facing) + 3) % 4].name;
}

[[nodiscard]] std::string_view opposite(std::string_view facing) { return kHorizontal[(indexOf(facing) + 2) % 4].name; }

[[nodiscard]] bool sameAxis(std::string_view one, std::string_view other) {
    return indexOf(one) % 2 == indexOf(other) % 2;
}

[[nodiscard]] bool endsWith(std::string_view name, std::string_view suffix) { return name.ends_with(suffix); }

[[nodiscard]] bool isFence(std::string_view name) { return endsWith(name, "_fence"); }

[[nodiscard]] bool isGate(std::string_view name) { return endsWith(name, "_fence_gate"); }

[[nodiscard]] bool isPane(std::string_view name) {
    return endsWith(name, "glass_pane") || name == "minecraft:iron_bars";
}

[[nodiscard]] bool isWall(std::string_view name) { return endsWith(name, "_wall"); }

[[nodiscard]] bool isStairs(std::string_view name) { return endsWith(name, "_stairs"); }

[[nodiscard]] bool isChest(std::string_view name) {
    return name == "minecraft:chest" || name == "minecraft:trapped_chest";
}

[[nodiscard]] bool isDoor(std::string_view name) { return endsWith(name, "_door"); }

[[nodiscard]] bool isWire(std::string_view name) { return name == "minecraft:redstone_wire"; }

[[nodiscard]] bool isTallPlant(std::string_view name) {
    constexpr std::array<std::string_view, 6> kPlants = {"minecraft:sunflower",  "minecraft:lilac",
                                                         "minecraft:tall_grass", "minecraft:large_fern",
                                                         "minecraft:rose_bush",  "minecraft:peony"};
    return std::ranges::find(kPlants, name) != kPlants.end();
}

[[nodiscard]] bool isSnowy(std::string_view name) {
    return name == "minecraft:grass_block" || name == "minecraft:mycelium" || name == "minecraft:podzol";
}

/// parity: RedstoneWireBlock.canConnectTo, for the blocks 1.13 says can give power.
[[nodiscard]] bool givesPower(std::string_view name) {
    return endsWith(name, "_button") || endsWith(name, "_pressure_plate") || name == "minecraft:redstone_torch" ||
           name == "minecraft:redstone_wall_torch" || name == "minecraft:lever" || name == "minecraft:redstone_block" ||
           name == "minecraft:daylight_detector" || name == "minecraft:detector_rail" ||
           name == "minecraft:tripwire_hook" || name == "minecraft:trapped_chest" || name == "minecraft:comparator";
}

class Around {
public:
    Around(const LegacyWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)
        : world_(&world), x_(x), y_(y), z_(z) {}

    [[nodiscard]] std::int32_t at(std::int32_t dx, std::int32_t dy, std::int32_t dz) const {
        return content340::blockState(world_->at(x_ + dx, y_ + dy, z_ + dz));
    }

    [[nodiscard]] std::int32_t toward(const Direction& direction, std::int32_t dy = 0) const {
        return at(direction.dx, dy, direction.dz);
    }

    [[nodiscard]] std::int32_t toward(std::string_view facing) const { return toward(kHorizontal[indexOf(facing)]); }

    [[nodiscard]] bool airAbove() const { return world_->at(x_, y_ + 1, z_) >> 4 == 0; }

private:
    const LegacyWorld* world_;
    std::int32_t x_;
    std::int32_t y_;
    std::int32_t z_;
};

[[nodiscard]] bool sturdy(std::int32_t state) { return flat::sturdy().map(state) != IdMap::kNone; }

[[nodiscard]] bool gateAlong(std::int32_t neighbour, const Direction& direction) {
    return isGate(nameOf(neighbour)) && !sameAxis(get(neighbour, "facing"), direction.name);
}

[[nodiscard]] std::int32_t fence(const Around& around, std::int32_t state) {
    const bool netherBrick = nameOf(state) == "minecraft:nether_brick_fence";
    for (const Direction& direction : kHorizontal) {
        const std::int32_t neighbour = around.toward(direction);
        const std::string_view name = nameOf(neighbour);
        const bool joined = (isFence(name) && (name == "minecraft:nether_brick_fence") == netherBrick) ||
                            gateAlong(neighbour, direction) || sturdy(neighbour);
        state = with(state, direction.name, joined);
    }
    return state;
}

[[nodiscard]] std::int32_t pane(const Around& around, std::int32_t state) {
    for (const Direction& direction : kHorizontal) {
        const std::int32_t neighbour = around.toward(direction);
        state = with(state, direction.name, isPane(nameOf(neighbour)) || sturdy(neighbour));
    }
    return state;
}

[[nodiscard]] std::int32_t wall(const Around& around, std::int32_t state) {
    std::array<bool, 4> joined{};
    for (std::size_t index = 0; index < kHorizontal.size(); ++index) {
        const std::int32_t neighbour = around.toward(kHorizontal[index]);
        joined[index] = isWall(nameOf(neighbour)) || gateAlong(neighbour, kHorizontal[index]) || sturdy(neighbour);
        state = with(state, kHorizontal[index].name, joined[index]);
    }
    const bool alongZ = joined[0] && joined[2] && !joined[1] && !joined[3];
    const bool alongX = joined[1] && joined[3] && !joined[0] && !joined[2];
    return with(state, "up", !(alongZ || alongX) || !around.airAbove());
}

[[nodiscard]] std::int32_t gate(const Around& around, std::int32_t state) {
    const std::string_view facing = get(state, "facing");
    const bool walled =
        isWall(nameOf(around.toward(clockwise(facing)))) || isWall(nameOf(around.toward(counterClockwise(facing))));
    return with(state, "in_wall", walled);
}

/// parity: StairBlock.getStairsShape.
[[nodiscard]] std::int32_t stairs(const Around& around, std::int32_t state) {
    const std::string_view facing = get(state, "facing");
    const std::string_view half = get(state, "half");
    const auto sameStairs = [&](std::int32_t other) { return isStairs(nameOf(other)) && get(other, "half") == half; };
    const auto canTake = [&](std::string_view side) {
        const std::int32_t other = around.toward(side);
        return !isStairs(nameOf(other)) || get(other, "facing") != facing || get(other, "half") != half;
    };
    const std::int32_t behind = around.toward(facing);
    if (sameStairs(behind)) {
        const std::string_view turned = get(behind, "facing");
        if (!sameAxis(turned, facing) && canTake(opposite(turned))) {
            return with(state, "shape", turned == counterClockwise(facing) ? "outer_left"sv : "outer_right"sv);
        }
    }
    const std::int32_t ahead = around.toward(opposite(facing));
    if (sameStairs(ahead)) {
        const std::string_view turned = get(ahead, "facing");
        if (!sameAxis(turned, facing) && canTake(turned)) {
            return with(state, "shape", turned == counterClockwise(facing) ? "inner_left"sv : "inner_right"sv);
        }
    }
    return with(state, "shape", "straight"sv);
}

[[nodiscard]] std::int32_t chest(const Around& around, std::int32_t state) {
    const std::string_view facing = get(state, "facing");
    const auto partner = [&](std::string_view side) {
        const std::int32_t other = around.toward(side);
        return nameOf(other) == nameOf(state) && get(other, "facing") == facing;
    };
    if (partner(clockwise(facing))) {
        return with(state, "type", "left"sv);
    }
    if (partner(counterClockwise(facing))) {
        return with(state, "type", "right"sv);
    }
    return with(state, "type", "single"sv);
}

[[nodiscard]] std::int32_t door(const Around& around, std::int32_t state) {
    if (get(state, "half") == "lower") {
        const std::int32_t upper = around.at(0, 1, 0);
        if (nameOf(upper) != nameOf(state)) {
            return state;
        }
        return with(with(state, "hinge", get(upper, "hinge")), "powered", get(upper, "powered"));
    }
    const std::int32_t lower = around.at(0, -1, 0);
    if (nameOf(lower) != nameOf(state)) {
        return state;
    }
    return with(with(state, "facing", get(lower, "facing")), "open", get(lower, "open"));
}

[[nodiscard]] std::int32_t tallPlant(const Around& around, std::int32_t state) {
    if (get(state, "half") != "upper") {
        return state;
    }
    const std::int32_t lower = around.at(0, -1, 0);
    return isTallPlant(nameOf(lower)) ? with(lower, "half", "upper"sv) : state;
}

[[nodiscard]] std::int32_t wire(const Around& around, std::int32_t state) {
    const bool openAbove = !sturdy(around.at(0, 1, 0));
    for (const Direction& direction : kHorizontal) {
        const std::int32_t neighbour = around.toward(direction);
        const std::string_view name = nameOf(neighbour);
        std::string_view side = "none";
        if (openAbove && sturdy(neighbour) && isWire(nameOf(around.toward(direction, 1)))) {
            side = "up";
        } else if (isWire(name) || givesPower(name) ||
                   (name == "minecraft:repeater" && sameAxis(get(neighbour, "facing"), direction.name)) ||
                   (name == "minecraft:observer" && get(neighbour, "facing") == direction.name) ||
                   (!sturdy(neighbour) && isWire(nameOf(around.toward(direction, -1))))) {
            side = "side";
        }
        state = with(state, direction.name, side);
    }
    return state;
}

[[nodiscard]] std::int32_t tripwire(const Around& around, std::int32_t state) {
    for (const Direction& direction : kHorizontal) {
        const std::int32_t neighbour = around.toward(direction);
        const std::string_view name = nameOf(neighbour);
        const bool joined = name == "minecraft:tripwire" ||
                            (name == "minecraft:tripwire_hook" && get(neighbour, "facing") == opposite(direction.name));
        state = with(state, direction.name, joined);
    }
    return state;
}

[[nodiscard]] std::int32_t chorus(const Around& around, std::int32_t state) {
    const auto part = [](std::int32_t other) {
        const std::string_view name = nameOf(other);
        return name == "minecraft:chorus_plant" || name == "minecraft:chorus_flower";
    };
    for (const Direction& direction : kHorizontal) {
        state = with(state, direction.name, part(around.toward(direction)));
    }
    state = with(state, "up", part(around.at(0, 1, 0)));
    const std::int32_t below = around.at(0, -1, 0);
    return with(state, "down", part(below) || nameOf(below) == "minecraft:end_stone");
}

[[nodiscard]] std::int32_t stem(const Around& around, std::int32_t state) {
    const bool pumpkin = nameOf(state) == "minecraft:pumpkin_stem";
    if (get(state, "age") != "7") {
        return state;
    }
    const Block* attached = named(pumpkin ? "minecraft:attached_pumpkin_stem" : "minecraft:attached_melon_stem");
    for (const Direction& direction : kHorizontal) {
        if (nameOf(around.toward(direction)) == (pumpkin ? "minecraft:pumpkin" : "minecraft:melon") &&
            attached != nullptr) {
            return with(attached->base, "facing", direction.name);
        }
    }
    return state;
}

[[nodiscard]] std::int64_t columnKey(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) << 32) | static_cast<std::uint32_t>(chunkZ);
}

} // namespace

std::int32_t LegacyWorld::at(std::int32_t x, std::int32_t y, std::int32_t z) const {
    if (y < 0 || y >= kHeight) {
        return 0;
    }
    const auto column = columns_.find(columnKey(x >> 4, z >> 4));
    if (column == columns_.end()) {
        return 0;
    }
    const auto& section = column->second[static_cast<std::size_t>(y >> 4)];
    if (!section) {
        return 0;
    }
    return (*section)[static_cast<std::size_t>(((y & 15) << 8) | ((z & 15) << 4) | (x & 15))];
}

void LegacyWorld::set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t legacy) {
    if (y < 0 || y >= kHeight) {
        return;
    }
    const auto column = columns_.find(columnKey(x >> 4, z >> 4));
    if (column == columns_.end()) {
        return;
    }
    auto& section = column->second[static_cast<std::size_t>(y >> 4)];
    if (!section) {
        if (legacy == 0) {
            return;
        }
        section = std::make_unique<Section>();
        section->fill(0);
    }
    (*section)[static_cast<std::size_t>(((y & 15) << 8) | ((z & 15) << 4) | (x & 15))] =
        static_cast<std::uint16_t>(legacy);
}

void LegacyWorld::setSection(std::int32_t chunkX, std::int32_t chunkZ, int section, std::unique_ptr<Section> blocks) {
    const auto column = columns_.find(columnKey(chunkX, chunkZ));
    if (column != columns_.end() && section >= 0 && section < kSections) {
        column->second[static_cast<std::size_t>(section)] = std::move(blocks);
    }
}

bool LegacyWorld::loaded(std::int32_t chunkX, std::int32_t chunkZ) const {
    return columns_.contains(columnKey(chunkX, chunkZ));
}

void LegacyWorld::load(std::int32_t chunkX, std::int32_t chunkZ) { columns_[columnKey(chunkX, chunkZ)] = Column{}; }

void LegacyWorld::unload(std::int32_t chunkX, std::int32_t chunkZ) { columns_.erase(columnKey(chunkX, chunkZ)); }

std::int32_t noteBlock(std::int32_t instrument, std::int32_t note) {
    constexpr std::array<std::string_view, 10> kInstruments = {"harp",  "basedrum", "snare",  "hat",   "bass",
                                                               "flute", "bell",     "guitar", "chime", "xylophone"};
    const Block* block = named("minecraft:note_block");
    if (block == nullptr || instrument < 0 || instrument >= static_cast<std::int32_t>(kInstruments.size())) {
        return -1;
    }
    const std::string tuned = std::to_string(std::clamp(note, 0, 24));
    const std::int32_t state = with(block->base, "instrument", kInstruments[static_cast<std::size_t>(instrument)]);
    return with(with(state, "note", std::string_view{tuned}), "powered", false);
}

bool joins(std::int32_t state) {
    const std::string_view name = nameOf(state);
    return !name.empty() &&
           (isFence(name) || isGate(name) || isPane(name) || isWall(name) || isStairs(name) || isChest(name) ||
            isDoor(name) || isTallPlant(name) || isSnowy(name) || isWire(name) || name == "minecraft:tripwire" ||
            name == "minecraft:chorus_plant" || name == "minecraft:pumpkin_stem" || name == "minecraft:melon_stem");
}

bool joinsSideways(std::int32_t state) {
    const std::string_view name = nameOf(state);
    return joins(state) && !isSnowy(name) && !isDoor(name) && !isTallPlant(name);
}

std::int32_t connect(const LegacyWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t state) {
    const std::string_view name = nameOf(state);
    if (name.empty()) {
        return state;
    }
    const Around around{world, x, y, z};
    if (isGate(name)) {
        return gate(around, state);
    }
    if (isFence(name)) {
        return fence(around, state);
    }
    if (isPane(name)) {
        return pane(around, state);
    }
    if (isWall(name)) {
        return wall(around, state);
    }
    if (isStairs(name)) {
        return stairs(around, state);
    }
    if (isChest(name)) {
        return chest(around, state);
    }
    if (isDoor(name)) {
        return door(around, state);
    }
    if (isTallPlant(name)) {
        return tallPlant(around, state);
    }
    if (isSnowy(name)) {
        const std::string_view above = nameOf(around.at(0, 1, 0));
        return with(state, "snowy", above == "minecraft:snow" || above == "minecraft:snow_block");
    }
    if (isWire(name)) {
        return wire(around, state);
    }
    if (name == "minecraft:tripwire") {
        return tripwire(around, state);
    }
    if (name == "minecraft:chorus_plant") {
        return chorus(around, state);
    }
    if (name == "minecraft:pumpkin_stem" || name == "minecraft:melon_stem") {
        return stem(around, state);
    }
    return state;
}

} // namespace basalt::wire::multiversion::java::connections340
