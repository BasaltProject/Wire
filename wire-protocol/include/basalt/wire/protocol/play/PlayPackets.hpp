#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::play {

/// Clientbound, id 0x32: everything the client needs to enter the game.
struct LoginPlayPacket {
    static constexpr std::int32_t kId = 0x32;

    std::int32_t entityId = 0;
    bool isHardcore = false;
    std::vector<std::string> worldNames;
    std::int32_t maxPlayers = 20;
    std::int32_t viewDistance = 10;
    std::int32_t simulationDistance = 10;
    bool reducedDebugInfo = false;
    bool enableRespawnScreen = true;
    bool doLimitedCrafting = false;
    SpawnInfo spawnInfo;
    bool onlineMode = false;
    bool enforcesSecureChat = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LoginPlayPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LoginPlayPacket&, const LoginPlayPacket&) = default;
};

/// Serverbound, id 0x2c: the client confirming it finished loading the world.
inline constexpr std::int32_t kPlayerLoadedId = 0x2c;

inline constexpr std::int32_t kPunchId = 0x2e;

struct PlayerPositionPacket {
    static constexpr std::int32_t kId = 0x49;

    std::int32_t teleportId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::int32_t relativeFlags = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayerPositionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerPositionPacket&, const PlayerPositionPacket&) = default;
};

struct AcceptTeleportationPacket {
    static constexpr std::int32_t kId = 0;

    std::int32_t teleportId = 0;
    /// Where the client put itself, which it no longer confirms with a separate move packet.
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<AcceptTeleportationPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const AcceptTeleportationPacket&, const AcceptTeleportationPacket&) = default;
};

struct SpawnPositionPacket {
    static constexpr std::int32_t kId = 0x63;

    GlobalPos globalPos;
    float yaw = 0.0F;
    float pitch = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SpawnPositionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SpawnPositionPacket&, const SpawnPositionPacket&) = default;
};

struct PlayerInfoEntry {
    common::Uuid uuid;
    std::string name;
    bool listed = false;
    std::int32_t latencyMillis = 0;
    Gamemode gamemode = Gamemode::Survival;
    /// The signed profile properties from the session server, where a skin and cape live.
    std::vector<login::GameProfileProperty> properties;
    /// Their chat session, which lets every client check what they sign.
    std::optional<RemoteChatSessionData> chatSession;

    friend bool operator==(const PlayerInfoEntry&, const PlayerInfoEntry&) = default;
};

/// The action set is a bitset over vanilla's enum, so its ordinals are protocol.
struct PlayerInfoUpdatePacket {
    static constexpr std::int32_t kId = 0x47;

    static constexpr std::uint8_t kAddPlayer = 1 << 0;
    static constexpr std::uint8_t kInitializeChat = 1 << 1;
    static constexpr std::uint8_t kUpdateGameMode = 1 << 2;
    static constexpr std::uint8_t kUpdateListed = 1 << 3;
    static constexpr std::uint8_t kUpdateLatency = 1 << 4;
    static constexpr std::uint8_t kUpdateDisplayName = 1 << 5;
    static constexpr std::uint8_t kUpdateHat = 1 << 6;
    static constexpr std::uint8_t kUpdateListOrder = 1 << 7;

    static constexpr std::uint8_t kInitializing =
        kAddPlayer | kInitializeChat | kUpdateGameMode | kUpdateListed | kUpdateLatency;

    std::uint8_t actions = kInitializing;
    std::vector<PlayerInfoEntry> entries;

    [[nodiscard]] static PlayerInfoUpdatePacket initializing(std::vector<PlayerInfoEntry> entries);

    /// The single-action update vanilla broadcasts when a player's game mode changes.
    [[nodiscard]] static PlayerInfoUpdatePacket gameMode(const common::Uuid& uuid, Gamemode gamemode);

    /// parity: `ServerGamePacketListenerImpl.resetPlayerChatState`.
    [[nodiscard]] static PlayerInfoUpdatePacket chatSession(const common::Uuid& uuid,
                                                            std::optional<RemoteChatSessionData> session);

    void write(io::ByteBuffer& buffer) const;

    [[nodiscard]] static std::optional<PlayerInfoUpdatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerInfoUpdatePacket&, const PlayerInfoUpdatePacket&) = default;
};

struct PlayerInfoRemovePacket {
    static constexpr std::int32_t kId = 0x46;

    std::vector<common::Uuid> players;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayerInfoRemovePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerInfoRemovePacket&, const PlayerInfoRemovePacket&) = default;
};

/// parity: `PositionMoveRotation` is position, velocity, then two float rotations.
struct EntityTeleportPacket {
    static constexpr std::int32_t kId = 0x80;

    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    double deltaZ = 0.0;
    /// Degrees as floats, not the packed angle bytes older protocols used here.
    float yaw = 0.0F;
    float pitch = 0.0F;
    /// Vanilla's Relative bitmask: which fields the client should add to what it has instead of replacing.
    std::int32_t relatives = 0;
    bool onGround = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<EntityTeleportPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const EntityTeleportPacket&, const EntityTeleportPacket&) = default;
};

struct PlayKeepAlivePacket {
    static constexpr std::int32_t kClientboundId = 0x2d;
    static constexpr std::int32_t kServerboundId = 0x1c;

    std::int64_t keepAliveId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayKeepAlivePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayKeepAlivePacket&, const PlayKeepAlivePacket&) = default;
};

struct SystemChatPacket {
    static constexpr std::int32_t kId = 0x7c;

    text::Component content;
    bool isActionBar = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SystemChatPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SystemChatPacket&, const SystemChatPacket&) = default;
};

struct PlayDisconnectPacket {
    static constexpr std::int32_t kId = 0x20;

    text::Component reason;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayDisconnectPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayDisconnectPacket&, const PlayDisconnectPacket&) = default;
};

/// Serverbound, id 0x1e: a movement-only position update.
struct ServerboundPositionPacket {
    static constexpr std::int32_t kId = 0x1e;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool onGround = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundPositionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundPositionPacket&, const ServerboundPositionPacket&) = default;
};

struct ServerboundPositionLookPacket {
    static constexpr std::int32_t kId = 0x1f;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    bool onGround = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundPositionLookPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundPositionLookPacket&, const ServerboundPositionLookPacket&) = default;
};

struct BlockChangedAckPacket {
    static constexpr std::int32_t kId = 0x04;

    std::int32_t sequence = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BlockChangedAckPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BlockChangedAckPacket&, const BlockChangedAckPacket&) = default;
};

struct ServerboundPlayerActionPacket {
    static constexpr std::int32_t kId = 0x29;

    enum class Action : std::uint8_t {
        StartDestroyBlock,
        ChangeDestroyDirection,
        AbortDestroyBlock,
        StopDestroyBlock,
        DropAllItems,
        DropItem,
        ReleaseUseItem,
        SwapItemWithOffhand,
        Stab,
    };

    Action action = Action::StartDestroyBlock;
    BlockPos pos;
    Direction direction = Direction::Down;
    std::int32_t sequence = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundPlayerActionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundPlayerActionPacket&, const ServerboundPlayerActionPacket&) = default;
};

enum class Hand : std::uint8_t {
    MainHand,
    OffHand,
};

struct BlockHitResult {
    BlockPos pos;
    Direction direction = Direction::Up;
    float clickX = 0.0F;
    float clickY = 0.0F;
    float clickZ = 0.0F;
    bool inside = false;
    bool worldBorderHit = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BlockHitResult> read(io::ByteBuffer& buffer);

    friend bool operator==(const BlockHitResult&, const BlockHitResult&) = default;
};

struct ServerboundUseItemOnPacket {
    static constexpr std::int32_t kId = 0x42;

    Hand hand = Hand::MainHand;
    BlockHitResult blockHit;
    std::int32_t sequence = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundUseItemOnPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundUseItemOnPacket&, const ServerboundUseItemOnPacket&) = default;
};

/// The client predicts the change, so ignoring this leaves scooped water missing on that client for good.
struct ServerboundUseItemPacket {
    static constexpr std::int32_t kId = 0x43;

    Hand hand = Hand::MainHand;
    std::int32_t sequence = 0;
    float yaw = 0.0F;
    float pitch = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundUseItemPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundUseItemPacket&, const ServerboundUseItemPacket&) = default;
};

struct PickItemFromEntityPacket {
    static constexpr std::int32_t kId = 0x25;

    std::int32_t entityId = 0;
    bool includeData = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PickItemFromEntityPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PickItemFromEntityPacket&, const PickItemFromEntityPacket&) = default;
};

struct PickItemFromBlockPacket {
    static constexpr std::int32_t kId = 0x24;

    BlockPos pos;
    bool includeData = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PickItemFromBlockPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PickItemFromBlockPacket&, const PickItemFromBlockPacket&) = default;
};

} // namespace basalt::wire::protocol::play
