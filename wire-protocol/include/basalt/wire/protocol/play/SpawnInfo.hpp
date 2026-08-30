#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::wire::protocol::play {

enum class Gamemode : std::uint8_t {
    Survival,
    Creative,
    Adventure,
    Spectator,
};

/// A position that names its dimension, which is what a death point has to be: the client shows the
/// compass needle in the overworld for somebody who died in the nether.
struct GlobalPos {
    std::string dimensionName;
    BlockPos location;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static GlobalPos read(io::ByteBuffer& buffer);

    friend bool operator==(const GlobalPos&, const GlobalPos&) = default;
};

/// Everything the client needs to enter a world, shared by the join and the respawn packets.
struct SpawnInfo {
    std::int32_t dimension = 0;
    std::string dimensionName;
    std::int64_t hashedSeed = 0;
    Gamemode gamemode = Gamemode::Survival;
    /// -1 when there was none, which is what a fresh join sends.
    std::int32_t previousGamemode = 255;
    bool isDebug = false;
    bool isFlat = false;
    std::optional<GlobalPos> death;
    std::int32_t portalCooldown = 0;
    std::int32_t seaLevel = 63;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SpawnInfo> read(io::ByteBuffer& buffer);

    friend bool operator==(const SpawnInfo&, const SpawnInfo&) = default;
};

} // namespace basalt::wire::protocol::play
