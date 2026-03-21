#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// The packets about the session and the player: joining, moving, the tab list, chat, and the
/// actions a client takes on the world.
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

/// Serverbound, id 0x2c: the client confirming it finished loading the world. No payload.
inline constexpr std::int32_t kPlayerLoadedId = 0x2c;

/// Serverbound, id 0x2e: the player punched the air or a block. No payload.
inline constexpr std::int32_t kPunchId = 0x2e;

/// Clientbound, id 0x49: an authoritative position sync, either the initial spawn or a forced
/// teleport.
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

/// Serverbound, id 0x00: acknowledges a teleport.
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

/// Clientbound, id 0x63: the world spawn point the compass points at.
struct SpawnPositionPacket {
    static constexpr std::int32_t kId = 0x63;

    GlobalPos globalPos;
    float yaw = 0.0F;
    float pitch = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SpawnPositionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SpawnPositionPacket&, const SpawnPositionPacket&) = default;
};

/// One tab-list entry. Which of these fields actually travel is decided by the packet's action set.
struct PlayerInfoEntry {
    common::Uuid uuid;
    std::string name;
    bool listed = false;
    std::int32_t latencyMillis = 0;
    Gamemode gamemode = Gamemode::Survival;
    /// The signed profile properties from the session server, where a skin and cape live. A client
    /// renders the default skin for a player it was told nothing about, so these travelling is the
    /// whole difference between everyone looking like Steve and everyone looking like themselves.
    /// Empty in offline mode, since nothing signs them there.
    std::vector<login::GameProfileProperty> properties;

    friend bool operator==(const PlayerInfoEntry&, const PlayerInfoEntry&) = default;
};

/// Clientbound, id 0x47: adds or updates tab list entries.
///
/// Carries a set of actions and, per entry, the fields those actions call for, in the enum's own
/// order: the set is a bitset over that enum, so the ordinals are protocol.
///
/// The game mode is not cosmetic: a client reads its own tab-list entry to decide it is a spectator,
/// so the game event alone switches the HUD while the player still collides.
struct PlayerInfoUpdatePacket {
    static constexpr std::int32_t kId = 0x47;

    /// The bits are positions in vanilla's own action enum, and the four this build does not write
    /// still hold their places.
    static constexpr std::uint8_t kAddPlayer = 1 << 0;
    static constexpr std::uint8_t kInitializeChat = 1 << 1;
    static constexpr std::uint8_t kUpdateGameMode = 1 << 2;
    static constexpr std::uint8_t kUpdateListed = 1 << 3;
    static constexpr std::uint8_t kUpdateLatency = 1 << 4;
    static constexpr std::uint8_t kUpdateDisplayName = 1 << 5;
    static constexpr std::uint8_t kUpdateHat = 1 << 6;
    static constexpr std::uint8_t kUpdateListOrder = 1 << 7;

    /// What vanilla's createPlayerInitializing sends for a player who just joined, minus the actions
    /// this build does not write. The game mode belongs in here: a player who joins in creative is a
    /// spectator-or-not to everyone else from this packet alone.
    static constexpr std::uint8_t kInitializing = kAddPlayer | kUpdateGameMode | kUpdateListed | kUpdateLatency;

    std::uint8_t actions = kInitializing;
    std::vector<PlayerInfoEntry> entries;

    [[nodiscard]] static PlayerInfoUpdatePacket initializing(std::vector<PlayerInfoEntry> entries);

    /// The single-action update vanilla broadcasts when a player's game mode changes.
    [[nodiscard]] static PlayerInfoUpdatePacket gameMode(const common::Uuid& uuid, Gamemode gamemode);

    void write(io::ByteBuffer& buffer) const;

    /// An action this build cannot write is also one it cannot read: the field widths are not on the
    /// wire, so a packet carrying one is refused rather than misparsed.
    [[nodiscard]] static std::optional<PlayerInfoUpdatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerInfoUpdatePacket&, const PlayerInfoUpdatePacket&) = default;
};

/// Clientbound, id 0x46: removes entries from the tab list.
struct PlayerInfoRemovePacket {
    static constexpr std::int32_t kId = 0x46;

    std::vector<common::Uuid> players;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayerInfoRemovePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerInfoRemovePacket&, const PlayerInfoRemovePacket&) = default;
};

/// Clientbound, id 0x80: an absolute position and rotation update for an entity other than the
/// receiving player.
///
/// parity: PositionMoveRotation is position, then velocity, then two floats of rotation. The
/// velocity is not optional, and leaving it out is what made a client read three doubles it was
/// owed, run off the end of the packet and drop the connection.
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
    /// Vanilla's Relative bitmask: which fields the client should add to what it has instead of
    /// replacing. Zero means everything is absolute, which is what telling a client where somebody
    /// *is* always means.
    std::int32_t relatives = 0;
    bool onGround = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<EntityTeleportPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const EntityTeleportPacket&, const EntityTeleportPacket&) = default;
};

/// Both directions in play, with different ids.
struct PlayKeepAlivePacket {
    static constexpr std::int32_t kClientboundId = 0x2d;
    static constexpr std::int32_t kServerboundId = 0x1c;

    std::int64_t keepAliveId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayKeepAlivePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayKeepAlivePacket&, const PlayKeepAlivePacket&) = default;
};

/// Clientbound, id 0x7c: an unsigned chat message.
///
/// Used both for real system messages and, for now, to relay player chat: signed player chat, with
/// its session registration and message signing, is a documented gap, so relayed messages render
/// with system styling rather than a player name and head.
struct SystemChatPacket {
    static constexpr std::int32_t kId = 0x7c;

    text::Component content;
    bool isActionBar = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SystemChatPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SystemChatPacket&, const SystemChatPacket&) = default;
};

/// Serverbound, id 0x09: a chat message.
///
/// The signing fields are read to keep the wire format correct but are not validated: see the system
/// chat packet for the signed-chat gap.
struct ChatMessagePacket {
    static constexpr std::int32_t kId = 0x09;
    static constexpr std::size_t kSignatureLength = 256;
    static constexpr std::size_t kAcknowledgedLength = 3;

    std::string message;
    std::int64_t timestamp = 0;
    std::int64_t salt = 0;
    std::optional<std::vector<std::uint8_t>> signature;
    std::int32_t offset = 0;
    std::vector<std::uint8_t> acknowledged;
    std::int8_t checksum = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChatMessagePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChatMessagePacket&, const ChatMessagePacket&) = default;
};

/// Clientbound, id 0x20: kicks the player with a reason.
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

/// Serverbound, id 0x1f: a position and rotation update.
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

/// Clientbound, id 0x04: acknowledges a client-predicted dig or place, so the client can stop
/// predicting past that point.
struct BlockChangedAckPacket {
    static constexpr std::int32_t kId = 0x04;

    std::int32_t sequence = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BlockChangedAckPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BlockChangedAckPacket&, const BlockChangedAckPacket&) = default;
};

/// Serverbound, id 0x29: digging a block, or a non-block player action such as dropping an item.
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

/// Which hand an interaction used.
enum class Hand : std::uint8_t {
    MainHand,
    OffHand,
};

/// Where on a block's face an interaction landed.
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

/// Serverbound, id 0x42: using the held item on a block, which is placing and opening containers.
struct ServerboundUseItemOnPacket {
    static constexpr std::int32_t kId = 0x42;

    Hand hand = Hand::MainHand;
    BlockHitResult blockHit;
    std::int32_t sequence = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundUseItemOnPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundUseItemOnPacket&, const ServerboundUseItemOnPacket&) = default;
};

/// Serverbound, id 0x43: using an item with no block under the crosshair.
///
/// A bucket rides on this rather than on the use-on packet: fluids are not block targets, and the
/// item's own use is what fills or empties it. The client predicts the change locally and undoes it
/// only when the ack arrives, so a server that ignores this leaves the water it just scooped missing
/// from that client for good.
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

/// Serverbound, id 0x25: middle-click pick block on an entity, which answers with its spawn egg or
/// whatever item it is made of.
struct PickItemFromEntityPacket {
    static constexpr std::int32_t kId = 0x25;

    std::int32_t entityId = 0;
    /// The control-click that copies the entity's data, which this build ignores.
    bool includeData = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PickItemFromEntityPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PickItemFromEntityPacket&, const PickItemFromEntityPacket&) = default;
};

/// Serverbound, id 0x24: middle-click pick block on a targeted block.
struct PickItemFromBlockPacket {
    static constexpr std::int32_t kId = 0x24;

    BlockPos pos;
    /// Asks for the block entity's data copied onto the picked stack, which this build ignores.
    bool includeData = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PickItemFromBlockPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PickItemFromBlockPacket&, const PickItemFromBlockPacket&) = default;
};

} // namespace basalt::wire::protocol::play
