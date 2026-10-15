#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::wire::protocol::play {

struct SetHealthPacket {
    static constexpr std::int32_t kId = 0x6a;

    float health = 20.0F;
    std::int32_t food = 20;
    float saturation = 5.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetHealthPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetHealthPacket&, const SetHealthPacket&) = default;
};

struct SetExperiencePacket {
    static constexpr std::int32_t kId = 0x69;

    float progress = 0.0F;
    std::int32_t level = 0;
    std::int32_t total = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetExperiencePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetExperiencePacket&, const SetExperiencePacket&) = default;
};

/// The defaults are vanilla's own Abilities.
struct ClientboundPlayerAbilitiesPacket {
    static constexpr std::int32_t kId = 0x41;
    static constexpr float kDefaultFlyingSpeed = 0.05F;
    static constexpr float kDefaultWalkingSpeed = 0.1F;

    bool invulnerable = false;
    bool flying = false;
    bool mayFly = false;
    bool instabuild = false;
    float flyingSpeed = kDefaultFlyingSpeed;
    float walkingSpeed = kDefaultWalkingSpeed;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClientboundPlayerAbilitiesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClientboundPlayerAbilitiesPacket&, const ClientboundPlayerAbilitiesPacket&) = default;
};

/// Serverbound, id 0x28: the client toggling flight.
struct ServerboundPlayerAbilitiesPacket {
    static constexpr std::int32_t kId = 0x28;

    bool flying = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundPlayerAbilitiesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundPlayerAbilitiesPacket&, const ServerboundPlayerAbilitiesPacket&) = default;
};

struct RespawnPacket {
    static constexpr std::int32_t kId = 0x54;
    static constexpr std::uint8_t kKeepAttributeModifiers = 1;
    static constexpr std::uint8_t kKeepEntityData = 2;
    static constexpr std::uint8_t kKeepAllData = 3;

    SpawnInfo spawnInfo;
    std::uint8_t dataToKeep = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RespawnPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RespawnPacket&, const RespawnPacket&) = default;
};

/// Serverbound, id 0x0c: the client asking for something only the server can start.
struct ClientCommandPacket {
    static constexpr std::int32_t kId = 0x0c;

    enum class Action : std::uint8_t {
        PerformRespawn,
        RequestStats,
        RequestGameruleValues,
    };

    Action action = Action::PerformRespawn;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClientCommandPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClientCommandPacket&, const ClientCommandPacket&) = default;
};

/// Serverbound, id 0x2a: a state the client entered on its own.
struct PlayerCommandPacket {
    static constexpr std::int32_t kId = 0x2a;

    enum class Action : std::uint8_t {
        StopSleeping,
        StartSprinting,
        StopSprinting,
        StartRidingJump,
        StopRidingJump,
        OpenInventory,
        StartFallFlying,
    };

    std::int32_t entityId = 0;
    Action action = Action::StopSleeping;
    std::int32_t data = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayerCommandPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerCommandPacket&, const PlayerCommandPacket&) = default;
};

/// Without this the client never learns it died, so it keeps playing while the server considers it dead.
struct PlayerCombatKillPacket {
    static constexpr std::int32_t kId = 0x45;

    std::int32_t playerId = 0;
    text::Component message;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayerCombatKillPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerCombatKillPacket&, const PlayerCombatKillPacket&) = default;
};

struct RemoveEntitiesPacket {
    static constexpr std::int32_t kId = 0x4e;

    std::vector<std::int32_t> entityIds;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RemoveEntitiesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RemoveEntitiesPacket&, const RemoveEntitiesPacket&) = default;
};

} // namespace basalt::wire::protocol::play
