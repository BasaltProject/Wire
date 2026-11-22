#pragma once

#include "HostApi.hpp"

#include <array>
#include <cstring>
#include <map>
#include <set>

namespace basalt::wire::wasm::trace::worlds {

using Pos = std::array<std::int32_t, 3>;
using Chunk = std::pair<std::int32_t, std::int32_t>;

struct SimWorld {
    std::string name;
    bool alive = true;
    std::string dimension = "minecraft:overworld";
    std::int64_t seed = 0;
    std::map<Pos, std::int32_t> blocks;
    std::set<Chunk> loaded;
    std::map<std::string, std::string> rules{{"doDaylightCycle", "true"}, {"randomTickSpeed", "3"}};
    std::int64_t dayTime = 1000;
    bool raining = false;
    bool thundering = false;
    std::string difficulty = "normal";
    bool locked = false;
    Pos spawn{0, 64, 0};
    std::int32_t viewDistance = 10;
    std::array<std::int32_t, 3> weatherTimes{0, 0, 0};
    std::vector<std::uint8_t> pluginData;
    std::map<Chunk, std::vector<std::uint8_t>> chunkData;
    std::map<Pos, std::vector<std::uint8_t>> blockEntities;
    std::map<Chunk, std::vector<std::uint8_t>> stored;
    std::map<std::string, std::set<Chunk>> held;
    std::map<std::string, double> border{
        {"center_x", 0.0},           {"center_z", 0.0},         {"size", 60000000.0},
        {"target_size", 60000000.0}, {"lerp_ticks", 0.0},       {"damage_amount", 0.2},
        {"damage_buffer", 5.0},      {"warning_distance", 5.0}, {"warning_time", 15.0}};
    std::map<Pos, std::int32_t> biomeCells;
    std::vector<std::vector<std::uint8_t>> lightWrites;
};

struct Sim {
    std::vector<SimWorld> worlds;
    std::map<std::string, std::vector<std::uint8_t>> templates;
    std::vector<bool> noises;
};

[[nodiscard]] Sim& sim(World& world);

inline constexpr std::int32_t kSectionBlocks = 4096;
inline constexpr std::int32_t kMinY = -64;
inline constexpr std::int32_t kHeight = 384;

[[nodiscard]] std::int32_t stateId(std::string_view text);
[[nodiscard]] std::string stateText(std::int32_t id);

[[nodiscard]] inline Chunk chunkOf(std::int32_t x, std::int32_t z) { return {x >> 4, z >> 4}; }

[[nodiscard]] inline std::string list(std::span<const std::int32_t> values) {
    std::string text = "[";
    for (const std::int32_t value : values) {
        text += (text.size() > 1 ? ", " : "") + std::to_string(value);
    }
    return text + "]";
}

[[nodiscard]] inline std::string hex(std::span<const std::uint8_t> bytes) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string text = "b\"";
    for (const std::uint8_t byte : bytes) {
        text += kDigits[byte >> 4U];
        text += kDigits[byte & 0xFU];
    }
    return text + '"';
}

/// One traced import: the call line is built from a kind string, one letter per parameter group.
///   i i32, l i64, f f32, d f64, s string (ptr, len), v value (ptr, len), y bytes (ptr, len), x pointer not shown,
///   o output buffer (ptr, cap), w world handle. Parameter 0 is the world handle when `w` leads.
class Traced {
public:
    Traced(void* user, Call& call, std::string_view name, std::string_view kinds);

    Rec rec;
    Call& c;
    World& world;
    GuestMemory memory;
    SimWorld* sim = nullptr;
    std::int32_t status = kOk;

    std::int32_t end(std::int32_t result) {
        c.ret(rec.done(result));
        return result;
    }

    std::int32_t endBytes(std::int32_t ptr, std::int32_t cap, std::span<const std::uint8_t> data) {
        const std::int32_t result =
            memory.put(ptr, cap, std::string_view{reinterpret_cast<const char*>(data.data()), data.size()});
        c.ret(result >= 0 && result <= cap ? rec.done(result, hex(data)) : rec.done(result));
        return result;
    }

    std::int32_t endText(std::int32_t ptr, std::int32_t cap, std::string_view data) {
        c.ret(rec.doneBuffer(memory, ptr, cap, data));
        return 0;
    }

    std::int32_t endInts(std::int32_t ptr, std::int32_t cap, std::span<const std::int32_t> data) {
        const auto needed = static_cast<std::int32_t>(data.size());
        std::int32_t result = needed;
        if (needed <= cap && needed > 0) {
            const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(needed) * 4U);
            if (!view) {
                c.ret(rec.done(kErrInvalid));
                return kErrInvalid;
            }
            std::memcpy(view->data(), data.data(), view->size());
        }
        c.ret(needed <= cap ? rec.done(result, list(data)) : rec.done(result));
        return result;
    }

    std::int32_t endValue(std::int32_t ptr, std::int32_t cap, const Value& value) {
        const std::int32_t result = putValue(memory, ptr, cap, value);
        c.ret(result >= 0 && result <= cap ? rec.done(result, canonical(value)) : rec.done(result));
        return result;
    }

    [[nodiscard]] std::optional<std::vector<std::int32_t>> ints(std::int32_t ptr, std::int32_t count) const {
        if (count < 0 || count > (1 << 24)) {
            return std::nullopt;
        }
        std::vector<std::int32_t> out(static_cast<std::size_t>(count));
        if (count == 0) {
            return out;
        }
        const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(count) * 4U);
        if (!view) {
            return std::nullopt;
        }
        std::memcpy(out.data(), view->data(), view->size());
        return out;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> bytes(std::int32_t ptr, std::int32_t len) const {
        if (len < 0) {
            return std::nullopt;
        }
        const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(len));
        if (!view) {
            return std::nullopt;
        }
        return std::vector<std::uint8_t>(view->begin(), view->end());
    }

    /// Writes ints to a fixed-size output the guest sized itself; false when it does not fit memory.
    [[nodiscard]] bool writeInts(std::int32_t ptr, std::span<const std::int32_t> data) const {
        const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(data.size()) * 4U);
        if (!view) {
            return false;
        }
        std::memcpy(view->data(), data.data(), view->size());
        return true;
    }
};

[[nodiscard]] inline bool loaded(const SimWorld& world, std::int32_t x, std::int32_t z) {
    return world.loaded.contains(chunkOf(x, z));
}

[[nodiscard]] inline std::int32_t stateAt(const SimWorld& world, const Pos& at) {
    const auto found = world.blocks.find(at);
    return found == world.blocks.end() ? 0 : found->second;
}

[[nodiscard]] std::int32_t intArg(const Value& map, std::string_view key, std::int32_t fallback = 0);
[[nodiscard]] double realArg(const Value& map, std::string_view key, double fallback = 0.0);

} // namespace basalt::wire::wasm::trace::worlds
