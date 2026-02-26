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

/// Every shape below that a protocol changed, changed at the same one: 2168 rewrote the skin, the
/// metadata, the player list, the chunk, the two resource pack packets and two fields of
/// `start_game`, and everything older in the window reads the shape underneath. A writer that
/// takes a protocol takes it for that reason and no other.
[[nodiscard]] constexpr bool writesNativeShapes(const int protocol) { return protocol >= kNativeBedrockProtocol; }

/// The one shape in the window that also changed somewhere other than 2168: `start_game` grew three
/// fields at 1001 (two about editor worlds and one asking whether chat is logged), of which 2168
/// kept the first two and dropped the third. The three protocols below 1001 have none of them.
inline constexpr int kStartGameGrewProtocol = 1001;

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
struct PlayerAuthInput {
    float x = 0.0F;
    /// As the client writes it, which is its eyes and not its feet. kPlayerEyeHeight is the
    /// difference.
    float y = 0.0F;
    float z = 0.0F;
    float pitch = 0.0F;
    float yaw = 0.0F;
    float headYaw = 0.0F;
};

/// How far above its feet a Bedrock client reports a player to be. Java measures from the feet, so
/// every position that crosses between them moves by this.
inline constexpr float kPlayerEyeHeight = 1.62001F;

/// Reads only as far as the position and the rotation: what follows is input state, item use and
/// stack requests, none of which this server acts on yet.
[[nodiscard]] std::optional<PlayerAuthInput> readPlayerAuthInput(io::ByteBuffer& buffer);

/// How far the client should ask the server for, and how far it draws.
void writeChunkRadiusUpdated(io::ByteBuffer& buffer, std::int32_t radius);

[[nodiscard]] std::optional<std::int32_t> readRequestChunkRadius(io::ByteBuffer& buffer);

/// The centre of what the server is willing to send, which is what stops a client waiting for
/// chunks that are not coming.
void writeNetworkChunkPublisherUpdate(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::uint32_t radius);

/// An empty chunk column: every subchunk absent, biomes flat, no block entities. It is what a
/// client needs to stop waiting for a chunk it was told to expect.
void writeEmptyLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t subChunkCount,
                          std::int32_t dimension, int protocol);

/// Which bit of the flags field a flag is.
[[nodiscard]] constexpr std::uint64_t flagBit(const EntityFlag flag) { return 1ULL << static_cast<std::uint8_t>(flag); }

/// One field of entity metadata. Since 2168 the format goes out twice, as a varint and then as a
/// byte, and the client refuses a packet where the two disagree.
void writeMetadataLong(io::ByteBuffer& buffer, EntityDataId id, std::int64_t value, int protocol);

void writeMetadataFloat(io::ByteBuffer& buffer, EntityDataId id, float value, int protocol);

void writeMetadataString(io::ByteBuffer& buffer, EntityDataId id, std::string_view value, int protocol);

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
