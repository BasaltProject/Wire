#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

/// The packet that starts the game, read and written field by field. It cannot go through the packet
/// class: by the time a link halfway down the chain sees it, the links above have already taken
/// fields out, so the reader that matches this server no longer matches the bytes.
///
/// What each version dropped, newest first: 26.2 added `onlineMode`, 1.21.2 the spawn sea level.
struct LoginFields {
    /// The half of it that describes where the player is, which the respawn packet carries alone.
    struct SpawnFields {
        std::int32_t dimension = 0;
        std::string dimensionName;
        std::int64_t hashedSeed = 0;
        std::int32_t gamemode = 0;
        std::int32_t previousGamemode = 0;
        bool debug = false;
        bool flat = false;
        std::optional<std::string> deathDimension;
        std::int64_t deathPosition = 0;
        std::int32_t portalCooldown = 0;
        std::int32_t seaLevel = 0;

        void write(io::ByteBuffer& output, bool withSeaLevel) const;

        /// `varIntGamemodes` reads 26.3's shape: the mode as a varint and the previous one as an
        /// optional varint, where every version below writes two bytes.
        [[nodiscard]] static SpawnFields read(io::ByteBuffer& input, bool withSeaLevel, bool varIntGamemodes = false);
    };

    std::int32_t entityId = 0;
    bool hardcore = false;
    std::vector<std::string> worldNames;
    std::int32_t maxPlayers = 0;
    std::int32_t viewDistance = 0;
    std::int32_t simulationDistance = 0;
    bool reducedDebugInfo = false;
    bool respawnScreen = true;
    bool limitedCrafting = false;
    SpawnFields spawn;
    bool onlineMode = false;
    bool enforcesSecureChat = false;

    void write(io::ByteBuffer& output, bool withOnlineMode, bool withSeaLevel) const;

    [[nodiscard]] static LoginFields read(io::ByteBuffer& input, bool withOnlineMode, bool withSeaLevel,
                                          bool varIntGamemodes = false);

    /// Where the player died, as a block position, for the versions that want it unpacked.
    [[nodiscard]] std::optional<protocol::play::BlockPos> deathBlock() const;
};

} // namespace basalt::wire::multiversion::java
