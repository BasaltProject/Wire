#pragma once

#include <basalt/wire/bedrock/BedrockEntityData.hpp>
#include <basalt/wire/bedrock/BedrockPacketIds.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::bedrock {

/// Where almost every shape in this window last changed: 2168 rewrote the skin, the metadata, the
/// player list, the chunk and two fields of `start_game`, and everything older reads the shape
/// underneath. Its own number rather than "the newest this build speaks", because 2169 changed
/// nothing and tying the two together quietly gave 2168 the old shapes.
inline constexpr int kShapesChangedProtocol = 2168;

[[nodiscard]] constexpr bool writesNativeShapes(const int protocol) { return protocol >= kShapesChangedProtocol; }

/// The one shape in the window that also changed somewhere other than 2168: `start_game` grew three
/// fields at 1001 (two about editor worlds and one asking whether chat is logged), of which 2168
/// kept the first two and dropped the third. The three protocols below 1001 have none of them.
inline constexpr int kStartGameGrewProtocol = 1001;

/// Where a played sound gained the handle of one the server holds itself, which this one never has.
inline constexpr int kServerSoundHandleProtocol = 975;

/// Where the flags of `player_auth_input` stopped being a run of bits and became a list of the
/// ones that are on.
inline constexpr int kInputFlagListProtocol = 2168;

/// Where an entity event gained a trailing optional position, which only a fire event fills.
inline constexpr int kEntityEventGrewProtocol = 975;

/// Where an item stack in a window changed from the old shape to the descriptor: the id goes from
/// a varint to a short and an air stack stops being a single zero byte.
inline constexpr int kItemDescriptorProtocol = 1001;

/// Where a stack named outside a window (the hand of a player being drawn, the hand a click was
/// made with) took the same descriptor shape. It is not the same protocol as the window's own: for
/// three versions the two shapes were different, which is the whole reason both exist here.
inline constexpr int kHeldItemDescriptorProtocol = 2168;

/// Where a click gained a trailing byte saying how far into its cooldown the client thinks the
/// item is.
inline constexpr int kItemCooldownProtocol = 944;

/// What a client asks for before anything else, and the only packet either side sends in the
/// clear and uncompressed.
struct RequestNetworkSettings {
    int protocolVersion = 0;
};

[[nodiscard]] std::optional<RequestNetworkSettings> readRequestNetworkSettings(io::ByteBuffer& buffer);

void writeNetworkSettings(io::ByteBuffer& buffer, std::uint16_t compressionThreshold, std::uint16_t algorithm);

/// The two halves of a login: what the client claims to be, and the key it wants to talk with.
struct LoginRequest {
    int protocolVersion = 0;
    std::string name;
    std::string xuid;
    std::string identityUuid;
    /// The client's public key, as the DER a JWT header carries.
    std::vector<std::uint8_t> identityPublicKey;
    /// True when Mojang signed the chain, rather than the client signing itself.
    bool authenticated = false;
};

[[nodiscard]] std::optional<LoginRequest> readLogin(io::ByteBuffer& buffer);

/// What a client answers the resource pack question with.
enum class ResourcePackResponse : std::uint8_t {
    Refused = 1,
    SendPacks = 2,
    HaveAllPacks = 3,
    Completed = 4,
};

[[nodiscard]] std::optional<ResourcePackResponse> readResourcePackResponse(io::ByteBuffer& buffer, int protocol);

/// The states a client is told it is in. The numbers are the protocol's.
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

/// Everything the client needs before it will draw a world. Most of it is the server describing a
/// world nobody configured, so the defaults here are the answer for an ordinary server and only a
/// handful of fields are ever set by a caller.
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

/// What a line of text is for. The number is the protocol's own order, and it decides what the
/// client does with the line: a chat message gets a name in front, a raw one is printed as it is.
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

/// A line for the chat window. The source is only written for the types that carry a name.
void writeText(io::ByteBuffer& buffer, TextType type, std::string_view source, std::string_view message,
               std::string_view xuid = {});

/// What a client typed, or whatever else it sent as text.
struct IncomingText {
    TextType type = TextType::Raw;
    std::string source;
    std::string message;
};

[[nodiscard]] std::optional<IncomingText> readText(io::ByteBuffer& buffer);

/// Moves a player where the server says, rather than where the client thinks it is. The position
/// is the eyes, like everywhere else in this protocol.
void writeTeleport(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, float x, float y, float z, float yaw,
                   float pitch, std::uint64_t tick, int protocol);

/// What game a player is playing. The numbers are Bedrock's own: survival, creative, adventure,
/// then the two that watch.
void writeSetPlayerGameType(io::ByteBuffer& buffer, std::int32_t gameType);

/// What a title is for. The number is the protocol's own order.
enum class TitleType : std::int32_t {
    Clear = 0,
    Reset = 1,
    Title = 2,
    Subtitle = 3,
    ActionBar = 4,
    Times = 5,
};

/// parity: how long a line over the hotbar lasts in vanilla, `Gui.OVERLAY_MESSAGE_TIME`. Bedrock
/// draws one for as long as it is told, so it is told this.
inline constexpr std::int32_t kActionBarTicks = 60;

void writeSetTitle(io::ByteBuffer& buffer, TitleType type, std::string_view text, std::int32_t fadeIn,
                   std::int32_t stay, std::int32_t fadeOut);

/// What time it is where this player is. Bedrock runs its own clock from here, like Java does.
void writeSetTime(io::ByteBuffer& buffer, std::int32_t time);

/// A command a client typed. Only the line itself is read: what follows says which of the client's
/// own consoles it came from, and a server that runs it for the player answers the same way to all
/// of them.
[[nodiscard]] std::optional<std::string> readCommandRequest(io::ByteBuffer& buffer);

/// A form, which is what a Bedrock player gets instead of a window of items. The json is the form
/// itself, in the shape the client's own UI reads.
void writeModalFormRequest(io::ByteBuffer& buffer, std::uint32_t formId, std::string_view json);

/// Nothing to draw any more, for a form the server takes back.
void writeCloseForm(io::ByteBuffer& buffer);

/// What a client answers a form with: the id it was given, and either what was chosen or nothing
/// at all when it was dismissed.
struct ModalFormResponse {
    std::uint32_t formId = 0;
    std::optional<std::string> data;
};

[[nodiscard]] std::optional<ModalFormResponse> readModalFormResponse(io::ByteBuffer& buffer);

/// Builds the json of a simple form: a title, a line of text, and a button per entry.
[[nodiscard]] std::string simpleFormJson(std::string_view title, std::string_view content,
                                         std::span<const std::string> buttons);

/// Where a client says it is, which in Bedrock is the only movement packet: the client moves itself
/// and tells the server afterwards.
/// What a player did to a block this tick, in that edition's own numbering. Only the five that
/// carry a position are worth naming here; the rest arrive as their number and are ignored.
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

/// A click with something in hand. Placing a block is this with `action` naming a click on a block
/// and the hand holding one.
struct BedrockItemUse {
    /// 0 clicks a block, 1 clicks the air, 2 breaks one.
    std::int32_t action = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t face = 0;
    std::int32_t hotbarSlot = 0;
    /// The item the client believes it is holding, by the id this server gave it.
    std::int32_t itemRuntimeId = 0;
    /// Where on the face it was clicked, from the block's own corner.
    float clickX = 0.0F;
    float clickY = 0.0F;
    float clickZ = 0.0F;
};

/// The containers a stack request can name that this server models. The numbers are that edition's
/// own, and they are the ones every protocol in this window uses: the table shifted last at 560.
enum class BedrockRequestContainer : std::int32_t {
    Armour = 6,
    /// What a block being looked into holds: a chest's own rows, a furnace seen as one list.
    LevelEntity = 7,
    /// The 36 a player carries, hotbar first.
    HotbarAndInventory = 12,
    CraftingInput = 13,
    CraftingOutput = 14,
    FurnaceFuel = 24,
    FurnaceIngredient = 25,
    FurnaceResult = 26,
    Hotbar = 28,
    Inventory = 29,
    Offhand = 34,
    /// What is being dragged. Bedrock keeps it in a container of its own rather than outside them.
    Cursor = 59,
    CreatedOutput = 60,
};

/// One end of a move: which window, which slot of it, and what the client believes is there.
struct BedrockStackSlot {
    std::int32_t container = 0;
    std::int32_t slot = 0;
    /// The number the client thinks that stack has. This server counts no stacks, so it is echoed
    /// back rather than checked.
    std::int32_t stackNetworkId = 0;
};

/// What one line of a stack request asks for. The numbering differs between protocols, so it is
/// resolved here and the caller never sees a raw id.
enum class BedrockStackActionType : std::int32_t {
    /// Read but not acted on: everything from crafting to a beacon's payment.
    Other = 0,
    Take,
    Place,
    Swap,
    Drop,
    Destroy,
    Consume,
    Create,
    CraftResults,
};

struct BedrockStackAction {
    BedrockStackActionType type = BedrockStackActionType::Other;
    /// How many the line moves, for the five that move any.
    std::int32_t count = 0;
    BedrockStackSlot source;
    BedrockStackSlot destination;
};

/// A client asking for stacks to be moved. It arrives inside a tick's own packet, and the answer
/// carries its number back so the client knows which of its guesses held.
struct BedrockStackRequest {
    std::int32_t requestId = 0;
    std::vector<BedrockStackAction> actions;
};

struct PlayerAuthInput {
    float x = 0.0F;
    /// As the client writes it, which is its eyes and not its feet. kPlayerEyeHeight is the
    /// difference.
    float y = 0.0F;
    float z = 0.0F;
    float pitch = 0.0F;
    float yaw = 0.0F;
    float headYaw = 0.0F;

    /// What they did to a block, in the order they did it.
    std::vector<BedrockBlockAction> blockActions;

    /// What they did with what they hold, when they did anything.
    std::optional<BedrockItemUse> itemUse;

    /// What they asked to be moved between windows, when they asked for anything.
    std::optional<BedrockStackRequest> stackRequest;
};

/// How far above its feet a Bedrock client reports a player to be. Java measures from the feet, so
/// every position that crosses between them moves by this.
inline constexpr float kPlayerEyeHeight = 1.62001F;

/// Reads the whole packet: where the client is, what it did to a block, what it did with what it
/// holds, and what it asked to be moved between its windows. What is left is input state, which
/// this server works out for itself.
[[nodiscard]] std::optional<PlayerAuthInput> readPlayerAuthInput(io::ByteBuffer& buffer, int protocol);

/// What one slot of a window holds after a request was answered. The client draws what this says
/// and drops its own guess.
struct BedrockResponseSlot {
    std::int32_t slot = 0;
    std::int32_t count = 0;
    std::int32_t stackNetworkId = 0;
};

/// One window of an answer, and what changed in it.
struct BedrockResponseContainer {
    std::int32_t container = 0;
    std::vector<BedrockResponseSlot> slots;
};

/// The answer to one stack request. An empty `containers` with `accepted` still tells the client
/// its guess stood; `accepted` false makes it put everything back.
void writeItemStackResponse(io::ByteBuffer& buffer, std::int32_t requestId, bool accepted,
                            std::span<const BedrockResponseContainer> containers, int protocol);

/// How far the client should ask the server for, and how far it draws.
void writeChunkRadiusUpdated(io::ByteBuffer& buffer, std::int32_t radius);

[[nodiscard]] std::optional<std::int32_t> readRequestChunkRadius(io::ByteBuffer& buffer);

/// The centre of what the server is willing to send, which is what stops a client waiting for
/// chunks that are not coming.
void writeNetworkChunkPublisherUpdate(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::uint32_t radius);

/// How many blocks a subchunk holds, which is sixteen cubed on both editions.
inline constexpr std::size_t kSubChunkBlocks = 4096;

/// One subchunk's worth of blocks, as Java numbers its states: 4096 of them, and the caller says
/// which corner they start from. The order is Java's own, y innermost.
using JavaSubChunk = std::span<const std::int32_t>;

/// Writes one subchunk in the shape a Bedrock client reads: its own palette, and the blocks as
/// indices into it packed however few bits they need.
///
/// The index of a block inside a subchunk is `(x << 8) | (z << 4) | y` over there, which is the
/// opposite nesting to Java's, so this is also where the two orders meet.
void writeSubChunk(io::ByteBuffer& buffer, JavaSubChunk blocks, std::int8_t subChunkIndex, int protocol);

/// The four things a client is told between the world's description and being let in; without them
/// it drops the connection without saying why. Almost empty on purpose, since this server overrides
/// neither the biomes nor the creative menu. The entity identifiers are the exception: a client
/// told none refuses the ones it is shown.
void writeAvailableEntityIdentifiers(io::ByteBuffer& buffer);

void writeBiomeDefinitionList(io::ByteBuffer& buffer);

void writeCreativeContent(io::ByteBuffer& buffer);

void writeSetCommandsEnabled(io::ByteBuffer& buffer, bool enabled);

/// A sound, at a place. The name is Bedrock's own, which `bedrockSoundName` translates from Java's.
///
/// The position travels as eighths of a block, which is the one place in this protocol where a
/// coordinate is not a coordinate.
void writePlaySound(io::ByteBuffer& buffer, std::string_view sound, double x, double y, double z, float volume,
                    float pitch, int protocol);

/// One block that changed, for a client that already has the column it is in.
///
/// The flags say what the client does about it: telling the neighbours is what makes a fence
/// rejoin and water flow on, and the network bit is what says the change came from here rather
/// than from the player's own hand.
void writeUpdateBlock(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t javaState,
                      int protocol);

/// A chunk column with blocks in it. `subChunks` is what `writeSubChunk` already wrote, one after
/// another, and `count` how many of them: the client reads them from the bottom of the world up.
void writeLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t count,
                     std::span<const std::uint8_t> subChunks, std::uint32_t biomeSections, std::int32_t dimension,
                     int protocol);

/// An empty chunk column: every subchunk absent, biomes flat, no block entities. It is what a
/// client needs to stop waiting for a chunk it was told to expect.
void writeEmptyLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t subChunkCount,
                          std::int32_t dimension, int protocol);

/// Which bit of the flags field a flag is.
[[nodiscard]] constexpr std::uint64_t flagBit(const EntityFlag flag) { return 1ULL << static_cast<std::uint8_t>(flag); }

/// One field of entity metadata. Since 2168 the format goes out twice, as a varint and then as a
/// byte, and the client refuses a packet where the two disagree.
void writeMetadataLong(io::ByteBuffer& buffer, EntityDataId id, std::int64_t value, int protocol);

void writeMetadataShort(io::ByteBuffer& buffer, EntityDataId id, std::int16_t value, int protocol);

void writeMetadataFloat(io::ByteBuffer& buffer, EntityDataId id, float value, int protocol);

void writeMetadataString(io::ByteBuffer& buffer, EntityDataId id, std::string_view value, int protocol);

/// How much air is left and which of the shared flags are on. The two travel together because
/// they are the two halves of what the HUD draws while somebody is under water or on fire.
void writeSubmersion(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, std::int16_t airSupply, std::uint64_t flags,
                     int protocol);

/// What a player looks like. Bedrock will not draw somebody it has no skin for, and a server that
/// redistributed Mojang's would be shipping their art, so this is a plain image made here.
struct SkinInfo {
    std::string skinId;
    /// The json that names the geometry the client already has built in. Empty geometry data is
    /// how a server says "use that one", which is what every plain humanoid does.
    std::string resourcePatch = R"({"geometry":{"default":"geometry.humanoid.custom"}})";
    std::uint32_t width = 64;
    std::uint32_t height = 64;
    /// Rows of RGBA, top to bottom. width * height * 4 bytes and no header: Bedrock sends raw
    /// pixels, not a png.
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

/// The other half of the same packet: who is no longer here, by uuid.
void writePlayerListRemove(io::ByteBuffer& buffer, std::span<const std::pair<std::uint64_t, std::uint64_t>> uuids,
                           int protocol);

/// Somebody else, as an entity in the world. The position is the eyes, like everywhere else here.
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

/// Where somebody else is now. The rotation goes as a byte a turn, which is all this packet has
/// room for and is what the client draws from.
void writeMoveEntity(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, float x, float y, float z, float yaw,
                     float pitch, bool onGround);

/// Somebody else is gone. Bedrock removes by the id that never changes, not the runtime one.
void writeRemoveEntity(io::ByteBuffer& buffer, std::int64_t entityId);

/// Which of Bedrock's three dimensions a world is drawn as. The numbers are the protocol's own.
enum class BedrockDimension : std::int32_t {
    Overworld = 0,
    Nether = 1,
    End = 2,
};

/// Moves a client to another dimension: the sky, the fog and the chunk cache all come from this.
/// The client answers when it has finished, and the chunks go out after that.
void writeChangeDimension(io::ByteBuffer& buffer, BedrockDimension dimension, float x, float y, float z, bool respawn);

/// A particle, at a place, under the name that edition knows it by. The dimension goes first
/// because this packet can be sent to a world the receiver is not in.
void writeSpawnParticle(io::ByteBuffer& buffer, BedrockDimension dimension, std::string_view particle, float x, float y,
                        float z);

/// The handful of entity events this server has anything to say with. The numbers have not moved
/// once in the window: every version since 291 only adds to the end of the table.
enum class BedrockEntityEvent : std::uint8_t {
    Hurt = 2,
    Death = 3,
};

/// One thing happening to one entity that the client draws itself: the flash of a hit, the fall of
/// a death. Grew a trailing optional position at 975, which nothing here fills.
void writeEntityEvent(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, BedrockEntityEvent event,
                      std::int32_t data, int protocol);

/// Throws an entity, in blocks per tick. What a hit does to whoever took it.
void writeSetEntityMotion(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, float x, float y, float z);

/// One stack as a window carries it. An id of zero is air and says the rest of itself by saying
/// nothing.
struct BedrockItemStack {
    std::int32_t runtimeId = 0;
    std::int32_t count = 0;
    std::int32_t data = 0;
    /// Which block it places, for an item that places one. Zero for everything else.
    std::int32_t blockRuntimeId = 0;
};

/// Which part of a window a run of slots belongs to. The numbers have not moved in this window.
enum class BedrockContainerSlot : std::uint8_t {
    Armour = 6,
    HotbarAndInventory = 12,
    Offhand = 33,
};

/// The names of every item this server knows, with the id it wants each one called by. Bedrock has
/// let the server declare this since 776, which is why no table of per-version item ids exists.
void writeItemComponent(io::ByteBuffer& buffer);

/// Everything in one window. The slot type names which window it is, because the number alone has
/// not been enough since containers could be dynamic.
void writeInventoryContent(io::ByteBuffer& buffer, std::uint32_t containerId, std::span<const BedrockItemStack> items,
                           BedrockContainerSlot slot, int protocol);

/// What a mob effect packet is saying about the effect it names.
enum class BedrockEffectEvent : std::uint8_t {
    None = 0,
    Add = 1,
    Modify = 2,
    Remove = 3,
};

/// One effect starting, changing or ending on one entity. Bedrock draws its own icon and screen
/// tint off this and off nothing else, so an effect not sent is an effect the player cannot see.
void writeMobEffect(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId, BedrockEffectEvent event,
                    std::int32_t effectId, std::int32_t amplifier, bool particles, std::int32_t durationTicks,
                    bool ambient);

/// The sentence on the screen a dead player is shown. Bedrock draws that screen off the health
/// attribute reaching zero; this only puts the words on it.
void writeDeathInfo(io::ByteBuffer& buffer, std::string_view cause);

/// Where a respawn has got to. The numbers are the protocol's own order: the server is looking for
/// somewhere to put them, the server is ready, and the client is.
enum class RespawnState : std::uint8_t {
    Searching = 0,
    ServerReady = 1,
    ClientReady = 2,
};

void writeRespawn(io::ByteBuffer& buffer, float x, float y, float z, RespawnState state, std::uint64_t runtimeEntityId);

/// What a client answers with when somebody presses the button. Only the state is read: the
/// position it sends is where it thinks it was, and the server decides where it goes.
[[nodiscard]] std::optional<RespawnState> readRespawn(io::ByteBuffer& buffer);

/// A command, as a Bedrock client is told about it. The description is what its own command list
/// draws next to the name.
struct CommandInfo {
    std::string name;
    std::string description;
};

/// The commands a client may use. Bedrock parses what is typed against this list before it sends
/// anything, so a command missing from here cannot be run at all, however well the server handles
/// it. Only the names go out: every command is declared as taking one optional line of text, which
/// is what a server that parses its own arguments wants, and the same shape on all five protocols.
void writeAvailableCommands(io::ByteBuffer& buffer, std::span<const CommandInfo> commands);

/// One of the numbers a client draws its HUD from. Java keeps the bounds to itself and sends only
/// the value; Bedrock is told all of them, so every attribute carries its own range.
struct AttributeValue {
    std::string name;
    float minimum = 0.0F;
    float maximum = 0.0F;
    float value = 0.0F;
    float defaultValue = 0.0F;
};

void writeUpdateAttributes(io::ByteBuffer& buffer, std::uint64_t runtimeEntityId,
                           std::span<const AttributeValue> attributes, std::uint64_t tick);

/// Something that happens to the world rather than to anybody. The numbers are the protocol's own,
/// and the order of these four is its order and not the obvious one.
enum class LevelEventType : std::int32_t {
    StartRaining = 3001,
    StartThunderstorm = 3002,
    StopRaining = 3003,
    StopThunderstorm = 3004,
};

/// How hard it is raining, as this packet counts it: the whole range of an unsigned short.
inline constexpr std::int32_t kWeatherStrengthScale = 65535;

void writeLevelEvent(io::ByteBuffer& buffer, LevelEventType type, float x, float y, float z, std::int32_t data);

} // namespace basalt::wire::bedrock
