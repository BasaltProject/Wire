#include <basalt/wire/bedrock/BedrockBlocks.hpp>
#include <basalt/wire/bedrock/BedrockEntityIdentifiers.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>
#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockSounds.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>
#include <set>
#include <vector>

namespace basalt::wire::bedrock {
namespace {

void writeBool(io::ByteBuffer& buffer, bool value) { buffer.writeByte(value ? 1 : 0); }

/// A block position, which writes its height unsigned. Only this one field does; every other
/// coordinate on the wire is zigzag.
void writeBlockPosition(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z) {
    writeVarInt(buffer, x);
    writeVarUInt(buffer, static_cast<std::uint32_t>(y));
    writeVarInt(buffer, z);
}

/// An empty NBT compound in the shape Bedrock's network NBT uses: the tag, a name of no length,
/// and the end marker.
void writeEmptyCompound(io::ByteBuffer& buffer) {
    buffer.writeByte(0x0a);
    writeVarUInt(buffer, 0);
    buffer.writeByte(0x00);
}

/// The last entry of the chain is the one that speaks for the client: the ones before it are there
/// to prove it.
std::optional<std::string> lastChainEntry(std::string_view certificateJson) {
    const auto document = common::parseJson(certificateJson);
    if (!document) {
        return std::nullopt;
    }
    const auto* object = document->get<common::JsonObject>();
    if (object == nullptr) {
        return std::nullopt;
    }
    const common::JsonValue* chain = object->find("chain");
    if (chain == nullptr) {
        return std::nullopt;
    }
    const auto* entries = chain->get<std::vector<common::JsonValue>>();
    if (entries == nullptr || entries->empty()) {
        return std::nullopt;
    }
    const auto* token = entries->back().get<std::string>();
    if (token == nullptr) {
        return std::nullopt;
    }
    return *token;
}

/// The identity a chain entry carries, which lives one level down in extraData.
void readExtraData(std::string_view payload, LoginRequest& login) {
    const auto document = common::parseJson(payload);
    if (!document) {
        return;
    }
    const auto* object = document->get<common::JsonObject>();
    if (object == nullptr) {
        return;
    }
    if (const common::JsonValue* key = object->find("identityPublicKey"); key != nullptr) {
        if (const auto* text = key->get<std::string>(); text != nullptr) {
            if (const auto decoded = common::base64::decode(*text); decoded) {
                login.identityPublicKey = *decoded;
            }
        }
    }
    const common::JsonValue* extra = object->find("extraData");
    if (extra == nullptr) {
        return;
    }
    const auto* fields = extra->get<common::JsonObject>();
    if (fields == nullptr) {
        return;
    }
    if (const common::JsonValue* value = fields->find("displayName"); value != nullptr) {
        if (const auto* text = value->get<std::string>(); text != nullptr) {
            login.name = *text;
        }
    }
    if (const common::JsonValue* value = fields->find("identity"); value != nullptr) {
        if (const auto* text = value->get<std::string>(); text != nullptr) {
            login.identityUuid = *text;
        }
    }
    if (const common::JsonValue* value = fields->find("XUID"); value != nullptr) {
        if (const auto* text = value->get<std::string>(); text != nullptr) {
            login.xuid = *text;
        }
    }
}

/// The newer login carries claims of its own instead of a chain: the same three facts under
/// shorter names, and no uuid at all, so the xuid is what it is derived from.
void readTokenClaims(std::string_view payload, LoginRequest& login) {
    if (const auto key = jsonString(payload, "cpk"); key) {
        if (const auto decoded = common::base64::decode(*key); decoded) {
            login.identityPublicKey = *decoded;
        }
    }
    if (const auto name = jsonString(payload, "xname"); name) {
        login.name = *name;
    }
    if (const auto xuid = jsonString(payload, "xid"); xuid) {
        login.xuid = *xuid;
    }
}

} // namespace

std::optional<RequestNetworkSettings> readRequestNetworkSettings(io::ByteBuffer& buffer) {
    RequestNetworkSettings request;
    request.protocolVersion = buffer.readInt();
    if (buffer.failed()) {
        return std::nullopt;
    }
    return request;
}

void writeNetworkSettings(io::ByteBuffer& buffer, std::uint16_t compressionThreshold, std::uint16_t algorithm) {
    writeShortLE(buffer, compressionThreshold);
    writeShortLE(buffer, algorithm);
    writeBool(buffer, false); // No client throttle: the server decides what it sends.
    buffer.writeByte(0);
    writeFloatLE(buffer, 0.0F);
}

std::optional<LoginRequest> readLogin(io::ByteBuffer& buffer) {
    LoginRequest login;
    login.protocolVersion = buffer.readInt();

    const std::uint32_t blobLength = readVarUInt(buffer);
    if (buffer.failed() || blobLength > buffer.readableBytes()) {
        return std::nullopt;
    }

    const auto authLength = static_cast<std::uint32_t>(readIntLE(buffer));
    if (buffer.failed() || authLength > buffer.readableBytes()) {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> authBytes = buffer.readSpan(authLength);
    const std::string auth{reinterpret_cast<const char*>(authBytes.data()), authBytes.size()};

    const auto clientLength = static_cast<std::uint32_t>(readIntLE(buffer));
    if (buffer.failed() || clientLength > buffer.readableBytes()) {
        return std::nullopt;
    }
    static_cast<void>(buffer.readSpan(clientLength)); // The skin and the device, which nothing needs yet.

    // A chain proves who the client is with Mojang's signature over it; a token is the same claim
    // made by the newer service. Either can arrive, and which one did is what says whether anybody
    // vouched for the name.
    if (const auto certificate = jsonString(auth, "Certificate"); certificate && !certificate->empty()) {
        const auto entry = lastChainEntry(*certificate);
        if (!entry) {
            return std::nullopt;
        }
        const auto parts = decodeJwt(*entry);
        if (!parts) {
            return std::nullopt;
        }
        readExtraData(parts->payload, login);
        login.authenticated = !login.xuid.empty();
    } else if (const auto token = jsonString(auth, "Token"); token && !token->empty()) {
        const auto parts = decodeJwt(*token);
        if (!parts) {
            return std::nullopt;
        }
        readTokenClaims(parts->payload, login);
        login.authenticated = !login.xuid.empty();
    } else {
        // The oldest shape: the chain is the whole document, with no wrapper around it.
        const auto entry = lastChainEntry(auth);
        if (!entry) {
            return std::nullopt;
        }
        const auto parts = decodeJwt(*entry);
        if (!parts) {
            return std::nullopt;
        }
        readExtraData(parts->payload, login);
    }

    if (login.name.empty() || login.identityPublicKey.empty()) {
        return std::nullopt;
    }
    return login;
}

std::optional<ResourcePackResponse> readResourcePackResponse(io::ByteBuffer& buffer, const int protocol) {
    // 2168 moved the status off by one and put its name behind it; below that it is a plain byte.
    const std::uint32_t status = writesNativeShapes(protocol) ? readVarUInt(buffer) + 1 : buffer.readByte();
    if (buffer.failed() || status < 1 || status > 4) {
        return std::nullopt;
    }
    return static_cast<ResourcePackResponse>(status);
}

void writePlayStatus(io::ByteBuffer& buffer, PlayStatus status) { buffer.writeInt(static_cast<std::int32_t>(status)); }

void writeServerToClientHandshake(io::ByteBuffer& buffer, std::string_view jwt) { writeString(buffer, jwt); }

void writeResourcePacksInfo(io::ByteBuffer& buffer, const int protocol) {
    writeBool(buffer, false); // Nothing to accept.
    writeBool(buffer, false); // No addon packs.
    writeBool(buffer, false); // No scripting.
    writeBool(buffer, false); // Vibrant visuals left to the client.
    writeUuid(buffer, 0, 0);
    writeString(buffer, "");
    // No packs, counted as 2168 counts an array. Below it the count of this one list is a short.
    if (writesNativeShapes(protocol)) {
        writeVarUInt(buffer, 0);
    } else {
        writeShortLE(buffer, 0);
    }
}

void writeResourcePackStack(io::ByteBuffer& buffer, std::string_view gameVersion) {
    writeBool(buffer, false);
    writeVarUInt(buffer, 0); // No packs in the stack either.
    writeString(buffer, gameVersion);
    writeIntLE(buffer, 0); // No experiments.
    writeBool(buffer, false);
    writeBool(buffer, false);
}

void writeDisconnect(io::ByteBuffer& buffer, std::string_view reason) {
    writeVarInt(buffer, 0); // Unknown: the message is what carries the explanation.
    writeVarUInt(buffer, 0);
    writeString(buffer, reason);
    writeString(buffer, "");
}

void writeStartGame(io::ByteBuffer& buffer, const StartGameInfo& info, const int protocol) {
    writeVarLong(buffer, info.uniqueEntityId);
    writeVarULong(buffer, info.runtimeEntityId);
    writeVarInt(buffer, info.playerGameType);
    writeFloatLE(buffer, info.x);
    writeFloatLE(buffer, info.y);
    writeFloatLE(buffer, info.z);
    writeFloatLE(buffer, info.yaw);
    writeFloatLE(buffer, info.pitch);

    // Level settings.
    writeLongLE(buffer, info.seed);
    writeShortLE(buffer, 0); // Spawn biome: whatever the generator says.
    writeString(buffer, "plains");
    writeVarInt(buffer, info.dimension);
    writeVarInt(buffer, 1); // Infinite world.
    writeVarInt(buffer, info.levelGameType);
    writeBool(buffer, false); // Not hardcore.
    writeVarInt(buffer, info.difficulty);
    writeBlockPosition(buffer, info.spawnX, info.spawnY, info.spawnZ);
    writeBool(buffer, true); // Achievements off: a server with commands cannot grant them anyway.
    writeVarInt(buffer, 0);  // Not an editor world.
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeVarInt(buffer, -1); // The day cycle runs.
    writeVarUInt(buffer, 0); // No education offers.
    writeBool(buffer, false);
    writeString(buffer, "");
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeBool(buffer, false);
    writeBool(buffer, true); // A multiplayer game, which is the only kind a server hosts.
    writeBool(buffer, true);
    writeVarInt(buffer, 4); // Broadcast to everyone, on both counts.
    writeVarInt(buffer, 4);
    writeBool(buffer, info.commandsEnabled);
    writeBool(buffer, false);
    writeVarUInt(buffer, 0); // No gamerules yet.
    writeIntLE(buffer, 0);   // No experiments.
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    // Members by default. 2168 writes the permission as a byte where everything below it wrote a
    // varint, which for the number one is a different byte and not a different size.
    if (writesNativeShapes(protocol)) {
        buffer.writeByte(1);
    } else {
        writeVarInt(buffer, 1);
    }
    writeIntLE(buffer, 4); // Chunk tick range.
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeString(buffer, info.vanillaVersion);
    writeIntLE(buffer, 0);
    writeIntLE(buffer, 0);
    writeBool(buffer, false); // Not the nether.
    writeString(buffer, "");
    writeString(buffer, "");
    writeBool(buffer, false); // No forced experimental gameplay.
    buffer.writeByte(0);      // Chat unrestricted.
    writeBool(buffer, false); // Player interactions left alone.
    if (protocol >= kStartGameGrewProtocol) {
        writeVarInt(buffer, 0);   // No editor connection,
        writeBool(buffer, false); // and nothing dropped anonymously in an editor world.
    }

    writeString(buffer, info.levelId);
    writeString(buffer, info.levelName);
    writeString(buffer, ""); // No premium world template.
    writeBool(buffer, false);
    // How movement is settled, in two fields. There used to be a third in front of them saying
    // which mode it was, and **818 took it away**: from there the server always verifies movement
    // from `player_auth_input` and there is nothing to choose. Writing it anyway puts one varint
    // too many in the middle of the packet, and a client reads everything after it shifted: that is
    // what an `InitialConnection-90` on a real 1.26 client looks like.
    writeVarInt(buffer, 0);   // No rewind history: this server does not rewind.
    writeBool(buffer, false); // Block breaking stays with the client for now.
    writeLongLE(buffer, info.currentTick);
    writeVarInt(buffer, 0);  // Enchantment seed.
    writeVarUInt(buffer, 0); // No custom blocks.
    writeString(buffer, info.multiplayerCorrelationId);
    writeBool(buffer, true); // Inventories are the server's.
    writeString(buffer, "Basalt");
    writeEmptyCompound(buffer); // No player properties.
    writeLongLE(buffer, 0);     // No block registry checksum: the client checks nothing when it is zero.
    writeUuid(buffer, 0, 0);
    writeBool(buffer, false); // The client does not generate its own chunks.
    // Block ids are the position in the client's own palette, which is what the generated tables
    // hold: saying they are hashes of the state would have the client resolve every one of them
    // against the wrong thing.
    writeBool(buffer, false);
    writeBool(buffer, true); // Sounds come from the server.
    if (protocol >= kStartGameGrewProtocol && !writesNativeShapes(protocol)) {
        // Whether chat is logged, which only 1001 asks: 975 and below never had it and 2168
        // stopped asking.
        writeBool(buffer, false);
    }
    writeBool(buffer, false); // No join info.
    writeString(buffer, info.serverId);
    writeString(buffer, ""); // No scenario.
    writeString(buffer, info.worldId);
    writeString(buffer, ""); // No owner.
}

void writeTeleport(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const float x, const float y,
                   const float z, const float yaw, const float pitch, const std::uint64_t tick, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    // The rotation is pitch, yaw and the head's own yaw, which for a teleport are all the same way.
    writeFloatLE(buffer, pitch);
    writeFloatLE(buffer, yaw);
    writeFloatLE(buffer, yaw);
    buffer.writeByte(2); // A teleport, not a step.
    writeBool(buffer, false);
    writeVarULong(buffer, 0); // Riding nothing.
    // Only a teleport says why it happened, and this one always does. 2168 puts a "it is here"
    // flag in front of the reason; below it the reason simply follows a teleport.
    if (writesNativeShapes(protocol)) {
        writeBool(buffer, true);
    }
    writeIntLE(buffer, 3); // Because something asked for it, which is what a command is.
    writeIntLE(buffer, 0);
    writeVarULong(buffer, tick);
}

void writeSetPlayerGameType(io::ByteBuffer& buffer, const std::int32_t gameType) { writeVarInt(buffer, gameType); }

void writeSetTitle(io::ByteBuffer& buffer, const TitleType type, const std::string_view text, const std::int32_t fadeIn,
                   const std::int32_t stay, const std::int32_t fadeOut) {
    writeVarInt(buffer, static_cast<std::int32_t>(type));
    writeString(buffer, text);
    writeVarInt(buffer, fadeIn);
    writeVarInt(buffer, stay);
    writeVarInt(buffer, fadeOut);
    writeString(buffer, ""); // No xuid.
    writeString(buffer, ""); // No platform id.
    writeString(buffer, ""); // Nothing filtered.
}

void writeSetTime(io::ByteBuffer& buffer, const std::int32_t time) { writeVarInt(buffer, time); }

std::optional<std::string> readCommandRequest(io::ByteBuffer& buffer) {
    std::string command = readString(buffer);
    if (buffer.failed() || command.empty()) {
        return std::nullopt;
    }
    // A client sends the slash with it; the dispatcher wants the line without one.
    if (command.front() == '/') {
        command.erase(command.begin());
    }
    return command;
}

void writeModalFormRequest(io::ByteBuffer& buffer, const std::uint32_t formId, const std::string_view json) {
    writeVarUInt(buffer, formId);
    writeString(buffer, json);
}

void writeCloseForm(io::ByteBuffer& buffer) {
    static_cast<void>(buffer); // Nothing in it: the packet itself is the message.
}

std::optional<ModalFormResponse> readModalFormResponse(io::ByteBuffer& buffer) {
    ModalFormResponse response;
    response.formId = readVarUInt(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    if (buffer.readByte() != 0) {
        response.data = readString(buffer);
    }
    // What follows is why it was dismissed, which only matters when there is no answer, and "no
    // answer" is already what an empty one means here.
    if (buffer.failed()) {
        return std::nullopt;
    }
    return response;
}

std::string simpleFormJson(const std::string_view title, const std::string_view content,
                           const std::span<const std::string> buttons) {
    common::JsonObject form;
    form.set("type", common::JsonValue::of(std::string{"form"}));
    form.set("title", common::JsonValue::of(std::string{title}));
    form.set("content", common::JsonValue::of(std::string{content}));

    std::vector<common::JsonValue> entries;
    entries.reserve(buttons.size());
    for (const std::string& button : buttons) {
        common::JsonObject entry;
        entry.set("text", common::JsonValue::of(button));
        entries.push_back(common::JsonValue::ofObject(std::move(entry)));
    }
    form.set("buttons", common::JsonValue::ofArray(std::move(entries)));
    return common::writeJson(common::JsonValue::ofObject(std::move(form)));
}

namespace {

/// Which of the flags this server acts on. They are positions in an enum the reference codec keeps,
/// and nothing has moved them inside this window.
constexpr std::int32_t kFlagSneaking = 8;
constexpr std::int32_t kFlagItemInteraction = 34;
constexpr std::int32_t kFlagBlockActions = 35;
constexpr std::int32_t kFlagItemStackRequest = 36;

/// One block position as this edition writes it: the height is unsigned, which is not a mistake of
/// ours either.
void readBedrockBlockPosition(io::ByteBuffer& buffer, std::int32_t& x, std::int32_t& y, std::int32_t& z) {
    x = readVarInt(buffer);
    y = static_cast<std::int32_t>(readVarUInt(buffer));
    z = readVarInt(buffer);
}

/// One item as the client sends it back. Only the id is wanted; the rest is walked past.
///
/// From 2168 it is the descriptor shape, which is not the same packet field written twice: the id
/// is a short, and a stack of nothing still writes every field rather than stopping at the zero.
[[nodiscard]] std::int32_t readItem(io::ByteBuffer& buffer, const int protocol) {
    if (protocol >= kHeldItemDescriptorProtocol) {
        const std::int32_t id = static_cast<std::int16_t>(readShortLE(buffer));
        static_cast<void>(readShortLE(buffer)); // How many,
        static_cast<void>(readVarUInt(buffer)); // how damaged,
        if (buffer.readByte() != 0) {
            static_cast<void>(readVarInt(buffer)); // and which one of its kind, when it is counted.
        }
        static_cast<void>(readVarUInt(buffer)); // Which block it places.
        const std::uint32_t trailing = readVarUInt(buffer);
        for (std::uint32_t at = 0; at < trailing && !buffer.failed(); ++at) {
            static_cast<void>(buffer.readByte());
        }
        return id;
    }
    const std::int32_t id = readVarInt(buffer);
    if (id == 0) {
        return 0;
    }
    static_cast<void>(readShortLE(buffer)); // How many,
    static_cast<void>(readVarUInt(buffer)); // how damaged,
    if (buffer.readByte() != 0) {
        static_cast<void>(readVarInt(buffer)); // and which one of its kind, when it is counted.
    }
    static_cast<void>(readVarInt(buffer)); // Which block it places.
    const std::uint32_t extra = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < extra && !buffer.failed(); ++at) {
        static_cast<void>(buffer.readByte());
    }
    return id;
}

/// Where a stack came from. Two of the five kinds carry a number of their own.
void readInventorySource(io::ByteBuffer& buffer) {
    const std::uint32_t kind = readVarUInt(buffer);
    if (kind == 0 || kind == 99999) {
        static_cast<void>(readVarInt(buffer)); // Which window,
    } else if (kind == 2) {
        static_cast<void>(readVarUInt(buffer)); // or what the world did with it.
    }
}

/// The click itself. Everything before the block it names is walked past: this server does not let
/// a client tell it what moved where, only what was clicked.
[[nodiscard]] std::optional<BedrockItemUse> readItemUseTransaction(io::ByteBuffer& buffer, const int protocol) {
    // 2168 put a boolean in front of each of the two lists, and turned three of the small numbers
    // from varints into single bytes. Everything else is the same run of fields.
    const bool native = protocol >= kHeldItemDescriptorProtocol;
    const std::int32_t legacyRequestId = readVarInt(buffer);
    const bool legacySlots = native ? (buffer.readByte() != 0) : true;
    if (legacySlots && legacyRequestId < -1 && (legacyRequestId & 1) == 0) {
        const std::uint32_t windows = readVarUInt(buffer);
        for (std::uint32_t at = 0; at < windows && !buffer.failed(); ++at) {
            static_cast<void>(buffer.readByte());
            const std::uint32_t slots = readVarUInt(buffer);
            for (std::uint32_t slot = 0; slot < slots && !buffer.failed(); ++slot) {
                static_cast<void>(buffer.readByte());
            }
        }
    }
    if (!native || ((buffer.readByte() != 0) && (buffer.readByte() != 0))) {
        const std::uint32_t actions = readVarUInt(buffer);
        for (std::uint32_t at = 0; at < actions && !buffer.failed(); ++at) {
            readInventorySource(buffer);
            static_cast<void>(readVarUInt(buffer));        // Which slot,
            static_cast<void>(readItem(buffer, protocol)); // what was there,
            static_cast<void>(readItem(buffer, protocol)); // and what is now.
        }
    }

    BedrockItemUse use;
    use.action = native ? readVarInt(buffer) : static_cast<std::int32_t>(readVarUInt(buffer));
    // What set the click off: a hand, a hold, or the server asking again.
    static_cast<void>(native ? std::int32_t{buffer.readByte()} : static_cast<std::int32_t>(readVarUInt(buffer)));
    readBedrockBlockPosition(buffer, use.x, use.y, use.z);
    use.face = native ? std::int32_t{buffer.readByte()} : readVarInt(buffer);
    use.hotbarSlot = readVarInt(buffer);
    use.itemRuntimeId = readItem(buffer, protocol);
    static_cast<void>(readFloatLE(buffer)); // Where the player was,
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    use.clickX = readFloatLE(buffer); // and where on the face they clicked.
    use.clickY = readFloatLE(buffer);
    use.clickZ = readFloatLE(buffer);
    static_cast<void>(readVarUInt(buffer)); // Which block they think they hit,
    // and what they think the click did, which this server decides for itself.
    static_cast<void>(native ? std::int32_t{buffer.readByte()} : static_cast<std::int32_t>(readVarUInt(buffer)));
    if (protocol >= kItemCooldownProtocol) {
        static_cast<void>(buffer.readByte()); // How far into its cooldown the client thinks it is.
    }
    if (buffer.failed()) {
        return std::nullopt;
    }
    return use;
}

/// Every line a stack request can carry, whether or not this server acts on it: one that cannot be
/// read leaves the rest of the tick's packet unreachable, so all of them are walked past.
enum class RequestAction : std::int32_t {
    Unknown,
    Take,
    Place,
    Swap,
    Drop,
    Destroy,
    Consume,
    Create,
    LabTableCombine,
    BeaconPayment,
    MineBlock,
    CraftRecipe,
    CraftRecipeAuto,
    CraftCreative,
    CraftRecipeOptional,
    CraftGrindstone,
    CraftLoom,
    CraftNonImplemented,
    CraftResults,
};

/// What the number on the wire means. 2168 renumbered the list from the top after two entries that
/// had been removed left holes in the middle, so the two tables have nothing in common past six.
[[nodiscard]] RequestAction requestActionOf(const std::int32_t id, const int protocol) {
    if (writesNativeShapes(protocol)) {
        switch (id) {
        case 0:
            return RequestAction::Take;
        case 1:
            return RequestAction::Place;
        case 2:
            return RequestAction::Swap;
        case 3:
            return RequestAction::Drop;
        case 4:
            return RequestAction::Destroy;
        case 5:
            return RequestAction::Consume;
        case 6:
            return RequestAction::Create;
        case 7:
            return RequestAction::LabTableCombine;
        case 8:
            return RequestAction::BeaconPayment;
        case 9:
            return RequestAction::MineBlock;
        case 10:
            return RequestAction::CraftRecipe;
        case 11:
            return RequestAction::CraftRecipeAuto;
        case 12:
            return RequestAction::CraftCreative;
        case 13:
            return RequestAction::CraftRecipeOptional;
        case 14:
            return RequestAction::CraftGrindstone;
        case 15:
            return RequestAction::CraftLoom;
        case 16:
            return RequestAction::CraftNonImplemented;
        case 17:
            return RequestAction::CraftResults;
        default:
            return RequestAction::Unknown;
        }
    }
    switch (id) {
    case 0:
        return RequestAction::Take;
    case 1:
        return RequestAction::Place;
    case 2:
        return RequestAction::Swap;
    case 3:
        return RequestAction::Drop;
    case 4:
        return RequestAction::Destroy;
    case 5:
        return RequestAction::Consume;
    case 6:
        return RequestAction::Create;
    case 9:
        return RequestAction::LabTableCombine;
    case 10:
        return RequestAction::BeaconPayment;
    case 11:
        return RequestAction::MineBlock;
    case 12:
        return RequestAction::CraftRecipe;
    case 13:
        return RequestAction::CraftRecipeAuto;
    case 14:
        return RequestAction::CraftCreative;
    case 15:
        return RequestAction::CraftRecipeOptional;
    case 16:
        return RequestAction::CraftGrindstone;
    case 17:
        return RequestAction::CraftLoom;
    case 18:
        return RequestAction::CraftNonImplemented;
    case 19:
        return RequestAction::CraftResults;
    default:
        return RequestAction::Unknown;
    }
}

[[nodiscard]] BedrockStackActionType publicActionOf(const RequestAction action) {
    switch (action) {
    case RequestAction::Take:
        return BedrockStackActionType::Take;
    case RequestAction::Place:
        return BedrockStackActionType::Place;
    case RequestAction::Swap:
        return BedrockStackActionType::Swap;
    case RequestAction::Drop:
        return BedrockStackActionType::Drop;
    case RequestAction::Destroy:
        return BedrockStackActionType::Destroy;
    case RequestAction::Consume:
        return BedrockStackActionType::Consume;
    case RequestAction::Create:
        return BedrockStackActionType::Create;
    case RequestAction::CraftResults:
        return BedrockStackActionType::CraftResults;
    default:
        return BedrockStackActionType::Other;
    }
}

/// One end of a move. The window is named by a byte and an id of its own that only a bundle uses,
/// and the stack's number went from a varint to a fixed four bytes at 2168.
[[nodiscard]] BedrockStackSlot readStackSlot(io::ByteBuffer& buffer, const int protocol) {
    BedrockStackSlot slot;
    slot.container = buffer.readByte();
    if (buffer.readByte() != 0) {
        static_cast<void>(readIntLE(buffer));
    }
    slot.slot = buffer.readByte();
    slot.stackNetworkId = writesNativeShapes(protocol) ? readIntLE(buffer) : readVarInt(buffer);
    return slot;
}

/// One item as a recipe names it, which is a shape of its own: a name or a tag rather than an id.
void readIngredient(io::ByteBuffer& buffer, const int protocol) {
    if (writesNativeShapes(protocol)) {
        const std::uint32_t kind = readVarUInt(buffer);
        static_cast<void>(buffer.readByte()); // The same kind again, which the client writes twice.
        if (kind == 1) {
            static_cast<void>(readString(buffer));
            static_cast<void>(readVarInt(buffer));
        } else if (kind == 2) {
            static_cast<void>(readString(buffer));
            static_cast<void>(readShortLE(buffer));
        } else if (kind == 3) {
            static_cast<void>(readString(buffer));
        }
        static_cast<void>(readShortLE(buffer));
        return;
    }
    const std::uint8_t kind = buffer.readByte();
    if (kind == 1) {
        if (readShortLE(buffer) != 0) {
            static_cast<void>(readShortLE(buffer));
        }
    } else if (kind == 2) {
        static_cast<void>(readString(buffer));
        static_cast<void>(buffer.readByte());
    } else if (kind == 3 || kind == 5) {
        static_cast<void>(readString(buffer));
    } else if (kind == 4) {
        static_cast<void>(readString(buffer));
        static_cast<void>(readShortLE(buffer));
    }
    static_cast<void>(readVarInt(buffer));
}

/// One item as a finished result names it. Below 2168 it is the plain shape without the flag that
/// says a server is counting stacks; from there it is named rather than numbered.
void readResultItem(io::ByteBuffer& buffer, const int protocol) {
    if (writesNativeShapes(protocol)) {
        const std::uint32_t kind = readVarUInt(buffer);
        static_cast<void>(buffer.readByte());
        if (kind != 0) {
            static_cast<void>(readString(buffer));
            static_cast<void>(readVarInt(buffer));
        }
        static_cast<void>(readShortLE(buffer)); // How many,
        static_cast<void>(readVarUInt(buffer)); // which block it places,
    } else {
        if (readVarInt(buffer) == 0) {
            return;
        }
        static_cast<void>(readShortLE(buffer));
        static_cast<void>(readVarUInt(buffer));
        static_cast<void>(readVarInt(buffer));
    }
    const std::uint32_t trailing = readVarUInt(buffer); // and what is written on it.
    for (std::uint32_t at = 0; at < trailing && !buffer.failed(); ++at) {
        static_cast<void>(buffer.readByte());
    }
}

/// A request to move stacks between windows, as the client asks for it.
[[nodiscard]] std::optional<BedrockStackRequest> readItemStackRequest(io::ByteBuffer& buffer, const int protocol) {
    const bool native = writesNativeShapes(protocol);
    BedrockStackRequest request;
    request.requestId = readVarInt(buffer);
    const std::uint32_t lines = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < lines && !buffer.failed(); ++at) {
        const std::int32_t id = native ? static_cast<std::int32_t>(readVarUInt(buffer)) : buffer.readByte();
        const RequestAction kind = requestActionOf(id, protocol);
        if (native) {
            static_cast<void>(buffer.readByte()); // The same line again, which the client writes twice.
        }
        BedrockStackAction action;
        action.type = publicActionOf(kind);
        action.line = id;
        switch (kind) {
        case RequestAction::Take:
        case RequestAction::Place:
            action.count = buffer.readByte();
            action.source = readStackSlot(buffer, protocol);
            action.destination = readStackSlot(buffer, protocol);
            break;
        case RequestAction::Swap:
            action.source = readStackSlot(buffer, protocol);
            action.destination = readStackSlot(buffer, protocol);
            break;
        case RequestAction::Drop:
            action.count = buffer.readByte();
            action.source = readStackSlot(buffer, protocol);
            static_cast<void>(buffer.readByte()); // Whether it is thrown rather than dropped.
            break;
        case RequestAction::Destroy:
        case RequestAction::Consume:
            action.count = buffer.readByte();
            action.source = readStackSlot(buffer, protocol);
            break;
        case RequestAction::Create:
            action.count = buffer.readByte();
            break;
        case RequestAction::LabTableCombine:
        case RequestAction::CraftNonImplemented:
            break;
        case RequestAction::BeaconPayment:
            static_cast<void>(readVarInt(buffer));
            static_cast<void>(readVarInt(buffer));
            break;
        case RequestAction::MineBlock:
            static_cast<void>(readVarInt(buffer)); // Which hotbar slot,
            static_cast<void>(readVarInt(buffer)); // how worn the client thinks the tool is,
            static_cast<void>(native ? readIntLE(buffer) : readVarInt(buffer)); // and which stack it was.
            break;
        case RequestAction::CraftRecipe:
        case RequestAction::CraftCreative:
            static_cast<void>(readVarUInt(buffer));
            static_cast<void>(buffer.readByte());
            break;
        case RequestAction::CraftRecipeAuto: {
            static_cast<void>(readVarUInt(buffer));
            static_cast<void>(buffer.readByte());
            if (!native) {
                static_cast<void>(buffer.readByte()); // How many times it was made, counted twice.
            }
            const std::uint32_t ingredients = native ? readVarUInt(buffer) : buffer.readByte();
            for (std::uint32_t item = 0; item < ingredients && !buffer.failed(); ++item) {
                readIngredient(buffer, protocol);
            }
            break;
        }
        case RequestAction::CraftRecipeOptional:
            static_cast<void>(readVarUInt(buffer));
            static_cast<void>(readIntLE(buffer));
            break;
        case RequestAction::CraftGrindstone:
            static_cast<void>(native ? readIntLE(buffer) : static_cast<std::int32_t>(readVarUInt(buffer)));
            static_cast<void>(buffer.readByte());
            static_cast<void>(readVarInt(buffer));
            break;
        case RequestAction::CraftLoom:
            static_cast<void>(readString(buffer));
            static_cast<void>(buffer.readByte());
            break;
        case RequestAction::CraftResults: {
            const std::uint32_t results = readVarUInt(buffer);
            for (std::uint32_t item = 0; item < results && !buffer.failed(); ++item) {
                readResultItem(buffer, protocol);
            }
            static_cast<void>(buffer.readByte());
            break;
        }
        case RequestAction::Unknown:
            // Nothing says how long an unknown line is, so the rest of the packet is given up on.
            return std::nullopt;
        }
        request.actions.push_back(action);
    }

    // What the client typed where a server may filter it, and where that text came from.
    const std::uint32_t filtered = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < filtered && !buffer.failed(); ++at) {
        static_cast<void>(readString(buffer));
    }
    static_cast<void>(readIntLE(buffer));
    if (buffer.failed()) {
        return std::nullopt;
    }
    return request;
}

} // namespace

std::optional<PlayerAuthInput> readPlayerAuthInput(io::ByteBuffer& buffer, const int protocol) {
    PlayerAuthInput input;
    // The rotation is split around the position: pitch and yaw lead, the head's own yaw follows the
    // movement vector. It is not an accident of ours.
    input.pitch = readFloatLE(buffer);
    input.yaw = readFloatLE(buffer);
    input.x = readFloatLE(buffer);
    input.y = readFloatLE(buffer);
    input.z = readFloatLE(buffer);
    static_cast<void>(readFloatLE(buffer)); // Where the client is trying to move, sideways.
    static_cast<void>(readFloatLE(buffer)); // The same, forwards.
    input.headYaw = readFloatLE(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }

    // What the player is doing, which decides what the rest of the packet holds. 2168 turned the
    // run of bits into a list of the ones that are on; below it they are one long varint.
    std::set<std::int32_t> flags;
    if (protocol >= kInputFlagListProtocol) {
        if ((buffer.readByte() != 0)) {
            const std::uint32_t count = readVarUInt(buffer);
            for (std::uint32_t at = 0; at < count && !buffer.failed(); ++at) {
                flags.insert(readVarInt(buffer));
            }
        }
    } else {
        for (std::int32_t bit = 0; !buffer.failed();) {
            const std::uint8_t part = buffer.readByte();
            for (std::int32_t at = 0; at < 7; ++at, ++bit) {
                if ((part & (1U << static_cast<std::uint32_t>(at))) != 0) {
                    flags.insert(bit);
                }
            }
            if ((part & 0x80U) == 0) {
                break;
            }
        }
    }
    input.sneaking = flags.contains(kFlagSneaking);
    static_cast<void>(readVarUInt(buffer)); // How they are playing: a keyboard, a pad, a screen,
    static_cast<void>(readVarUInt(buffer)); // where they are playing it,
    static_cast<void>(readVarUInt(buffer)); // and which of the two ways of touching things.
    static_cast<void>(readFloatLE(buffer)); // Where they are looking when that is not where they face.
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readVarULong(buffer)); // Which tick this is,
    static_cast<void>(readFloatLE(buffer));  // and how far they moved in it.
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    if (buffer.failed()) {
        return std::nullopt;
    }

    // From here the packet is a run of optional pieces, each one there because a flag says so. 2168
    // puts a pair of booleans in front of each: the first says the field exists at all.
    const auto present = [&buffer, protocol](const bool flagged) {
        if (protocol < kInputFlagListProtocol) {
            return flagged;
        }
        return (buffer.readByte() != 0) && (buffer.readByte() != 0);
    };
    if (present(flags.contains(kFlagItemInteraction))) {
        input.itemUse = readItemUseTransaction(buffer, protocol);
    }
    if (present(flags.contains(kFlagItemStackRequest))) {
        input.stackRequest = readItemStackRequest(buffer, protocol);
        if (!input.stackRequest.has_value()) {
            // A line this build cannot measure leaves the rest of the tick unreadable.
            input.itemUse.reset();
            return input;
        }
    }
    if (present(flags.contains(kFlagBlockActions))) {
        const std::int32_t count =
            protocol >= kInputFlagListProtocol ? static_cast<std::int32_t>(readVarUInt(buffer)) : readVarInt(buffer);
        for (std::int32_t at = 0; at < count && !buffer.failed(); ++at) {
            BedrockBlockAction action;
            action.action = readVarInt(buffer);
            // Below 2168 only the five that mean a block carry one; from 2168 every line does.
            const bool positioned =
                protocol >= kInputFlagListProtocol ||
                action.action == static_cast<std::int32_t>(BedrockBlockActionType::StartBreak) ||
                action.action == static_cast<std::int32_t>(BedrockBlockActionType::AbortBreak) ||
                action.action == static_cast<std::int32_t>(BedrockBlockActionType::ContinueBreak) ||
                action.action == static_cast<std::int32_t>(BedrockBlockActionType::PredictDestroy) ||
                action.action == static_cast<std::int32_t>(BedrockBlockActionType::ContinueDestroy);
            if (positioned) {
                action.x = readVarInt(buffer);
                action.y = readVarInt(buffer);
                action.z = readVarInt(buffer);
                action.face = readVarInt(buffer);
            }
            input.blockActions.push_back(action);
        }
    }
    if (buffer.failed()) {
        input.blockActions.clear();
        input.itemUse.reset();
    }
    return input;
}

void writeText(io::ByteBuffer& buffer, TextType type, std::string_view source, std::string_view message,
               std::string_view xuid) {
    const bool withAuthor = type == TextType::Chat || type == TextType::Whisper || type == TextType::Announcement;

    writeBool(buffer, false); // Nothing here is a translation key; the server resolved it already.
    buffer.writeByte(withAuthor ? 1 : 0);
    buffer.writeByte(static_cast<std::uint8_t>(type));
    if (withAuthor) {
        writeString(buffer, source);
    }
    // A client with an empty line to draw logs a complaint and draws nothing, so it gets a space.
    writeString(buffer, message.empty() ? std::string_view{" "} : message);
    writeString(buffer, xuid);
    writeString(buffer, "");  // No platform chat id: this server does not route between consoles.
    writeBool(buffer, false); // Nothing filtered.
}

std::optional<IncomingText> readText(io::ByteBuffer& buffer) {
    static_cast<void>(buffer.readByte()); // Whether the client thinks this needs translating.
    const std::uint8_t shape = buffer.readByte();
    const std::uint8_t type = buffer.readByte();
    if (buffer.failed() || shape > 2 || type > static_cast<std::uint8_t>(TextType::Announcement)) {
        return std::nullopt;
    }

    IncomingText text;
    text.type = static_cast<TextType>(type);
    if (shape == 1) {
        text.source = readString(buffer);
    }
    text.message = readString(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    return text;
}

void writeChunkRadiusUpdated(io::ByteBuffer& buffer, std::int32_t radius) { writeVarInt(buffer, radius); }

std::optional<std::int32_t> readRequestChunkRadius(io::ByteBuffer& buffer) {
    const std::int32_t radius = readVarInt(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    return radius;
}

void writeNetworkChunkPublisherUpdate(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::uint32_t radius) {
    writeVarInt(buffer, x);
    writeVarInt(buffer, y);
    writeVarInt(buffer, z);
    writeVarUInt(buffer, radius);
    writeIntLE(buffer, 0); // No saved chunks.
}

namespace {

/// One stack where it is named rather than moved: the creative menu and an entity's hand. It is
/// the window shape without the flag that says a server is counting stacks.
void writeItemInstance(io::ByteBuffer& buffer, const BedrockItemStack& item) {
    if (item.runtimeId == 0 || item.count == 0) {
        writeVarInt(buffer, 0);
        return;
    }
    writeVarInt(buffer, item.runtimeId);
    writeShortLE(buffer, static_cast<std::uint16_t>(item.count));
    writeVarUInt(buffer, static_cast<std::uint32_t>(item.data));
    writeVarInt(buffer, item.blockRuntimeId);
    writeVarUInt(buffer, 10);
    writeShortLE(buffer, 0);
    writeIntLE(buffer, 0);
    writeIntLE(buffer, 0);
}

/// How many bits a palette of this size needs, rounded up to one of the widths Bedrock packs with.
/// Six and five and three leave two bits of every word unused, which is why the list is not simply
/// the powers of two.
[[nodiscard]] std::uint8_t bitsFor(const std::size_t entries) {
    for (const std::uint8_t bits : std::array<std::uint8_t, 8>{1, 2, 3, 4, 5, 6, 8, 16}) {
        if (entries <= (std::size_t{1} << bits)) {
            return bits;
        }
    }
    return 16;
}

} // namespace

void writeSubChunk(io::ByteBuffer& buffer, const JavaSubChunk blocks, const std::int8_t subChunkIndex,
                   const int protocol) {
    // Version nine, which is the one that says which subchunk it is rather than leaving the client
    // to count. One storage layer: the second is for water inside another block, and nothing here
    // puts anything there yet.
    buffer.writeByte(9);
    buffer.writeByte(1);
    buffer.writeByte(static_cast<std::uint8_t>(subChunkIndex));

    // The palette, in the order the blocks are first met, and the blocks as indices into it.
    std::vector<std::uint16_t> palette;
    std::array<std::uint16_t, kSubChunkBlocks> indices{};
    for (std::size_t over = 0; over < kSubChunkBlocks; ++over) {
        // The two editions nest their loops the opposite way round: Bedrock counts x slowest and y
        // fastest, Java the other way. This is the only place the two orders have to meet.
        const std::size_t x = (over >> 8U) & 0xFU;
        const std::size_t z = (over >> 4U) & 0xFU;
        const std::size_t y = over & 0xFU;
        const std::size_t here = (y << 8U) | (z << 4U) | x;
        const std::int32_t state = here < blocks.size() ? blocks[here] : 0;

        const std::uint16_t block = bedrockBlockId(state, protocol);
        const auto found = std::ranges::find(palette, block);
        if (found == palette.end()) {
            indices[over] = static_cast<std::uint16_t>(palette.size());
            palette.push_back(block);
        } else {
            indices[over] = static_cast<std::uint16_t>(found - palette.begin());
        }
    }

    const std::uint8_t bits = bitsFor(palette.size());
    // The bottom bit says the numbers are the client's own, not names it has to read.
    buffer.writeByte(static_cast<std::uint8_t>((static_cast<unsigned>(bits) << 1U) | 1U));

    const std::size_t perWord = 32U / static_cast<std::size_t>(bits);
    for (std::size_t at = 0; at < kSubChunkBlocks; at += perWord) {
        std::uint32_t word = 0;
        for (std::size_t slot = 0; slot < perWord && at + slot < kSubChunkBlocks; ++slot) {
            word |= static_cast<std::uint32_t>(indices[at + slot]) << (slot * bits);
        }
        writeIntLE(buffer, static_cast<std::int32_t>(word));
    }

    writeVarInt(buffer, static_cast<std::int32_t>(palette.size()));
    for (const std::uint16_t block : palette) {
        writeVarInt(buffer, block);
    }
}

void writeAvailableEntityIdentifiers(io::ByteBuffer& buffer) { buffer.writeBytes(entityIdentifiers()); }

void writeBiomeDefinitionList(io::ByteBuffer& buffer) {
    // One definition, and it is the one every chunk this server sends points at: the biome palette
    // in `level_chunk` is a single entry saying zero, so zero has to name something. An empty list
    // is not the same as sending nothing: a client told there are no biomes at all cannot resolve
    // the one its own chunks name. The real table, with a definition per biome and its climate and
    // surfaces, is still missing.
    writeVarUInt(buffer, 1);
    writeShortLE(buffer, 0);  // Its name is the first string below.
    writeBool(buffer, false); // No id of its own: its place in this list is its id.
    writeFloatLE(buffer, 0.8F);
    writeFloatLE(buffer, 0.4F);
    writeFloatLE(buffer, 0.0F); // No red spores,
    writeFloatLE(buffer, 0.0F); // no blue ones,
    writeFloatLE(buffer, 0.0F); // no ash,
    writeFloatLE(buffer, 0.0F); // and no white ash: those belong to the nether.
    writeFloatLE(buffer, 0.125F);
    writeFloatLE(buffer, 0.05F);
    writeIntLE(buffer, 0x0044aff5); // The water colour a map draws it in.
    writeBool(buffer, true);        // It rains here,
    writeBool(buffer, false);       // carries no tags,
    writeBool(buffer, false);       // and says nothing about how it is generated.

    writeVarUInt(buffer, 1);
    writeString(buffer, "plains");
}

void writeCreativeContent(io::ByteBuffer& buffer, const int protocol) {
    // One group with everything in it. Vanilla sorts its creative menu into four tabs and a great
    // many groups, and that order is content rather than protocol: what the protocol needs is that
    // every item a player may take is in a group, and that the group exists.
    writeVarUInt(buffer, 1);
    // Everything, rather than one of the four tabs.
    if (protocol >= kCreativeCategoryByteProtocol) {
        buffer.writeByte(0);
    } else {
        writeIntLE(buffer, 0);
    }
    writeString(buffer, "itemGroup.name.basalt");
    writeItemInstance(buffer, BedrockItemStack{});

    const std::span<const std::string_view> palette = bedrockItemPalette();
    writeVarUInt(buffer, static_cast<std::uint32_t>(palette.size()));
    std::uint32_t netId = 1; // Zero is "no stack", so the menu counts from one.
    for (std::size_t at = 0; at < palette.size(); ++at) {
        writeVarUInt(buffer, netId++);
        writeItemInstance(buffer, BedrockItemStack{.runtimeId = static_cast<std::int32_t>(at) + 1, .count = 1});
        writeVarUInt(buffer, 0); // All of them in the one group.
    }
}

void writeSetCommandsEnabled(io::ByteBuffer& buffer, const bool enabled) { writeBool(buffer, enabled); }

void writeSpawnParticle(io::ByteBuffer& buffer, const BedrockDimension dimension, const std::string_view particle,
                        const float x, const float y, const float z) {
    buffer.writeByte(static_cast<std::uint8_t>(dimension));
    writeVarLong(buffer, -1); // Belonging to no entity in particular.
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeString(buffer, particle);
    writeBool(buffer, false); // No variables of its own to fill in.
}

void writeEntityEvent(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockEntityEvent event,
                      const std::int32_t data, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    buffer.writeByte(static_cast<std::uint8_t>(event));
    writeVarInt(buffer, data);
    if (protocol >= kEntityEventGrewProtocol) {
        writeBool(buffer, false); // No position of its own, which only a fire event carries.
    }
}

void writeSetEntityMotion(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const float x, const float y,
                          const float z) {
    writeVarULong(buffer, runtimeEntityId);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

namespace {

/// One stack, in the shape the protocol asked for. Everything this server sends is plain: no net
/// id, no nbt, nothing it may only be placed on or broken with.
void writeItemStack(io::ByteBuffer& buffer, const BedrockItemStack& item, const int protocol) {
    const bool descriptor = protocol >= kItemDescriptorProtocol;
    if (item.runtimeId == 0 || item.count == 0) {
        if (descriptor) {
            writeShortLE(buffer, 0);
            writeShortLE(buffer, 0);
            writeVarUInt(buffer, 0);
            writeBool(buffer, false);
            writeVarUInt(buffer, 0);
            writeVarUInt(buffer, 0);
            return;
        }
        writeVarInt(buffer, 0);
        return;
    }
    if (descriptor) {
        writeShortLE(buffer, static_cast<std::uint16_t>(item.runtimeId));
    } else {
        writeVarInt(buffer, item.runtimeId);
    }
    writeShortLE(buffer, static_cast<std::uint16_t>(item.count));
    writeVarUInt(buffer, static_cast<std::uint32_t>(item.data));
    writeBool(buffer, false); // The server does not count stacks, so no net id follows.
    if (descriptor) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(item.blockRuntimeId));
    } else {
        writeVarInt(buffer, item.blockRuntimeId);
    }
    // What is written on it, and where it may be used. All three are empty, and the whole run is
    // little-endian rather than varints because it is a stream of its own inside the packet.
    writeVarUInt(buffer, 10);
    writeShortLE(buffer, 0); // No nbt,
    writeIntLE(buffer, 0);   // nothing it may be placed on,
    writeIntLE(buffer, 0);   // nothing it may break.
}

} // namespace

void writeItemComponent(io::ByteBuffer& buffer) {
    const std::span<const std::string_view> palette = bedrockItemPalette();
    writeVarUInt(buffer, static_cast<std::uint32_t>(palette.size()));
    std::uint16_t id = 1; // Zero is air, which is not in the list.
    for (const std::string_view name : palette) {
        writeString(buffer, name);
        writeShortLE(buffer, id++);
        writeBool(buffer, false); // Not driven by components of its own,
        writeVarInt(buffer, 2);   // and not a version that carries any.
        writeEmptyCompound(buffer);
    }
}

void writeInventoryContent(io::ByteBuffer& buffer, const std::uint32_t containerId,
                           const std::span<const BedrockItemStack> items, const BedrockContainerSlot slot,
                           const int protocol) {
    writeVarUInt(buffer, containerId);
    writeVarUInt(buffer, static_cast<std::uint32_t>(items.size()));
    for (const BedrockItemStack& item : items) {
        writeItemStack(buffer, item, protocol);
    }
    buffer.writeByte(static_cast<std::uint8_t>(slot));
    writeBool(buffer, false);                             // No dynamic id: this window is one the client already knows.
    writeItemStack(buffer, BedrockItemStack{}, protocol); // Nothing stored beside it.
}

BedrockLevelEffect bedrockLevelEffect(const std::int32_t javaEvent, const std::int32_t data) {
    // parity: the nine `LevelEvent`s this server raises, each one read out of Geyser's own switch
    // and the codec's event table. The particle numbers are 2000 plus their place in that table.
    switch (javaEvent) {
    case 1001: // A dispenser with nothing in it: a click and no particles.
        return {.sound = "block.dispenser.fail"};
    case 1050: // A crafter that was asked for something that is not a recipe.
        return {.sound = "block.crafter.fail"};
    case 1500: // A composter: the sparkles always, and which sound depends on whether it filled.
        return {.event = 2005, .sound = data != 0 ? "block.composter.fill_success" : "block.composter.fill"};
    case 1502: // A redstone torch burning out, which that edition draws from the block's corner.
        return {.event = 2016, .sound = "block.redstone_torch.burnout", .x = 0.0F, .y = 0.5F, .z = 0.0F};
    case 1505: // Bone meal: the number is how many sparkles, and it means the same over there.
        return {.event = 2005, .keepsData = true};
    case 3002: // The sparks off a lightning rod. The axis Java sends means nothing over there.
        return {.event = 2033};
    case 3003:
        return {.event = 2030, .sound = "item.honeycomb.wax_on"};
    case 3004:
        return {.event = 2031, .sound = "item.axe.wax_off"};
    case 3005:
        return {.event = 2032, .sound = "item.axe.scrape"};
    default:
        return {};
    }
}

void writeLevelEvent(io::ByteBuffer& buffer, const std::int32_t event, const float x, const float y, const float z,
                     const std::int32_t data) {
    writeVarInt(buffer, event);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeVarInt(buffer, data);
}

void writeBlockEvent(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                     const std::int32_t b0, const std::int32_t b1) {
    writeBlockPosition(buffer, x, y, z);
    writeVarInt(buffer, b0);
    writeVarInt(buffer, b1);
}

void writeContainerOpen(io::ByteBuffer& buffer, const std::int32_t containerId, const BedrockContainerType type,
                        const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    buffer.writeByte(static_cast<std::uint8_t>(containerId));
    buffer.writeByte(static_cast<std::uint8_t>(type));
    writeBlockPosition(buffer, x, y, z);
    writeVarLong(buffer, -1); // No entity behind it: every window this server opens is a block's.
}

void writeContainerSetData(io::ByteBuffer& buffer, const std::int32_t containerId, const BedrockContainerData property,
                           const std::int32_t value) {
    buffer.writeByte(static_cast<std::uint8_t>(containerId));
    writeVarInt(buffer, static_cast<std::int32_t>(property));
    writeVarInt(buffer, value);
}

void writeContainerClose(io::ByteBuffer& buffer, const std::int32_t containerId, const BedrockContainerType type,
                         const bool serverInitiated) {
    buffer.writeByte(static_cast<std::uint8_t>(containerId));
    buffer.writeByte(static_cast<std::uint8_t>(type));
    writeBool(buffer, serverInitiated);
}

std::optional<BedrockContainerClose> readContainerClose(io::ByteBuffer& buffer) {
    BedrockContainerClose close;
    close.containerId = static_cast<std::int8_t>(buffer.readByte());
    static_cast<void>(buffer.readByte()); // Which kind it was, which this server already knows.
    close.serverInitiated = buffer.readByte() != 0;
    if (buffer.failed()) {
        return std::nullopt;
    }
    return close;
}

void writeItemStackResponse(io::ByteBuffer& buffer, const std::int32_t requestId, const bool accepted,
                            const std::span<const BedrockResponseContainer> containers, const int protocol) {
    const bool native = writesNativeShapes(protocol);
    writeVarUInt(buffer, 1); // One answer, because one request is answered at a time.
    buffer.writeByte(accepted ? 0 : 1);
    writeVarInt(buffer, requestId);
    if (native) {
        writeBool(buffer, true);
        if (containers.empty()) {
            writeBool(buffer, false);
            return;
        }
        writeBool(buffer, true);
    } else if (!accepted) {
        // Below 2168 a refused request says nothing more: the client puts everything back itself.
        return;
    }

    writeVarUInt(buffer, static_cast<std::uint32_t>(containers.size()));
    for (const BedrockResponseContainer& container : containers) {
        buffer.writeByte(static_cast<std::uint8_t>(container.container));
        writeBool(buffer, false); // No id of its own: none of these windows is a bundle.
        writeVarUInt(buffer, static_cast<std::uint32_t>(container.slots.size()));
        for (const BedrockResponseSlot& slot : container.slots) {
            buffer.writeByte(static_cast<std::uint8_t>(slot.slot));
            buffer.writeByte(static_cast<std::uint8_t>(slot.slot)); // The same slot, as the hotbar numbers it.
            buffer.writeByte(static_cast<std::uint8_t>(slot.count));
            if (native) {
                writeBool(buffer, true);
                writeBool(buffer, slot.stackNetworkId > 0);
                if (slot.stackNetworkId > 0) {
                    writeVarInt(buffer, slot.stackNetworkId);
                }
                writeString(buffer, "");
                writeBool(buffer, false); // Nothing filtered out of the name,
            } else {
                writeVarInt(buffer, slot.stackNetworkId);
                writeString(buffer, "");
                writeString(buffer, "");
            }
            writeVarInt(buffer, 0); // and nothing worn off it.
        }
    }
}

void writeMobEffect(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockEffectEvent event,
                    const std::int32_t effectId, const std::int32_t amplifier, const bool particles,
                    const std::int32_t durationTicks, const bool ambient) {
    writeVarULong(buffer, runtimeEntityId);
    buffer.writeByte(static_cast<std::uint8_t>(event));
    writeVarInt(buffer, effectId);
    writeVarInt(buffer, amplifier);
    writeBool(buffer, particles);
    writeVarInt(buffer, durationTicks);
    writeVarULong(buffer, 0); // Which tick it started on, which only a replay cares about.
    writeBool(buffer, ambient);
}

void writePlaySound(io::ByteBuffer& buffer, const std::string_view sound, const double x, const double y,
                    const double z, const float volume, const float pitch, const int protocol) {
    writeString(buffer, sound);
    writeBlockPosition(buffer, static_cast<std::int32_t>(x * 8.0), static_cast<std::int32_t>(y * 8.0),
                       static_cast<std::int32_t>(z * 8.0));
    writeFloatLE(buffer, volume);
    writeFloatLE(buffer, pitch);
    if (writesNativeShapes(protocol)) {
        writeVarUInt(buffer, 0); // Played once, not looped.
    }
    // 26.20 and up let the server name a sound it holds itself. This one never does.
    if (protocol >= kServerSoundHandleProtocol) {
        writeBool(buffer, false);
    }
}

void writeUpdateBlock(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                      const std::int32_t javaState, const int protocol) {
    writeBlockPosition(buffer, x, y, z);
    writeVarUInt(buffer, bedrockBlockId(javaState, protocol));
    // Tell the neighbours, and say it came over the network: the first is what makes a fence rejoin
    // its posts, the second what stops the client believing it did this itself.
    writeVarUInt(buffer, 0b11);
    writeVarUInt(buffer, 0); // The layer of blocks, not the one water sits in.
}

void writeLevelChunk(io::ByteBuffer& buffer, const std::int32_t chunkX, const std::int32_t chunkZ,
                     const std::uint32_t count, const std::span<const std::uint8_t> subChunks,
                     const std::uint32_t biomeSections, const std::int32_t dimension, const int protocol) {
    writeVarInt(buffer, chunkX);
    writeVarInt(buffer, chunkZ);
    writeVarInt(buffer, dimension);
    writeVarUInt(buffer, count);
    if (writesNativeShapes(protocol)) {
        writeBool(buffer, false); // Not asking for subchunks one at a time.
        writeBool(buffer, false); // No blob cache.
        writeVarUInt(buffer, 0);  // No blobs.
    } else {
        writeBool(buffer, false); // No blob cache.
    }

    io::ByteBuffer payload{subChunks.size() + biomeSections + 8};
    payload.writeBytes(subChunks);
    // One biome section written out and the rest told to carry it forward, which is what an empty
    // one costs: the blocks are the point here, not the biomes.
    payload.writeByte(0x01);
    writeVarInt(payload, 0);
    for (std::uint32_t section = 1; section < biomeSections; ++section) {
        payload.writeByte(static_cast<std::uint8_t>((127U << 1U) | 1U));
    }
    payload.writeByte(0); // No border blocks.

    writeVarUInt(buffer, static_cast<std::uint32_t>(payload.readableBytes()));
    buffer.writeBytes(payload.readable());
}

void writeEmptyLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t subChunkCount,
                          std::int32_t dimension, const int protocol) {
    writeVarInt(buffer, chunkX);
    writeVarInt(buffer, chunkZ);
    writeVarInt(buffer, dimension);
    writeVarUInt(buffer, 0); // No block sections: the column is air.
    // 2168 always writes both the "how many subchunks at a time" question and the blob count; below
    // it the first is not asked at all and the second only when the cache is on.
    if (writesNativeShapes(protocol)) {
        writeBool(buffer, false); // Not asking for subchunks one at a time.
        writeBool(buffer, false); // No blob cache.
        writeVarUInt(buffer, 0);  // No blobs.
    } else {
        writeBool(buffer, false); // No blob cache.
    }

    io::ByteBuffer payload{static_cast<std::size_t>(subChunkCount) + 4};
    // One biome section written out, and the rest told to carry the previous one forward. The first
    // is a palette of a single entry, which is what zero bits per block means.
    payload.writeByte(0x01);
    writeVarInt(payload, 0);
    for (std::uint32_t section = 1; section < subChunkCount; ++section) {
        payload.writeByte(static_cast<std::uint8_t>((127U << 1U) | 1U));
    }
    payload.writeByte(0); // No border blocks.

    writeVarUInt(buffer, static_cast<std::uint32_t>(payload.readableBytes()));
    buffer.writeBytes(payload.readable());
}

namespace {

/// An image, as a skin carries one: its size and then the pixels, with nothing in between.
void writeImage(io::ByteBuffer& buffer, const std::uint32_t width, const std::uint32_t height,
                const std::span<const std::uint8_t> pixels) {
    writeIntLE(buffer, static_cast<std::int32_t>(width));
    writeIntLE(buffer, static_cast<std::int32_t>(height));
    writeVarUInt(buffer, static_cast<std::uint32_t>(pixels.size()));
    buffer.writeBytes(pixels);
}

void writeSkin(io::ByteBuffer& buffer, const SkinInfo& skin, const int protocol) {
    if (!writesNativeShapes(protocol)) {
        // The shape below 2168: every count is a little-endian int rather than a varint, the arm
        // size and the colour are written as words, and there is no "trusted" or profile hash.
        writeString(buffer, skin.skinId);
        writeString(buffer, ""); // No PlayFab account behind it.
        writeString(buffer, skin.resourcePatch);
        writeImage(buffer, skin.width, skin.height, skin.image);
        writeIntLE(buffer, 0); // No animations.
        writeImage(buffer, 0, 0, {});
        writeString(buffer, ""); // The geometry the patch names is one the client already has.
        writeString(buffer, "");
        writeString(buffer, ""); // Nothing animated.
        writeString(buffer, ""); // No cape.
        writeString(buffer, skin.skinId);
        writeString(buffer, "wide");
        writeString(buffer, "#0"); // The colour of a persona, which this is not.
        writeIntLE(buffer, 0);     // Not a persona, so no pieces
        writeIntLE(buffer, 0);     // and no tints.
        writeBool(buffer, false);  // Not bought,
        writeBool(buffer, false);  // not a persona,
        writeBool(buffer, false);  // no cape over the old model,
        writeBool(buffer, true);   // and it is this player's own.
        writeBool(buffer, false);  // It does not override what the player chose, because there is none.
        return;
    }

    writeString(buffer, skin.skinId);
    writeString(buffer, ""); // No PlayFab account behind it.
    writeString(buffer, skin.resourcePatch);
    writeImage(buffer, skin.width, skin.height, skin.image);
    writeVarUInt(buffer, 0); // No animations.
    writeImage(buffer, 0, 0, {});
    writeString(buffer, ""); // The geometry the patch names is one the client already has.
    writeString(buffer, "");
    writeString(buffer, ""); // Nothing animated.
    writeString(buffer, ""); // No cape.
    writeString(buffer, skin.skinId);
    buffer.writeByte(1); // Arms the wide way round, which is the default body.
    writeIntLE(buffer, 0);
    writeVarUInt(buffer, 0);  // Not a persona, so no pieces
    writeVarUInt(buffer, 0);  // and no tints.
    writeBool(buffer, false); // Not bought,
    writeBool(buffer, false); // not a persona,
    writeBool(buffer, false); // no cape over the old model,
    writeBool(buffer, true);  // and it is this player's own.
    writeBool(buffer, false); // It does not override what the player chose, because there is none.
    writeString(buffer, "true");
    writeString(buffer, "");
}

/// A turn as the byte a movement packet has room for: a full circle over the whole range.
void writeByteAngle(io::ByteBuffer& buffer, const float degrees) {
    buffer.writeByte(static_cast<std::uint8_t>(static_cast<std::int8_t>(degrees / (360.0F / 256.0F))));
}

/// Nothing in a hand. Below 2168 air is the one item that is only its id, so the count, the damage
/// and everything after it are not written at all; from there it is the descriptor shape, which
/// writes every field even for a stack of nothing.
void writeAirItem(io::ByteBuffer& buffer, const int protocol) {
    if (protocol >= kHeldItemDescriptorProtocol) {
        writeItemStack(buffer, BedrockItemStack{}, protocol);
        return;
    }
    writeVarInt(buffer, 0);
}

} // namespace

namespace {

void writeMetadataHead(io::ByteBuffer& buffer, const EntityDataId id, const EntityDataFormat format,
                       const int protocol) {
    writeVarUInt(buffer, static_cast<std::uint32_t>(id));
    writeVarUInt(buffer, static_cast<std::uint32_t>(format));
    // 2168 says the format twice and refuses a field where the two disagree. Below it, once.
    if (writesNativeShapes(protocol)) {
        buffer.writeByte(static_cast<std::uint8_t>(format));
    }
}

} // namespace

void writeMetadataLong(io::ByteBuffer& buffer, const EntityDataId id, const std::int64_t value, const int protocol) {
    writeMetadataHead(buffer, id, EntityDataFormat::Long, protocol);
    writeVarLong(buffer, value);
}

void writeMetadataShort(io::ByteBuffer& buffer, const EntityDataId id, const std::int16_t value, const int protocol) {
    writeMetadataHead(buffer, id, EntityDataFormat::Short, protocol);
    writeShortLE(buffer, static_cast<std::uint16_t>(value));
}

void writeMetadataFloat(io::ByteBuffer& buffer, const EntityDataId id, const float value, const int protocol) {
    writeMetadataHead(buffer, id, EntityDataFormat::Float, protocol);
    writeFloatLE(buffer, value);
}

void writeMetadataString(io::ByteBuffer& buffer, const EntityDataId id, const std::string_view value,
                         const int protocol) {
    writeMetadataHead(buffer, id, EntityDataFormat::String, protocol);
    writeString(buffer, value);
}

SkinInfo flatSkin(const std::string_view name) {
    // The colour is the name's own hash, so a player keeps theirs and two players rarely share one.
    std::uint32_t hash = 2166136261U;
    for (const char letter : name) {
        hash = (hash ^ static_cast<std::uint8_t>(letter)) * 16777619U;
    }

    SkinInfo skin;
    skin.skinId = "basalt-flat-" + std::to_string(hash);
    skin.image.resize(static_cast<std::size_t>(skin.width) * skin.height * 4);
    for (std::size_t at = 0; at < skin.image.size(); at += 4) {
        skin.image[at] = static_cast<std::uint8_t>(hash >> 16U);
        skin.image[at + 1] = static_cast<std::uint8_t>(hash >> 8U);
        skin.image[at + 2] = static_cast<std::uint8_t>(hash);
        skin.image[at + 3] = 0xFF;
    }
    return skin;
}

void writePlayerListAdd(io::ByteBuffer& buffer, const std::span<const PlayerListEntry> entries, const int protocol) {
    // Below 2168 the whole packet is one action, said once and in front: nought is arriving.
    if (!writesNativeShapes(protocol)) {
        buffer.writeByte(0);
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(entries.size()));
    for (const PlayerListEntry& entry : entries) {
        // Since 2168 every line says for itself whether it is arriving or leaving, twice: once as
        // a number the client branches on and once as the action's own place in its list.
        if (writesNativeShapes(protocol)) {
            writeVarUInt(buffer, 1);
            buffer.writeByte(0);
        }
        writeUuid(buffer, entry.uuidHigh, entry.uuidLow);
        writeVarLong(buffer, entry.entityId);
        writeString(buffer, entry.name);
        writeString(buffer, entry.xuid);
        writeString(buffer, ""); // No platform chat of its own.
        writeIntLE(buffer, 0);   // Which device they play on, which this server does not ask.
        writeSkin(buffer, entry.skin, protocol);
        writeBool(buffer, false); // Not a teacher,
        writeBool(buffer, false); // not the host
        writeBool(buffer, false); // and not somebody's second player on one console.
        writeIntLE(buffer, 0);    // What colour their name is drawn in, which 800 added.
    }
    // Below 2168 whether a skin is to be trusted is not part of the line: it is a run of booleans
    // behind all of them, one each.
    if (!writesNativeShapes(protocol)) {
        for (std::size_t at = 0; at < entries.size(); ++at) {
            writeBool(buffer, true);
        }
    }
}

void writePlayerListRemove(io::ByteBuffer& buffer, const std::span<const std::pair<std::uint64_t, std::uint64_t>> uuids,
                           const int protocol) {
    if (!writesNativeShapes(protocol)) {
        buffer.writeByte(1); // One is leaving, for the whole packet.
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(uuids.size()));
    for (const auto& [high, low] : uuids) {
        if (writesNativeShapes(protocol)) {
            writeVarUInt(buffer, 0);
            buffer.writeByte(1);
        }
        writeUuid(buffer, high, low);
    }
}

void writeAddPlayer(io::ByteBuffer& buffer, const AddPlayerInfo& info, const int protocol) {
    writeUuid(buffer, info.uuidHigh, info.uuidLow);
    writeString(buffer, info.name);
    writeVarULong(buffer, info.runtimeEntityId);
    writeString(buffer, ""); // No platform chat of its own.
    writeFloatLE(buffer, info.x);
    writeFloatLE(buffer, info.y);
    writeFloatLE(buffer, info.z);
    writeFloatLE(buffer, 0.0F); // Standing still as far as this packet says: the next move says more.
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, info.pitch);
    writeFloatLE(buffer, info.yaw);
    writeFloatLE(buffer, info.yaw);
    writeAirItem(buffer, protocol);
    writeVarInt(buffer, info.gameType);

    // The metadata a player needs before a client will draw one at all: how big they are, what
    // they are called, and the flags that say they fall, collide and breathe like everything else.
    writeVarUInt(buffer, 5);
    writeMetadataLong(buffer, EntityDataId::Flags,
                      flagBit(EntityFlag::HasCollision) | flagBit(EntityFlag::HasGravity) |
                          flagBit(EntityFlag::Breathing) | flagBit(EntityFlag::CanShowName),
                      protocol);
    writeMetadataString(buffer, EntityDataId::Name, info.name, protocol);
    writeMetadataFloat(buffer, EntityDataId::Scale, 1.0F, protocol);
    writeMetadataFloat(buffer, EntityDataId::Width, 0.6F, protocol);
    writeMetadataFloat(buffer, EntityDataId::Height, 1.8F, protocol);

    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);

    // The abilities a player has here, which is the plain set: no layer, and the permissions of
    // somebody who is neither an operator nor locked down.
    writeLongLE(buffer, info.entityId);
    buffer.writeByte(1); // A member, in the client's own order of permissions.
    buffer.writeByte(0); // With no command permission of its own.
    writeVarUInt(buffer, 0);

    writeVarUInt(buffer, 0); // Riding nothing and ridden by nobody.
    writeString(buffer, ""); // No device id.
    writeIntLE(buffer, 0);   // Nor a device.
}

void writeMoveEntity(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const float x, const float y,
                     const float z, const float yaw, const float pitch, const bool onGround) {
    writeVarULong(buffer, runtimeEntityId);
    buffer.writeByte(onGround ? 0x01 : 0x00);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeByteAngle(buffer, pitch);
    writeByteAngle(buffer, yaw);
    writeByteAngle(buffer, yaw);
}

void writeRemoveEntity(io::ByteBuffer& buffer, const std::int64_t entityId) { writeVarLong(buffer, entityId); }

namespace {

/// What a parameter is, as the symbol that carries it: the type's own number with the bit that says
/// the number means a type at all rather than an index into the enums.
constexpr std::int32_t kArgValid = 0x100000;

/// The rest of the line, selectors and all. It is what `/say` and `/tell` declare, and the nearest
/// thing Bedrock has to "whatever the server wants to parse".
constexpr std::int32_t kArgMessage = 67;

} // namespace

void writeChangeDimension(io::ByteBuffer& buffer, const BedrockDimension dimension, const float x, const float y,
                          const float z, const bool respawn) {
    writeVarInt(buffer, static_cast<std::int32_t>(dimension));
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeBool(buffer, respawn);
    writeBool(buffer, false); // No loading screen of its own.
}

void writeDeathInfo(io::ByteBuffer& buffer, const std::string_view cause) {
    writeString(buffer, cause);
    writeVarUInt(buffer, 0); // No further lines: the cause is the whole sentence.
}

void writeRespawn(io::ByteBuffer& buffer, const float x, const float y, const float z, const RespawnState state,
                  const std::uint64_t runtimeEntityId) {
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    buffer.writeByte(static_cast<std::uint8_t>(state));
    writeVarULong(buffer, runtimeEntityId);
}

std::optional<RespawnState> readRespawn(io::ByteBuffer& buffer) {
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    const std::uint8_t state = buffer.readByte();
    if (buffer.failed() || state > static_cast<std::uint8_t>(RespawnState::ClientReady)) {
        return std::nullopt;
    }
    return static_cast<RespawnState>(state);
}

void writeAvailableCommands(io::ByteBuffer& buffer, const std::span<const CommandInfo> commands) {
    writeVarUInt(buffer, 0); // No enum values, because there are no enums,
    writeVarUInt(buffer, 0); // no subcommand words,
    writeVarUInt(buffer, 0); // no suffixes,
    writeVarUInt(buffer, 0); // no enums
    writeVarUInt(buffer, 0); // and no subcommands.

    writeVarUInt(buffer, static_cast<std::uint32_t>(commands.size()));
    for (const CommandInfo& command : commands) {
        writeString(buffer, command.name);
        writeString(buffer, command.description);
        writeShortLE(buffer, 0);    // No flags: it is not a cheat, not a test, not hidden.
        writeString(buffer, "any"); // Anybody the server told about it may run it.
        writeIntLE(buffer, -1);     // No aliases,
        writeVarUInt(buffer, 0);    // and no subcommands.
        writeVarUInt(buffer, 1);    // One way to call it:
        writeBool(buffer, false);   // which does not chain,
        writeVarUInt(buffer, 1);    // and takes one thing.
        writeString(buffer, "args");
        writeIntLE(buffer, kArgMessage | kArgValid);
        writeBool(buffer, true); // Optional, so the bare command is a complete line.
        buffer.writeByte(0);     // With none of the options a parameter can carry.
    }

    writeVarUInt(buffer, 0); // No soft enums to fill in later,
    writeVarUInt(buffer, 0); // and nothing constraining one.
}

void writeLevelEvent(io::ByteBuffer& buffer, const LevelEventType type, const float x, const float y, const float z,
                     const std::int32_t data) {
    writeVarInt(buffer, static_cast<std::int32_t>(type));
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeVarInt(buffer, data);
}

void writeUpdateAttributes(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId,
                           const std::span<const AttributeValue> attributes, const std::uint64_t tick) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, static_cast<std::uint32_t>(attributes.size()));
    for (const AttributeValue& attribute : attributes) {
        writeFloatLE(buffer, attribute.minimum);
        writeFloatLE(buffer, attribute.maximum);
        writeFloatLE(buffer, attribute.value);
        // The range it started with, which for everything here is the range it still has.
        writeFloatLE(buffer, attribute.minimum);
        writeFloatLE(buffer, attribute.maximum);
        writeFloatLE(buffer, attribute.defaultValue);
        writeString(buffer, attribute.name);
        writeVarUInt(buffer, 0); // Nothing is modifying it: the server sends the number it wants.
    }
    writeVarULong(buffer, tick);
}

void writeSubmersion(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::int16_t airSupply,
                     const std::uint64_t flags, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 2);
    writeMetadataLong(buffer, EntityDataId::Flags, static_cast<std::int64_t>(flags), protocol);
    writeMetadataShort(buffer, EntityDataId::AirSupply, airSupply, protocol);
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

} // namespace basalt::wire::bedrock
