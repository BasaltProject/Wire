#pragma once

#include <basalt/wire/bedrock/BedrockEntityData.hpp>
#include <basalt/wire/bedrock/BedrockPacketIds.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::bedrock {

inline constexpr int kShapesChangedProtocol = 2168;

[[nodiscard]] constexpr bool writesNativeShapes(const int protocol) { return protocol >= kShapesChangedProtocol; }

inline constexpr int kStartGameGrewProtocol = 1001;

inline constexpr int kEnchantCostByteProtocol = 975;

inline constexpr int kServerSoundHandleProtocol = 975;

inline constexpr int kInputFlagListProtocol = 2168;

/// Where an entity event gained a trailing optional position, which only a fire event fills.
inline constexpr int kEntityEventGrewProtocol = 975;

inline constexpr int kItemDescriptorProtocol = 1001;

inline constexpr int kHeldItemDescriptorProtocol = 2168;

inline constexpr int kItemCooldownProtocol = 944;

inline constexpr int kAirItemInstanceGrewProtocol = 2168;

/// Three bytes, and everything after them in the packet reads shifted.
inline constexpr int kCreativeCategoryByteProtocol = 2168;

inline constexpr int kTransactionFlagsProtocol = 1001;

inline constexpr int kSingleFlagProtocol = 2193;

inline constexpr int kItemUseHandProtocol = 2193;

inline constexpr int kBossBarPlayerlessProtocol = 2193;

inline constexpr int kCameraStartRotationProtocol = 2193;

inline constexpr int kSoundPlaybackProtocol = 2193;

inline constexpr int kScoreKindProtocol = 2168;

/// Where a jukebox plays its record under a server sound handle and says so with `RecordStarted`.
inline constexpr int kRecordStartedProtocol = 2193;

/// What a client asks for before anything else, and the only packet either side sends in the clear and uncompressed.
struct RequestNetworkSettings {
    int protocolVersion = 0;
};

[[nodiscard]] std::optional<RequestNetworkSettings> readRequestNetworkSettings(io::ByteBuffer& buffer);

void writeNetworkSettings(io::ByteBuffer& buffer, std::uint16_t compressionThreshold, std::uint16_t algorithm);

struct LoginRequest {
    int protocolVersion = 0;
    std::string name;
    std::string xuid;
    std::string identityUuid;
    /// The client's public key, as the DER a JWT header carries.
    std::vector<std::uint8_t> identityPublicKey;
    /// True when Mojang signed the chain, rather than the client signing itself.
    bool authenticated = false;

    std::string skinId;
    std::string skinGeometry;
    std::uint32_t skinWidth = 0;
    std::uint32_t skinHeight = 0;
    std::vector<std::uint8_t> skinPixels;
};

[[nodiscard]] std::optional<LoginRequest> readLogin(io::ByteBuffer& buffer);

enum class ResourcePackResponse : std::uint8_t {
    Refused = 1,
    SendPacks = 2,
    HaveAllPacks = 3,
    Completed = 4,
};

[[nodiscard]] std::optional<ResourcePackResponse> readResourcePackResponse(io::ByteBuffer& buffer, int protocol);

enum class PlayStatus : std::int32_t {
    LoginSuccess = 0,
    ClientTooOld = 1,
    ServerTooOld = 2,
    PlayerSpawn = 3,
};

void writePlayStatus(io::ByteBuffer& buffer, PlayStatus status);

void writeServerToClientHandshake(io::ByteBuffer& buffer, std::string_view jwt);

void writeResourcePacksInfo(io::ByteBuffer& buffer, int protocol);

void writeResourcePackStack(io::ByteBuffer& buffer, std::string_view gameVersion);

void writeDisconnect(io::ByteBuffer& buffer, std::string_view reason);

/// Everything the client needs before it will draw a world.
struct StartGameInfo {
    std::int64_t uniqueEntityId = 1;
    std::uint64_t runtimeEntityId = 1;
    std::int32_t playerGameType = 0;
    float x = 0.0F;
    float y = 64.0F;
    float z = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;

    std::int64_t seed = 0;
    std::int32_t dimension = 0;
    std::int32_t levelGameType = 0;
    std::int32_t difficulty = 2;
    std::int32_t spawnX = 0;
    std::int32_t spawnY = 64;
    std::int32_t spawnZ = 0;
    bool commandsEnabled = true;
    std::int64_t currentTick = 0;

    std::string levelId = "basalt";
    std::string levelName = "Basalt";
    std::string vanillaVersion;
    std::string multiplayerCorrelationId;
    std::string serverId;
    std::string worldId;
};

void writeStartGame(io::ByteBuffer& buffer, const StartGameInfo& info, int protocol);

enum class TextType : std::uint8_t {
    Raw = 0,
    Chat = 1,
    Translation = 2,
    Popup = 3,
    JukeboxPopup = 4,
    Tip = 5,
    System = 6,
    Whisper = 7,
    Announcement = 8,
};

/// The source is only written for the types that carry a name.
void writeText(io::ByteBuffer& buffer, TextType type, std::string_view source, std::string_view message,
               std::string_view xuid = {});

/// What a client typed, or whatever else it sent as text.
struct IncomingText {
    TextType type = TextType::Raw;
    std::string source;
    std::string message;
};

[[nodiscard]] std::optional<IncomingText> readText(io::ByteBuffer& buffer);

void writeTeleport(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, float x, float y, float z, float yaw,
                   float pitch, std::uint64_t tick, int protocol);

void writeSetPlayerGameType(io::ByteBuffer& buffer, std::int32_t gameType);

/// The number is the protocol's own order.
enum class TitleType : std::int32_t {
    Clear = 0,
    Reset = 1,
    Title = 2,
    Subtitle = 3,
    ActionBar = 4,
    Times = 5,
};

/// parity: how long a line over the hotbar lasts in vanilla, `Gui.OVERLAY_MESSAGE_TIME`. Bedrock.
inline constexpr std::int32_t kActionBarTicks = 60;

void writeSetTitle(io::ByteBuffer& buffer, TitleType type, std::string_view text, std::int32_t fadeIn,
                   std::int32_t stay, std::int32_t fadeOut);

void writeSetTime(io::ByteBuffer& buffer, std::int32_t time);

/// A command a client typed.
[[nodiscard]] std::optional<std::string> readCommandRequest(io::ByteBuffer& buffer);

/// A form, which is what a Bedrock player gets instead of a window of items.
void writeModalFormRequest(io::ByteBuffer& buffer, std::uint32_t formId, std::string_view json);

/// Nothing to draw any more, for a form the server takes back.
void writeCloseForm(io::ByteBuffer& buffer);

struct ModalFormResponse {
    std::uint32_t formId = 0;
    std::optional<std::string> data;
};

[[nodiscard]] std::optional<ModalFormResponse> readModalFormResponse(io::ByteBuffer& buffer);

[[nodiscard]] std::string simpleFormJson(std::string_view title, std::string_view content,
                                         std::span<const std::string> buttons);

/// Only the five that carry a position are worth naming here; the rest arrive as their number and are ignored.
enum class BedrockBlockActionType : std::int32_t {
    StartBreak = 0,
    AbortBreak = 1,
    StopBreak = 2,
    ContinueBreak = 18,
    PredictDestroy = 26,
    ContinueDestroy = 27,
};

struct BedrockBlockAction {
    std::int32_t action = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// Which side of the block was hit, in Bedrock's order: down, up, north, south, west, east.
    std::int32_t face = 0;
};

struct BedrockItemUse {
    std::int32_t action = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t face = 0;
    std::int32_t hotbarSlot = 0;
    std::int32_t itemRuntimeId = 0;
    float clickX = 0.0F;
    float clickY = 0.0F;
    float clickZ = 0.0F;
};

/// One end of a move: which window, which slot of it, and what the client believes is there.
struct BedrockStackSlot {
    std::int32_t container = 0;
    std::int32_t slot = 0;
    std::int32_t stackNetworkId = 0;
};

/// The numbering differs between protocols, so it is resolved here and the caller never sees a raw id.
enum class BedrockStackActionType : std::int32_t {
    Other = 0,
    Take,
    Place,
    Swap,
    Drop,
    Destroy,
    Consume,
    Create,
    CraftResults,
    /// A recipe the client asks to be made, named by the number this server gave it at login.
    Craft,
    CraftOptional,
    CraftGrindstone,
    BeaconPayment,
    CraftLoom,
};

struct BedrockStackAction {
    BedrockStackActionType type = BedrockStackActionType::Other;
    std::int32_t line = 0;
    std::int32_t count = 0;
    BedrockStackSlot source;
    BedrockStackSlot destination;
    std::int32_t recipeNetId = 0;
    /// How many times over, which the client counts for a stack crafted at once.
    std::int32_t crafts = 1;
    std::int32_t textIndex = -1;
    std::int32_t primaryEffect = 0;
    std::int32_t secondaryEffect = 0;
    std::string pattern;
};

/// A client asking for stacks to be moved.
struct BedrockStackRequest {
    std::int32_t requestId = 0;
    std::vector<BedrockStackAction> actions;
    /// What the client typed where a server may filter it: an anvil's name box is the only one this build reads.
    std::vector<std::string> typed;
};

struct PlayerAuthInput {
    float x = 0.0F;
    /// As the client writes it, which is its eyes and not its feet.
    float y = 0.0F;
    float z = 0.0F;
    float pitch = 0.0F;
    float yaw = 0.0F;
    float headYaw = 0.0F;

    bool sneaking = false;

    bool onGround = false;

    bool sprinting = false;

    std::optional<bool> flying;

    std::vector<BedrockBlockAction> blockActions;

    std::optional<BedrockItemUse> itemUse;

    std::optional<BedrockStackRequest> stackRequest;
};

inline constexpr float kPlayerEyeHeight = 1.62001F;

/// What is left is input state, which this server works out for itself.
[[nodiscard]] std::optional<PlayerAuthInput> readPlayerAuthInput(io::ByteBuffer& buffer, int protocol);

/// Only the ones this server draws are named; the list has not moved in this protocol window.
enum class BedrockContainerType : std::int8_t {
    Inventory = -1,
    Container = 0,
    Workbench = 1,
    Furnace = 2,
    Enchantment = 3,
    Anvil = 5,
    BrewingStand = 4,
    Dispenser = 6,
    Dropper = 7,
    Hopper = 8,
    Beacon = 13,
    Loom = 24,
    Grindstone = 26,
    SmithingTable = 33,
    BlastFurnace = 27,
    Smoker = 28,
    Stonecutter = 29,
    Cartography = 30,
    Crafter = 36,
};

void writeBlockEvent(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t b0,
                     std::int32_t b1);

struct BedrockLevelEffect {
    /// Zero when this edition has no such effect and only the sound is worth sending.
    std::int32_t event = 0;
    std::string_view sound;
    /// The middle unless the effect says otherwise.
    float x = 0.5F;
    float y = 0.5F;
    float z = 0.5F;
    bool keepsData = false;
    bool translatesBlock = false;
};

[[nodiscard]] BedrockLevelEffect bedrockLevelEffect(std::int32_t javaEvent, std::int32_t data);

void writeLevelEvent(io::ByteBuffer& buffer, std::int32_t event, float x, float y, float z, std::int32_t data);

/// The id is this server's own and comes back on everything the client says about that window, until it closes.
void writeContainerOpen(io::ByteBuffer& buffer, std::int32_t containerId, BedrockContainerType type, std::int32_t x,
                        std::int32_t y, std::int32_t z);

/// Each one is sent on its own, which is that edition's shape and not a convenience of ours.
enum class BedrockContainerData : std::int32_t {
    /// parity: Java's own property 2.
    FurnaceCookTime = 0,
    FurnaceLitTime = 1,
    FurnaceLitDuration = 2,
};

void writeContainerSetData(io::ByteBuffer& buffer, std::int32_t containerId, BedrockContainerData property,
                           std::int32_t value);

void writeContainerClose(io::ByteBuffer& buffer, std::int32_t containerId, BedrockContainerType type,
                         bool serverInitiated);

struct BedrockContainerClose {
    std::int32_t containerId = 0;
    bool serverInitiated = false;
};

[[nodiscard]] std::optional<BedrockContainerClose> readContainerClose(io::ByteBuffer& buffer);

/// What one slot of a window holds after a request was answered.
struct BedrockResponseSlot {
    std::int32_t slot = 0;
    std::int32_t count = 0;
    std::int32_t stackNetworkId = 0;
};

struct BedrockResponseContainer {
    std::int32_t container = 0;
    std::vector<BedrockResponseSlot> slots;
};

void writeItemStackResponse(io::ByteBuffer& buffer, std::int32_t requestId, bool accepted,
                            std::span<const BedrockResponseContainer> containers, int protocol);

/// How far the client should ask the server for, and how far it draws.
void writeChunkRadiusUpdated(io::ByteBuffer& buffer, std::int32_t radius);

[[nodiscard]] std::optional<std::int32_t> readRequestChunkRadius(io::ByteBuffer& buffer);

void writeNetworkChunkPublisherUpdate(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::uint32_t radius);

inline constexpr std::size_t kSubChunkBlocks = 4096;

/// The order is Java's own, y innermost.
using JavaSubChunk = std::span<const std::int32_t>;

void writeSubChunk(io::ByteBuffer& buffer, JavaSubChunk blocks, std::int8_t subChunkIndex, int protocol);

/// A client told no entity identifiers refuses the ones it is shown.
void writeAvailableEntityIdentifiers(io::ByteBuffer& buffer);

void writeBiomeDefinitionList(io::ByteBuffer& buffer);

/// Fewer than all of them is a way of asking which one a client will not take.
void writeCreativeContent(io::ByteBuffer& buffer, int protocol, std::size_t items = SIZE_MAX);

/// The recipe book, which this server has nothing to put in yet.

/// The handful of rules a Bedrock client decides things by on its own.
void writeGameRulesChanged(io::ByteBuffer& buffer, int protocol);

void writeSetCommandsEnabled(io::ByteBuffer& buffer, bool enabled);

void writeUpdateAbilities(io::ByteBuffer& buffer, std::int64_t entityId, bool isOperator, bool creative);

/// A gamemode that only looks is this and not an ability.
void writeUpdateAdventureSettings(io::ByteBuffer& buffer, bool immutableWorld);

void writePlaySound(io::ByteBuffer& buffer, std::string_view sound, double x, double y, double z, float volume,
                    float pitch, int protocol, std::optional<std::int64_t> soundHandle = std::nullopt);

void writeRecordStarted(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z,
                        std::int64_t soundHandle);

void writeStopSoundHandle(io::ByteBuffer& buffer, std::int64_t soundHandle);

void writeUpdateBlock(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t javaState,
                      int protocol);

void writeLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t count,
                     std::span<const std::uint8_t> subChunks, std::uint32_t biomeSections, std::int32_t dimension,
                     int protocol, std::span<const std::uint8_t> blockEntities = {},
                     std::span<const std::uint8_t> biomes = {});

/// It is what a client needs to stop waiting for a chunk it was told to expect.
void writeEmptyLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t subChunkCount,
                          std::int32_t dimension, int protocol);

/// Only the swing matters here: the client that swung animates itself, so this is what the others are told.
enum class BedrockAnimation : std::int32_t {
    None = 0,
    SwingArm = 1,
    WakeUp = 3,
    CriticalHit = 4,
    MagicCriticalHit = 5,
};

void writeAnimate(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, BedrockAnimation action);

struct BedrockScoreLine {
    std::int64_t lineId = 0;
    std::string name;
    std::int32_t score = 0;
};

void writeSetDisplayObjective(io::ByteBuffer& buffer, std::string_view slot, std::string_view objective,
                              std::string_view title);

void writeRemoveObjective(io::ByteBuffer& buffer, std::string_view objective);

void writeSetScore(io::ByteBuffer& buffer, std::string_view objective, std::span<const BedrockScoreLine> lines,
                   int protocol);

void writeRemoveScores(io::ByteBuffer& buffer, std::string_view objective, std::span<const std::int64_t> lineIds,
                       int protocol);

enum class BedrockBossAction : std::int32_t {
    Create = 0,
    RegisterPlayer = 1,
    Remove = 2,
    UnregisterPlayer = 3,
    UpdatePercentage = 4,
    UpdateName = 5,
    UpdateProperties = 6,
    UpdateStyle = 7,
};

void writeBossEvent(io::ByteBuffer& buffer, std::int64_t bossEntityId, BedrockBossAction action, std::string_view title,
                    float progress, int protocol);

/// Sends this client to another server, which is what a proxy does with a command.
void writeTransfer(io::ByteBuffer& buffer, std::string_view address, std::uint16_t port, int protocol);

void writeSignTag(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::string_view front,
                  std::string_view back, bool waxed);

void writeSignData(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::string_view front,
                   std::string_view back, bool waxed);

void writeBedTag(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::uint8_t colour);

struct BedrockBannerLayer {
    std::string_view pattern;
    std::int32_t colour = 0;
};

void writeBannerTag(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t base,
                    std::span<const BedrockBannerLayer> layers, bool ominous, std::string_view customName);

void writeBedData(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::uint8_t colour);

/// Without it a Bedrock player puts a sign down and is never given anywhere to write.
struct BedrockSignEdit {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// Java's four lines are the caller's business.
    std::string text;
};

[[nodiscard]] std::optional<BedrockSignEdit> readSignEdit(io::ByteBuffer& buffer);

/// What a client says its own arm did.
struct BedrockAnimation2 {
    std::int32_t action = 0;
    std::uint64_t runtimeEntityId = 0;
};

[[nodiscard]] std::optional<BedrockAnimation2> readAnimate(io::ByteBuffer& buffer);

/// A block the client asked to be handed, which is the middle click.
struct BedrockBlockPick {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t hotbarSlot = 0;
};

[[nodiscard]] std::optional<BedrockBlockPick> readBlockPickRequest(io::ByteBuffer& buffer);

/// A mob the client asked to be handed, which is the middle click on something alive.
struct BedrockEntityPick {
    std::uint64_t runtimeEntityId = 0;
    std::int32_t hotbarSlot = 0;
};

[[nodiscard]] std::optional<BedrockEntityPick> readEntityPickRequest(io::ByteBuffer& buffer);

struct BedrockLecternPage {
    std::int32_t page = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

[[nodiscard]] std::optional<BedrockLecternPage> readLecternUpdate(io::ByteBuffer& buffer, int protocol);

void writeStopSound(io::ByteBuffer& buffer, std::string_view sound, bool stopAll);

void writePlayerStartItemCooldown(io::ByteBuffer& buffer, std::string_view category, std::int32_t durationTicks);

/// parity: `ShowCreditsPacket.Status`.
enum class BedrockCredits : std::uint8_t { Starting = 0, Finished = 1 };

[[nodiscard]] std::optional<BedrockCredits> readShowCredits(io::ByteBuffer& buffer);

/// parity: `EntityEventPacket` travels both ways, and what a client.
struct BedrockEntityEventIn {
    std::uint64_t runtimeEntityId = 0;
    std::uint8_t event = 0;
    std::int32_t data = 0;
};

[[nodiscard]] std::optional<BedrockEntityEventIn> readEntityEvent(io::ByteBuffer& buffer);

/// parity: `BookEditPacket`.
struct BedrockBookEdit {
    enum class Action : std::uint8_t { ReplacePage = 0, AddPage, DeletePage, SwapPages, Sign };

    Action action = Action::ReplacePage;
    std::int32_t slot = 0;
    std::int32_t page = 0;
    std::int32_t otherPage = 0;
    std::string text;
    std::string title;
    std::string author;
};

[[nodiscard]] std::optional<BedrockBookEdit> readBookEdit(io::ByteBuffer& buffer);

/// What comes back is the text the server allows.
[[nodiscard]] std::optional<std::string> readFilterText(io::ByteBuffer& buffer);

void writeFilterText(io::ByteBuffer& buffer, std::string_view text);

struct BedrockCommandBlock {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// Nought impulse, one repeating, two chain, which is that edition's own order.
    std::int32_t mode = 0;
    bool needsRedstone = false;
    bool conditional = false;
    std::string command;
    bool trackOutput = false;
    std::int32_t tickDelay = 0;
};

[[nodiscard]] std::optional<BedrockCommandBlock> readCommandBlockUpdate(io::ByteBuffer& buffer);

struct BedrockCrafterSlot {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t slot = 0;
    bool disabled = false;
};

[[nodiscard]] std::optional<BedrockCrafterSlot> readCrafterSlotToggle(io::ByteBuffer& buffer);

struct BedrockEmote {
    std::uint64_t runtimeEntityId = 0;
    std::string emoteId;
    std::int32_t duration = 0;
    std::uint8_t flags = 0;
};

[[nodiscard]] std::optional<BedrockEmote> readEmote(io::ByteBuffer& buffer, int protocol);

void writeEmote(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, std::string_view emoteId, std::int32_t duration,
                std::uint8_t flags, int protocol);

void writeOpenSign(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, bool front);

enum class BedrockRide : std::int32_t {
    Remove = 0,
    /// The one steering, which for a boat or a horse is the first passenger.
    Rider = 1,
    Passenger = 2,
};

void writeSetEntityLink(io::ByteBuffer& buffer, std::int64_t vehicleId, std::int64_t riderId, BedrockRide how,
                        bool immediate);

void writeSetDifficulty(io::ByteBuffer& buffer, std::int32_t difficulty);

struct BedrockEntityUse {
    /// Two is an interact that names where it was touched, which only a few mobs read.
    std::int32_t action = 0;
    /// The entity as this client knows it, which is one more than the server's own number.
    std::uint64_t runtimeEntityId = 0;
    std::int32_t hotbarSlot = 0;
};

/// A Bedrock client eats on its own clock and says so here, which is the only word the server gets that it is done.
struct BedrockItemRelease {
    std::int32_t action = 0;
    std::int32_t hotbarSlot = 0;
};

struct BedrockTransaction {
    std::optional<BedrockEntityUse> entityUse;
    std::optional<BedrockItemRelease> itemRelease;
};

[[nodiscard]] BedrockTransaction readTransaction(io::ByteBuffer& buffer, int protocol);

/// What a client did to something alive, or to its own screen.
struct BedrockInteract {
    std::int32_t action = 0;
    std::uint64_t runtimeEntityId = 0;
};

[[nodiscard]] std::optional<BedrockInteract> readInteract(io::ByteBuffer& buffer);

[[nodiscard]] std::optional<std::int32_t> readHeldSlot(io::ByteBuffer& buffer, int protocol);

[[nodiscard]] constexpr std::uint64_t flagBit(const EntityFlag flag) {
    const auto at = static_cast<std::uint32_t>(flag);
    return at < 64U ? 1ULL << at : 0ULL;
}

[[nodiscard]] constexpr std::uint64_t flagBit2(const EntityFlag flag) {
    const auto at = static_cast<std::uint32_t>(flag);
    return at >= 64U && at < 128U ? 1ULL << (at - 64U) : 0ULL;
}

void writeMetadataLong(io::ByteBuffer& buffer, EntityDataId id, std::int64_t value, int protocol);

void writeMetadataShort(io::ByteBuffer& buffer, EntityDataId id, std::int16_t value, int protocol);

void writeMetadataFloat(io::ByteBuffer& buffer, EntityDataId id, float value, int protocol);

void writeMetadataString(io::ByteBuffer& buffer, EntityDataId id, std::string_view value, int protocol);

void writeSelfMetadata(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, std::int16_t airSupply,
                       std::int16_t maxAirSupply, std::uint64_t flags, std::uint64_t moreFlags, int protocol);

struct SkinInfo {
    std::string skinId;
    std::string resourcePatch = R"({"geometry":{"default":"geometry.humanoid.custom"}})";
    std::uint32_t width = 64;
    std::uint32_t height = 64;
    std::vector<std::uint8_t> image;
};

/// One colour over the whole skin, picked from the name so two players are not the same colour.
[[nodiscard]] SkinInfo flatSkin(std::string_view name);

/// A line of the list a client draws when it is asked who is here.
struct PlayerListEntry {
    /// Bedrock writes a uuid as two little-endian longs, most significant first.
    std::uint64_t uuidHigh = 0;
    std::uint64_t uuidLow = 0;
    std::int64_t entityId = 0;
    std::string name;
    std::string xuid;
    SkinInfo skin;
};

void writePlayerListAdd(io::ByteBuffer& buffer, std::span<const PlayerListEntry> entries, int protocol);

void writePlayerListRemove(io::ByteBuffer& buffer, std::span<const std::pair<std::uint64_t, std::uint64_t>> uuids,
                           int protocol);

struct AddPlayerInfo {
    std::uint64_t uuidHigh = 0;
    std::uint64_t uuidLow = 0;
    std::int64_t entityId = 0;
    std::uint64_t runtimeEntityId = 0;
    std::string name;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::int32_t gameType = 0;
};

void writeAddPlayer(io::ByteBuffer& buffer, const AddPlayerInfo& info, int protocol);

/// parity: that edition keeps.
void writeNameTag(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, std::string_view nameTag, bool always,
                  int protocol);

void writeMoveEntity(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, float x, float y, float z, float yaw,
                     float pitch, bool onGround);

void writeRemoveEntity(io::ByteBuffer& buffer, std::int64_t entityId);

enum class BedrockDimension : std::int32_t {
    Overworld = 0,
    Nether = 1,
    End = 2,
};

void writeChangeDimension(io::ByteBuffer& buffer, BedrockDimension dimension, float x, float y, float z, bool respawn);

/// parity: `SetSpawnPositionPacket`.
void writeSetSpawnPosition(io::ByteBuffer& buffer, BedrockDimension dimension, std::int32_t x, std::int32_t y,
                           std::int32_t z);

void writeSpawnParticle(io::ByteBuffer& buffer, BedrockDimension dimension, std::string_view particle, float x, float y,
                        float z);

enum class BedrockEntityEvent : std::uint8_t {
    Hurt = 2,
    Death = 3,
    /// What a client waits to hear back before it takes the death screen down.
    Respawn = 18,
    /// The one a client sends rather than is told: it waits to hear its own eating back before it makes the noise.
    EatingItem = 57,
};

/// One thing happening to one entity that the client draws itself: the flash of a hit, the fall of a death.
void writeEntityEvent(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, BedrockEntityEvent event,
                      std::int32_t data, int protocol);

void writeSetEntityMotion(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, float x, float y, float z);

struct BedrockItemStack {
    std::int32_t runtimeId = 0;
    std::int32_t count = 0;
    std::int32_t data = 0;
    std::int32_t blockRuntimeId = 0;
    std::optional<std::int64_t> mapId;
};

struct BedrockIngredient {
    std::string_view name;
    bool tag = false;
    std::int32_t aux = 0;
    std::int32_t count = 1;
};

struct BedrockRecipe {
    std::string id;
    std::string_view tag;
    bool shaped = false;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<BedrockIngredient> ingredients;
    BedrockItemStack result;
    /// Its own name again, as the number the client sends back instead of the string.
    std::int32_t netId = 1;
};

struct BedrockTradeItem {
    std::string_view name;
    std::int32_t count = 0;
    std::int32_t damage = 0;
};

struct BedrockTrade {
    BedrockTradeItem costA;
    BedrockTradeItem costB;
    BedrockTradeItem sell;
    /// What the two costs ask for before demand and a buyer's standing move them.
    std::int32_t baseCountA = 0;
    std::int32_t baseCountB = 0;
    std::int32_t maxUses = 12;
    std::int32_t uses = 0;
    std::int32_t experience = 0;
    float priceMultiplier = 0.05F;
    std::int32_t demand = 0;
    std::int32_t tier = 0;
};

void writeUpdateTrade(io::ByteBuffer& buffer, std::int32_t containerId, std::int32_t tier, std::int64_t traderId,
                      std::int64_t playerId, std::string_view displayName, std::span<const BedrockTrade> trades);

struct BedrockEnchant {
    std::int32_t type = 0;
    std::int32_t level = 1;
};

struct BedrockEnchantOption {
    std::int32_t cost = 0;
    std::int32_t primarySlot = 0;
    std::vector<BedrockEnchant> enchants;
    /// The made-up name the client draws in the fake galactic alphabet.
    std::string name;
    std::int32_t netId = 0;
};

/// Without it that edition's screen has three empty rows: it draws them from this packet and from nothing else.
void writePlayerEnchantOptions(io::ByteBuffer& buffer, std::span<const BedrockEnchantOption> options, int protocol);

/// Which number that edition knows an enchantment by, or nothing for one it has never had.
[[nodiscard]] std::optional<std::int32_t> bedrockEnchantment(std::string_view name);

/// Every recipe a client is told about at login.
inline constexpr std::int32_t kFirstCartographyRecipeNetId = 1 << 20;
inline constexpr std::int32_t kCartographyRecipeCount = 4;

/// Every recipe in `recipes`, plus the cartography table's four, which are the same on every server.
void writeCraftingData(io::ByteBuffer& buffer, std::span<const BedrockRecipe> recipes, int protocol);

enum class BedrockContainerSlot : std::uint8_t {
    AnvilInput = 0,
    AnvilMaterial = 1,
    AnvilResult = 2,
    SmithingInput = 3,
    SmithingMaterial = 4,
    SmithingResult = 5,
    Armour = 6,
    LevelEntity = 7,
    BeaconPayment = 8,
    LoomInput = 41,
    LoomDye = 42,
    LoomMaterial = 43,
    LoomResult = 44,
    /// The 36 a player carries, hotbar first.
    HotbarAndInventory = 12,
    CraftingInput = 13,
    CraftingOutput = 14,
    FurnaceFuel = 24,
    FurnaceIngredient = 25,
    FurnaceResult = 26,
    EnchantingInput = 22,
    EnchantingMaterial = 23,
    Hotbar = 28,
    Inventory = 29,
    Offhand = 34,
    TradeIngredientA = 47,
    TradeIngredientB = 48,
    TradeResult = 49,
    GrindstoneInput = 50,
    GrindstoneAdditional = 51,
    GrindstoneResult = 52,
    StonecutterInput = 53,
    StonecutterResult = 54,
    CartographyInput = 55,
    CartographyAdditional = 56,
    CartographyResult = 57,
    Cursor = 59,
    CreatedOutput = 60,
    SmithingTemplate = 61,
};

[[nodiscard]] std::string_view bannerPatternOf(std::string_view code);

void writeItemComponent(io::ByteBuffer& buffer, int protocol);

void writeAddItemEntity(io::ByteBuffer& buffer, std::int64_t entityId, const BedrockItemStack& item, float x, float y,
                        float z, float motionX, float motionY, float motionZ, int protocol);

/// A name this client has no entity for is one it will not draw, which is why the caller translates first.
void writeAddEntity(io::ByteBuffer& buffer, std::int64_t entityId, std::string_view identifier, float x, float y,
                    float z, float yaw, float pitch, float width, float height, std::uint64_t flags, int protocol);

/// parity: nothing in the jar.
void writeAddBorderCollider(io::ByteBuffer& buffer, std::int64_t entityId, float x, float y, float z, int protocol);

/// What an entity is doing now, when only that changed: the same flags the spawn carried.
void writeEntityFlags(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, std::uint64_t flags, int protocol);

struct BedrockLook {
    /// Nought is the first of them, which is also what nothing means.
    std::int32_t variant = 0;
    /// A second kind alongside the first, for the mobs that have two.
    std::int32_t markVariant = 0;
    std::optional<std::int8_t> colour;
    /// A second colour, which only a tropical fish's pattern uses.
    std::optional<std::int8_t> colour2;
    std::optional<float> scale;
    std::optional<std::int32_t> horns;
    std::optional<std::int32_t> strength;
    std::optional<std::int32_t> climate;

    friend bool operator==(const BedrockLook&, const BedrockLook&) = default;
};

void writePlayerHotbar(io::ByteBuffer& buffer, std::int32_t slot);

void writeLevelSoundEvent(io::ByteBuffer& buffer, std::int32_t event, float x, float y, float z, int protocol);

void writeItemFrameBlock(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, bool glowing, int face,
                         int protocol);

void writeItemFrameData(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, bool glowing,
                        std::string_view item, std::int32_t count, std::int32_t rotation);

void writeAddPainting(io::ByteBuffer& buffer, std::int64_t entityId, std::uint64_t runtimeEntityId, float x, float y,
                      float z, std::int32_t direction, std::string_view motive);

/// One kind per call, with its choices in the order their numbers run.
void writeSyncEntityProperty(io::ByteBuffer& buffer, std::string_view entityType, std::string_view property,
                             std::span<const std::string_view> choices);

void writeEntityIntProperty(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, std::uint32_t index,
                            std::int32_t value);

[[nodiscard]] bool hasAnything(const BedrockLook& look);

void writeEntityLook(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, const BedrockLook& look, int protocol);

void writeSleeping(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, std::uint64_t moreFlags, std::int32_t bedX,
                   std::int32_t bedY, std::int32_t bedZ, int protocol);

/// The item entity is taken away by whoever is tracking it; this is only the little arc it makes on the way.
void writeTakeItemEntity(io::ByteBuffer& buffer, std::uint64_t itemRuntimeId, std::uint64_t collectorRuntimeId);

void writeMobEquipment(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, const BedrockItemStack& item,
                       std::int32_t hotbarSlot, int protocol);

void writeMobArmorEquipment(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, const BedrockItemStack& head,
                            const BedrockItemStack& chest, const BedrockItemStack& legs, const BedrockItemStack& feet,
                            const BedrockItemStack& body, int protocol);

void writeInventoryContent(io::ByteBuffer& buffer, std::uint32_t containerId, std::span<const BedrockItemStack> items,
                           BedrockContainerSlot slot, int protocol);

enum class BedrockEffectEvent : std::uint8_t {
    None = 0,
    Add = 1,
    Modify = 2,
    Remove = 3,
};

void writeMobEffect(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, BedrockEffectEvent event,
                    std::int32_t effectId, std::int32_t amplifier, bool particles, std::int32_t durationTicks,
                    bool ambient);

void writeDeathInfo(io::ByteBuffer& buffer, std::string_view cause);

enum class RespawnState : std::uint8_t {
    Searching = 0,
    ServerReady = 1,
    ClientReady = 2,
};

void writeRespawn(io::ByteBuffer& buffer, float x, float y, float z, RespawnState state, std::uint64_t runtimeEntityId);

/// The few things a client still says through the old action packet, now that its movement carries the rest.
enum class BedrockPlayerAction : std::uint8_t {
    StopSleep = 6,
    Respawn = 7,
    DimensionChanged = 14,
};

[[nodiscard]] std::optional<BedrockPlayerAction> readPlayerAction(io::ByteBuffer& buffer);

/// What a client answers with when somebody presses the button.
[[nodiscard]] std::optional<RespawnState> readRespawn(io::ByteBuffer& buffer);

struct CommandInfo {
    std::string name;
    std::string description;
};

/// The commands a client may use.
void writeAvailableCommands(io::ByteBuffer& buffer, std::span<const CommandInfo> commands);

struct AttributeValue {
    std::string name;
    float minimum = 0.0F;
    float maximum = 0.0F;
    float value = 0.0F;
    float defaultValue = 0.0F;
};

void writeUpdateAttributes(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId,
                           std::span<const AttributeValue> attributes, std::uint64_t tick);

enum class LevelEventType : std::int32_t {
    StartRaining = 3001,
    StartThunderstorm = 3002,
    StopRaining = 3003,
    StopThunderstorm = 3004,
    /// Bedrock draws no cracks of its own on a server that never says this, and falls back to a pace of its own.
    ParticleExplosion = 2025,
    /// parity: `PARTICLE_DENY_BLOCK`.
    ParticleDenyBlock = 2008,
    BlockStartBreak = 3600,
    BlockStopBreak = 3601,
    BlockUpdateBreak = 3602,
};

inline constexpr std::int32_t kBreakProgressScale = 65535;

inline constexpr std::int32_t kWeatherStrengthScale = 65535;

void writeLevelEvent(io::ByteBuffer& buffer, LevelEventType type, float x, float y, float z, std::int32_t data);

inline constexpr int kLocatorBarProtocol = 944;

inline constexpr int kWaypointTextureProtocol = 975;

/// parity: `LocatorBarPacket.Action`.
enum class BedrockWaypointAction : std::uint8_t {
    None,
    Add,
    Remove,
    Update,
};

struct BedrockWaypoint {
    std::uint64_t groupHigh = 0;
    std::uint64_t groupLow = 0;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    BedrockDimension dimension = BedrockDimension::Overworld;
    std::int32_t colour = 0;
    float distance = 0.0F;
    /// The entity the dot follows, when the client has one: it then moves the dot itself between updates.
    std::optional<std::int64_t> entityId;
};

void writeLocatorBar(io::ByteBuffer& buffer, const BedrockWaypoint& waypoint, BedrockWaypointAction action,
                     int protocol);

/// Where one entity is drawn on the bar of a client too old for the packet above.
void writePlayerLocation(io::ByteBuffer& buffer, std::int64_t entityId, std::optional<std::array<float, 3>> position,
                         int protocol);

/// One rule on its own, for the few a server turns on and off while somebody is playing.
void writeGameRuleFlag(io::ByteBuffer& buffer, std::string_view name, bool value, int protocol);

struct BedrockPacketViolation {
    std::int32_t type = 0;
    std::int32_t severity = 0;

    std::int32_t packetId = 0;
    std::string context;
};

[[nodiscard]] std::optional<BedrockPacketViolation> readPacketViolationWarning(io::ByteBuffer& buffer);

enum class BedrockCameraPreset : std::int32_t {
    FirstPerson = 0,
    Free = 1,
    ThirdPerson = 2,
    ThirdPersonFront = 3,
};

/// Names those four for the client.
void writeCameraPresets(io::ByteBuffer& buffer, int protocol);

/// parity: `CameraInstructionPacket`.
struct BedrockCamera {
    BedrockCameraPreset preset = BedrockCameraPreset::Free;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float pitch = 0.0F;
    float yaw = 0.0F;
    float easeSeconds = 0.0F;
};

void writeCameraSet(io::ByteBuffer& buffer, const BedrockCamera& camera);

void writeCameraClear(io::ByteBuffer& buffer, bool asDefault);

} // namespace basalt::wire::bedrock
