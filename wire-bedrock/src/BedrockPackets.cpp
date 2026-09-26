#include <basalt/wire/bedrock/BedrockBiomes.hpp>
#include <basalt/wire/bedrock/BedrockBlocks.hpp>
#include <basalt/wire/bedrock/BedrockEntityIdentifiers.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>
#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/bedrock/BedrockNbt.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockSounds.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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

/// The skin out of the client's own description of itself. Everything is optional: a client that
/// sends none, or one this build cannot read, simply gets drawn in a flat colour.
void readSkin(std::string_view payload, LoginRequest& login) {
    // A skin is four bytes a pixel and nothing bigger than this is worth drawing.
    constexpr std::uint32_t kMaxSide = 256;

    const auto data = jsonString(payload, "SkinData");
    const auto width = jsonNumber(payload, "SkinImageWidth");
    const auto height = jsonNumber(payload, "SkinImageHeight");
    if (!data || !width || !height || *width <= 0.0 || *height <= 0.0 || *width > kMaxSide || *height > kMaxSide) {
        return;
    }
    const auto pixels = common::base64::decode(*data);
    if (!pixels) {
        return;
    }
    const auto across = static_cast<std::uint32_t>(*width);
    const auto down = static_cast<std::uint32_t>(*height);
    // The size has to match the pixels, or a client reads past the end of what it was handed.
    if (pixels->size() != static_cast<std::size_t>(across) * down * 4) {
        return;
    }
    login.skinWidth = across;
    login.skinHeight = down;
    login.skinPixels = *pixels;
    if (const auto id = jsonString(payload, "SkinId"); id) {
        login.skinId = *id;
    }
    if (const auto geometry = jsonString(payload, "SkinResourcePatch"); geometry) {
        if (const auto decoded = common::base64::decode(*geometry); decoded) {
            login.skinGeometry = std::string{decoded->begin(), decoded->end()};
        }
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
    // What the player looks like. The client signs its own description of itself, so nothing here
    // is trusted beyond being drawn: a skin is pixels, and the worst a bad one does is look wrong.
    const std::span<const std::uint8_t> clientBytes = buffer.readSpan(clientLength);
    if (const auto parts =
            decodeJwt(std::string_view{reinterpret_cast<const char*>(clientBytes.data()), clientBytes.size()});
        parts) {
        readSkin(parts->payload, login);
    }

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
    writeVarInt(buffer, 0); // No rewind history: this server does not rewind.
    // Not because this server decides how fast a block comes apart, which it cannot: it is what
    // makes the client put a position on every breaking action it sends. Left off, the one action
    // that says a block is gone arrives with no coordinates at all.
    writeBool(buffer, true);
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
constexpr std::int32_t kFlagSprinting = 20;
constexpr std::int32_t kFlagVerticalCollision = 50;
constexpr std::int32_t kFlagItemInteraction = 34;
constexpr std::int32_t kFlagBlockActions = 35;
constexpr std::int32_t kFlagItemStackRequest = 36;
constexpr std::int32_t kFlagStartFlying = 42;
constexpr std::int32_t kFlagStopFlying = 43;

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

/// Whether an optional field follows, said with `flags` booleans in a row. The reading stops at the
/// first one that is off, the way the client writes them.
[[nodiscard]] bool presentFlags(io::ByteBuffer& buffer, const int flags) {
    for (int at = 0; at < flags; ++at) {
        if (buffer.readByte() == 0) {
            return false;
        }
    }
    return true;
}

/// Where a stack came from. Unflagged (`flags` zero), three of the kinds carry a number of their
/// own; flagged, a window and what the world did are both always announced, with that many flags.
void readInventorySource(io::ByteBuffer& buffer, const int flags) {
    const std::uint32_t kind = readVarUInt(buffer);
    if (flags > 0) {
        if (presentFlags(buffer, flags)) {
            static_cast<void>(buffer.readByte()); // Which window,
        }
        if (presentFlags(buffer, flags)) {
            static_cast<void>(readVarUInt(buffer)); // or what the world did with it.
        }
        return;
    }
    if (kind == 0 || kind == 100 || kind == 99999) {
        static_cast<void>(readVarInt(buffer)); // Which window,
    } else if (kind == 2) {
        static_cast<void>(readVarUInt(buffer)); // or what the world did with it.
    }
}

/// The click itself. Everything before the block it names is walked past: this server does not let
/// a client tell it what moved where, only what was clicked.
[[nodiscard]] std::optional<BedrockItemUse> readItemUseTransaction(io::ByteBuffer& buffer, const int protocol) {
    // 2168 put a boolean in front of each of the two lists, two in front of the second, and turned
    // three of the small numbers from varints into single bytes. 26.50 dropped the pair again.
    const bool native = protocol >= kHeldItemDescriptorProtocol;
    const bool single = protocol >= kSingleFlagProtocol;
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
    if (!native || single || presentFlags(buffer, 2)) {
        const std::uint32_t actions = readVarUInt(buffer);
        for (std::uint32_t at = 0; at < actions && !buffer.failed(); ++at) {
            readInventorySource(buffer, single ? 1 : native ? 2 : 0);
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
    if (protocol >= kItemUseHandProtocol) {
        static_cast<void>(buffer.readByte()); // Which hand, which the held slot already says.
    }
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
    case RequestAction::CraftRecipe:
    case RequestAction::CraftRecipeAuto:
        return BedrockStackActionType::Craft;
    case RequestAction::CraftRecipeOptional:
        return BedrockStackActionType::CraftOptional;
    case RequestAction::CraftGrindstone:
        return BedrockStackActionType::CraftGrindstone;
    case RequestAction::BeaconPayment:
        return BedrockStackActionType::BeaconPayment;
    case RequestAction::CraftLoom:
        return BedrockStackActionType::CraftLoom;
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
            action.primaryEffect = readVarInt(buffer);
            action.secondaryEffect = readVarInt(buffer);
            break;
        case RequestAction::MineBlock:
            static_cast<void>(readVarInt(buffer)); // Which hotbar slot,
            static_cast<void>(readVarInt(buffer)); // how worn the client thinks the tool is,
            static_cast<void>(native ? readIntLE(buffer) : readVarInt(buffer)); // and which stack it was.
            break;
        case RequestAction::CraftRecipe:
            action.recipeNetId = static_cast<std::int32_t>(readVarUInt(buffer));
            action.crafts = std::int32_t{buffer.readByte()};
            break;
        case RequestAction::CraftCreative:
            static_cast<void>(readVarUInt(buffer));
            static_cast<void>(buffer.readByte());
            break;
        case RequestAction::CraftRecipeAuto: {
            action.recipeNetId = static_cast<std::int32_t>(readVarUInt(buffer));
            action.crafts = std::int32_t{buffer.readByte()};
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
            action.recipeNetId = static_cast<std::int32_t>(readVarUInt(buffer));
            action.textIndex = readIntLE(buffer);
            break;
        case RequestAction::CraftGrindstone:
            static_cast<void>(native ? readIntLE(buffer) : static_cast<std::int32_t>(readVarUInt(buffer)));
            static_cast<void>(buffer.readByte());
            static_cast<void>(readVarInt(buffer));
            break;
        case RequestAction::CraftLoom:
            action.pattern = std::string{bannerPatternOf(readString(buffer))};
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
        request.typed.push_back(readString(buffer));
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
        if (protocol >= kSingleFlagProtocol || (buffer.readByte() != 0)) {
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
    input.sprinting = flags.contains(kFlagSprinting);
    input.onGround = flags.contains(kFlagVerticalCollision);
    if (flags.contains(kFlagStartFlying)) {
        input.flying = true;
    } else if (flags.contains(kFlagStopFlying)) {
        input.flying = false;
    }
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
    // puts a pair of booleans in front of each, the first saying the field exists at all; 26.50
    // keeps one.
    const auto present = [&buffer, protocol](const bool flagged) {
        if (protocol < kInputFlagListProtocol) {
            return flagged;
        }
        return presentFlags(buffer, protocol >= kSingleFlagProtocol ? 1 : 2);
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

/// A little-endian NBT key, which is a short length and the name.
void writeNbtKey(io::ByteBuffer& buffer, std::uint8_t type, std::string_view name) {
    buffer.writeByte(type);
    writeShortLE(buffer, static_cast<std::uint16_t>(name.size()));
    buffer.writeBytes(std::span{reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
}

/// What is written on a stack and where it may be used, as the stream of its own the length in
/// front counts. Only a map carries a tag: the map it shows, in the three keys a client reads.
void writeItemUserData(io::ByteBuffer& buffer, const BedrockItemStack& item, bool blocking) {
    io::ByteBuffer data;
    if (item.mapId.has_value()) {
        writeShortLE(data, 0xFFFF); // A tag follows,
        data.writeByte(1);          // in the only version there is.
        writeNbtKey(data, 10, "");
        writeNbtKey(data, 4, "map_uuid");
        writeLongLE(data, *item.mapId);
        writeNbtKey(data, 3, "map_name_index");
        writeIntLE(data, static_cast<std::int32_t>(*item.mapId));
        writeNbtKey(data, 1, "map_display_players");
        data.writeByte(1);
        data.writeByte(0);
    } else {
        writeShortLE(data, 0);
    }
    writeIntLE(data, 0); // Nothing it may be placed on,
    writeIntLE(data, 0); // and nothing it may break.
    if (blocking) {
        writeLongLE(data, 0);
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(data.readableBytes()));
    buffer.writeBytes(data.readable());
}

/// The stack as a client of this protocol can draw it: an item it has no name for yet becomes its
/// look-alike.
[[nodiscard]] BedrockItemStack shownTo(BedrockItemStack item, const int protocol) {
    if (const ItemStandIn* standIn = bedrockItemStandIn(item.runtimeId - 1, protocol)) {
        item.runtimeId = standIn->standInPlace + 1;
        item.data = standIn->standInData;
    }
    return item;
}

/// One stack where it is named rather than moved: the creative menu and an entity's hand. It is
/// the window shape without the flag that says a server is counting stacks.
void writeItemInstance(io::ByteBuffer& buffer, const BedrockItemStack& named, const int protocol) {
    const BedrockItemStack item = shownTo(named, protocol);
    const bool air = item.runtimeId == 0 || item.count == 0;
    // Nothing at all used to be a single zero and nothing after it. 2168 gave air the same fields as
    // anything else, all of them empty, and a creative menu whose group icon is nothing was writing
    // one byte where the client counted six: everything past the icon read shifted.
    if (air && protocol < kAirItemInstanceGrewProtocol) {
        writeVarInt(buffer, 0);
        return;
    }
    writeVarInt(buffer, air ? 0 : item.runtimeId);
    writeShortLE(buffer, static_cast<std::uint16_t>(air ? 0 : item.count));
    writeVarUInt(buffer, air ? 0U : static_cast<std::uint32_t>(item.data));
    writeVarInt(buffer, air ? 0 : item.blockRuntimeId);
    if (air) {
        writeVarUInt(buffer, 0); // Nothing travels beside nothing.
        return;
    }
    // One item carries a field nothing else does: a shield says how long it has been raised. The
    // length in front counts it, so leaving it out of the one entry shifts every item after it.
    const std::span<const std::string_view> palette = bedrockItemPalette();
    const std::size_t at = static_cast<std::size_t>(item.runtimeId) - 1;
    const bool blocking = at < palette.size() && palette[at] == "minecraft:shield";

    writeItemUserData(buffer, item, blocking);
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

void writeCreativeContent(io::ByteBuffer& buffer, const int protocol, const std::size_t items) {
    // One group with everything in it. Vanilla sorts its creative menu into four tabs and a great
    // many groups, and that order is content rather than protocol: what the protocol needs is that
    // every item a player may take is in a group, and that the group exists.
    writeVarUInt(buffer, 1);
    // The `items` tab, which is the one a mixed bag belongs under. Not `all`, which is a word for
    // the whole menu rather than a tab a group can sit in: a client looking for the tab by that
    // number finds nothing there and stops reading.
    constexpr int kItemsTab = 4;
    if (protocol >= kCreativeCategoryByteProtocol) {
        buffer.writeByte(kItemsTab);
    } else {
        writeIntLE(buffer, kItemsTab);
    }
    writeString(buffer, "itemGroup.name.basalt");
    writeItemInstance(buffer, BedrockItemStack{}, protocol);

    const std::span<const std::string_view> palette = bedrockItemPalette();
    const std::size_t offered = std::min(items, palette.size());
    const auto known = [protocol](const std::size_t at) {
        return bedrockItemStandIn(static_cast<std::int32_t>(at), protocol) == nullptr;
    };
    std::uint32_t shown = 0;
    for (std::size_t at = 0; at < offered; ++at) {
        shown += known(at) ? 1 : 0;
    }
    writeVarUInt(buffer, shown);
    std::uint32_t netId = 1; // Zero is "no stack", so the menu counts from one.
    for (std::size_t at = 0; at < offered; ++at) {
        if (!known(at)) {
            continue;
        }
        writeVarUInt(buffer, netId++);
        writeItemInstance(buffer, BedrockItemStack{.runtimeId = static_cast<std::int32_t>(at) + 1, .count = 1},
                          protocol);
        writeVarUInt(buffer, 0); // All of them in the one group.
    }
}

void writeGameRulesChanged(io::ByteBuffer& buffer, const int protocol) {
    /// What a rule's value is: a flag, a whole number, or a fraction.
    constexpr std::uint32_t kBool = 1;
    constexpr std::uint32_t kInt = 2;

    const bool editable = writesNativeShapes(protocol);
    const auto flag = [&buffer, editable](const std::string_view name, const bool value) {
        writeString(buffer, name);
        if (editable) {
            writeBool(buffer, false); // Not the player's to change: the server decides all of these.
        }
        writeVarUInt(buffer, kBool);
        writeBool(buffer, value);
    };
    const auto number = [&buffer, editable](const std::string_view name, const std::int32_t value) {
        writeString(buffer, name);
        if (editable) {
            writeBool(buffer, false);
        }
        writeVarUInt(buffer, kInt);
        // 844 turned this one from a varint into four bytes.
        if (editable) {
            writeIntLE(buffer, value);
        } else {
            writeVarUInt(buffer, static_cast<std::uint32_t>(value));
        }
    };

    writeVarUInt(buffer, 5);
    // Off, so the client never heals anybody by itself: hearts come from this server or not at all.
    flag("naturalregeneration", false);
    // On, so the client never empties a bag on death. What is kept is decided here.
    flag("keepinventory", true);
    // Recipes are unlocked by being shown, not by the client deciding it has earned them.
    flag("recipesunlock", true);
    // And nobody is moved around their spawn by the client's own idea of where that is.
    number("spawnradius", 0);
    // From 944 the bar is drawn only when something is on it, so it can be left on; below that it
    // is drawn whenever it is on, and is turned on and off as waypoints come and go.
    flag("locatorBar", protocol >= kLocatorBarProtocol);
}

namespace {

/// What an ingredient's kind is called on the wire. The client reads the word, not the number.
constexpr std::string_view kDescriptorByName = "name";
constexpr std::string_view kDescriptorByTag = "item_tag";

/// The aux value that means "any of them", which is what a tag and a damaged tool both ask for.
constexpr std::int32_t kAnyAux = 32767;

/// Which screen a recipe belongs to when it does not say: a table, or a hand, which are the same
/// word to that client.
constexpr std::string_view kCraftingTableTag = "crafting_table";

void writeIngredient(io::ByteBuffer& buffer, const BedrockIngredient& ingredient) {
    // An empty cell is the one kind that carries no name at all, and it says so with a nought
    // where every other kind says one.
    if (ingredient.name.empty()) {
        writeVarUInt(buffer, 0);
        writeVarInt(buffer, kAnyAux);
        writeVarInt(buffer, 0);
        return;
    }
    writeVarUInt(buffer, 1);
    writeString(buffer, ingredient.tag ? kDescriptorByTag : kDescriptorByName);
    writeString(buffer, ingredient.name);
    writeVarInt(buffer, ingredient.tag ? kAnyAux : ingredient.aux);
    writeVarInt(buffer, ingredient.count);
}

/// A stack as a recipe's result carries it, which is not the shape a window carries: the id is a
/// varint here and there is no net id in front of the block.
void writeItemInstance(io::ByteBuffer& buffer, const BedrockItemStack& item) {
    writeVarInt(buffer, item.runtimeId);
    writeShortLE(buffer, static_cast<std::uint16_t>(item.count));
    writeVarUInt(buffer, static_cast<std::uint32_t>(item.data));
    writeVarInt(buffer, item.blockRuntimeId);
    // What is written on it and where it may be used, all three empty, as a stream of its own.
    writeVarUInt(buffer, 10);
    writeShortLE(buffer, 0);
    writeIntLE(buffer, 0);
    writeIntLE(buffer, 0);
}

/// Nothing has to be unlocked before this recipe can be used, said the long way the packet asks
/// for: the context, then the optional list of what would unlock it.
void writeNoUnlocking(io::ByteBuffer& buffer) {
    writeVarInt(buffer, 0); // Unlocked by nothing in particular,
    writeBool(buffer, true);
    writeVarUInt(buffer, 0); // and there is no list of what it would take.
}

void writeRecipe(io::ByteBuffer& buffer, const BedrockRecipe& recipe, const std::uint32_t at) {
    writeString(buffer, recipe.id);
    if (recipe.shaped) {
        writeVarInt(buffer, recipe.width);
        writeVarInt(buffer, recipe.height);
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(recipe.ingredients.size()));
    for (const BedrockIngredient& ingredient : recipe.ingredients) {
        writeIngredient(buffer, ingredient);
    }
    writeVarUInt(buffer, 1); // One result, which is all this server makes.
    writeItemInstance(buffer, recipe.result);
    // Its own name as a number, which nothing here reads back: the string above is what a request
    // names. It has to be there and it has to differ, so the place in the list serves.
    writeUuid(buffer, 0, static_cast<std::uint64_t>(at) + 1);
    writeString(buffer, recipe.tag.empty() ? kCraftingTableTag : recipe.tag);
    writeVarInt(buffer, 0); // No priority over any other recipe.
    if (recipe.shaped) {
        writeBool(buffer, true); // Mirrored is the same recipe, which is what vanilla's are.
    }
    writeBool(buffer, true);
    writeNoUnlocking(buffer);
    writeVarUInt(buffer, static_cast<std::uint32_t>(recipe.netId));
}

} // namespace

namespace {

/// One item inside a trade. Empty when there is nothing there, which is how a trade with one cost
/// says so: the tag is present and holds nothing.
void writeTradeItem(io::ByteBuffer& buffer, const std::string_view name, const BedrockTradeItem& item) {
    nbt::beginNamed(buffer, name);
    if (!item.name.empty() && item.count > 0) {
        nbt::writeByte(buffer, "Count", static_cast<std::int8_t>(item.count));
        nbt::writeShort(buffer, "Damage", static_cast<std::int16_t>(item.damage));
        nbt::writeString(buffer, "Name", item.name);
    }
    nbt::end(buffer);
}

} // namespace

/// Every enchantment that edition has, in its own order: the number on the wire is the place in
/// this list. A protocol fact, the same as the block and item palettes.
constexpr auto kBedrockEnchantments = std::to_array<std::string_view>({"protection",
                                                                       "fire_protection",
                                                                       "feather_falling",
                                                                       "blast_protection",
                                                                       "projectile_protection",
                                                                       "thorns",
                                                                       "respiration",
                                                                       "depth_strider",
                                                                       "aqua_affinity",
                                                                       "sharpness",
                                                                       "smite",
                                                                       "bane_of_arthropods",
                                                                       "knockback",
                                                                       "fire_aspect",
                                                                       "looting",
                                                                       "efficiency",
                                                                       "silk_touch",
                                                                       "unbreaking",
                                                                       "fortune",
                                                                       "power",
                                                                       "punch",
                                                                       "flame",
                                                                       "infinity",
                                                                       "luck_of_the_sea",
                                                                       "lure",
                                                                       "frost_walker",
                                                                       "mending",
                                                                       "binding_curse",
                                                                       "vanishing_curse",
                                                                       "impaling",
                                                                       "riptide",
                                                                       "loyalty",
                                                                       "channeling",
                                                                       "multishot",
                                                                       "piercing",
                                                                       "quick_charge",
                                                                       "soul_speed",
                                                                       "swift_sneak",
                                                                       "wind_burst",
                                                                       "density",
                                                                       "breach",
                                                                       "lunge"});

std::optional<std::int32_t> bedrockEnchantment(std::string_view name) {
    if (name.starts_with("minecraft:")) {
        name.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    for (std::size_t at = 0; at < kBedrockEnchantments.size(); ++at) {
        if (kBedrockEnchantments[at] == name) {
            return static_cast<std::int32_t>(at);
        }
    }
    return std::nullopt;
}

void writePlayerEnchantOptions(io::ByteBuffer& buffer, const std::span<const BedrockEnchantOption> options,
                               const int protocol) {
    writeVarUInt(buffer, static_cast<std::uint32_t>(options.size()));
    for (const BedrockEnchantOption& option : options) {
        if (protocol >= kEnchantCostByteProtocol) {
            buffer.writeByte(static_cast<std::uint8_t>(std::clamp(option.cost, 0, 0xFF)));
        } else {
            writeVarUInt(buffer, static_cast<std::uint32_t>(std::max(option.cost, 0)));
        }
        writeIntLE(buffer, option.primarySlot);
        // Three lists: what a book would take, what this item takes, and what a curse would add.
        // Only the middle one is filled, which is the one the row is drawn from.
        writeVarUInt(buffer, 0);
        writeVarUInt(buffer, static_cast<std::uint32_t>(option.enchants.size()));
        for (const BedrockEnchant& enchant : option.enchants) {
            writeVarUInt(buffer, static_cast<std::uint32_t>(enchant.type));
            buffer.writeByte(static_cast<std::uint8_t>(std::clamp(enchant.level, 0, 0xFF)));
        }
        writeVarUInt(buffer, 0);
        writeString(buffer, option.name);
        writeVarUInt(buffer, static_cast<std::uint32_t>(option.netId));
    }
}

void writeUpdateTrade(io::ByteBuffer& buffer, const std::int32_t containerId, const std::int32_t tier,
                      const std::int64_t traderId, const std::int64_t playerId, const std::string_view displayName,
                      const std::span<const BedrockTrade> trades) {
    /// What this edition calls a villager's window.
    constexpr std::uint8_t kTradeContainer = 15;

    buffer.writeByte(static_cast<std::uint8_t>(containerId));
    buffer.writeByte(kTradeContainer);
    writeVarInt(buffer, 0); // No slots of its own: the trades are the window.
    writeVarInt(buffer, tier);
    writeVarLong(buffer, traderId);
    writeVarLong(buffer, playerId);
    writeString(buffer, displayName);
    writeBool(buffer, true); // The newer of the two trading screens,
    writeBool(buffer, true); // and the one that counts what a trade is worth.

    nbt::beginCompound(buffer);
    nbt::beginCompoundList(buffer, "Recipes", static_cast<std::int32_t>(trades.size()));
    std::int32_t at = 0;
    for (const BedrockTrade& trade : trades) {
        nbt::writeInt(buffer, "netId", ++at);
        nbt::writeInt(buffer, "maxUses", trade.uses >= trade.maxUses ? 0 : trade.maxUses);
        nbt::writeInt(buffer, "traderExp", trade.experience);
        nbt::writeFloat(buffer, "priceMultiplierA", trade.priceMultiplier);
        nbt::writeFloat(buffer, "priceMultiplierB", 0.0F);
        writeTradeItem(buffer, "sell", trade.sell);
        nbt::writeInt(buffer, "buyCountA", trade.baseCountA);
        nbt::writeInt(buffer, "buyCountB", trade.baseCountB);
        nbt::writeInt(buffer, "demand", trade.demand);
        nbt::writeInt(buffer, "tier", trade.tier);
        writeTradeItem(buffer, "buyA", trade.costA);
        writeTradeItem(buffer, "buyB", trade.costB);
        nbt::writeInt(buffer, "uses", trade.uses);
        nbt::writeByte(buffer, "rewardExp", 1);
        nbt::end(buffer);
    }
    // What each rank costs, which the client draws the bar from. The numbers are the jar's own.
    constexpr std::array<std::int32_t, 5> kRankCosts{0, 10, 70, 150, 250};
    nbt::beginCompoundList(buffer, "TierExpRequirements", static_cast<std::int32_t>(kRankCosts.size()));
    for (std::size_t rank = 0; rank < kRankCosts.size(); ++rank) {
        nbt::writeInt(buffer, std::to_string(rank), kRankCosts[rank]);
        nbt::end(buffer);
    }
    nbt::end(buffer);
}

namespace {

/// Whether a client of this protocol knows every item a recipe names.
[[nodiscard]] bool knownTo(const BedrockRecipe& recipe, const int protocol) {
    if (bedrockItemStandIn(recipe.result.runtimeId - 1, protocol) != nullptr) {
        return false;
    }
    return std::ranges::all_of(recipe.ingredients, [protocol](const BedrockIngredient& ingredient) {
        return ingredient.tag || bedrockItemNameKnown(ingredient.name, protocol);
    });
}

} // namespace

void writeCraftingData(io::ByteBuffer& buffer, const std::span<const BedrockRecipe> recipes, const int protocol) {
    // Below 2168 the whole lot is one list with a kind in front of every entry. From there the
    // packet is eleven lists, one per kind, and this server fills two of them.
    if (!writesNativeShapes(protocol)) {
        constexpr int kKinds = 4;
        for (int at = 0; at < kKinds; ++at) {
            writeVarUInt(buffer, 0);
        }
        writeBool(buffer, true);
        return;
    }

    std::uint32_t shaped = 0;
    std::uint32_t shapeless = 0;
    for (const BedrockRecipe& recipe : recipes) {
        if (knownTo(recipe, protocol)) {
            (recipe.shaped ? shaped : shapeless)++;
        }
    }
    writeVarUInt(buffer, shaped);
    std::uint32_t at = 0;
    for (const BedrockRecipe& recipe : recipes) {
        if (recipe.shaped && knownTo(recipe, protocol)) {
            writeRecipe(buffer, recipe, at++);
        }
    }
    writeVarUInt(buffer, shapeless);
    for (const BedrockRecipe& recipe : recipes) {
        if (!recipe.shaped && knownTo(recipe, protocol)) {
            writeRecipe(buffer, recipe, at++);
        }
    }
    // The cartography table's recipes, which that edition names by these fixed identities.
    constexpr std::array<std::pair<std::uint64_t, std::uint64_t>, kCartographyRecipeCount> kCartography{{
        {0x8b36268c1829483cULL, 0xa0f1993b7156a8f2ULL},
        {0x442d85ed82724543ULL, 0xa6f1418f90ded05dULL},
        {0x98c84b38108546bdULL, 0xb1cedd38c159e6ccULL},
        {0x602234e4cac14353ULL, 0x8bb7b1ebff70024bULL},
    }};
    writeVarUInt(buffer, static_cast<std::uint32_t>(kCartography.size()));
    for (std::size_t index = 0; index < kCartography.size(); ++index) {
        writeUuid(buffer, kCartography[index].first, kCartography[index].second);
        writeVarUInt(buffer,
                     static_cast<std::uint32_t>(kFirstCartographyRecipeNetId + static_cast<std::int32_t>(index)));
    }
    // User shapeless, chemistry shapeless, chemistry shaped, smithing transform, smithing trim,
    // potion mixes, container mixes and material reducers: none of them yet.
    constexpr int kEmptyKinds = 8;
    for (int empty = 0; empty < kEmptyKinds; ++empty) {
        writeVarUInt(buffer, 0);
    }
    writeBool(buffer, true); // Replaces whatever the client had rather than adding to it.
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
void writeItemStack(io::ByteBuffer& buffer, const BedrockItemStack& named, const int protocol) {
    const BedrockItemStack item = shownTo(named, protocol);
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
    // Little-endian rather than varints, because it is a stream of its own inside the packet.
    writeItemUserData(buffer, item, false);
}

} // namespace

void writeItemComponent(io::ByteBuffer& buffer, const int protocol) {
    const std::span<const std::string_view> palette = bedrockItemPalette();
    const auto known = [protocol](const std::size_t at) {
        return bedrockItemStandIn(static_cast<std::int32_t>(at), protocol) == nullptr;
    };
    std::uint32_t declared = 0;
    for (std::size_t at = 0; at < palette.size(); ++at) {
        declared += known(at) ? 1 : 0;
    }
    writeVarUInt(buffer, declared);
    for (std::size_t at = 0; at < palette.size(); ++at) {
        if (!known(at)) {
            continue; // A name this client lacks is never declared to it: its look-alike goes instead.
        }
        writeString(buffer, palette[at]);
        writeShortLE(buffer, static_cast<std::uint16_t>(at + 1)); // Zero is air, which is not in the list.
        writeBool(buffer, false);                                 // Not driven by components of its own,
        // and the version a plain item the client already knows carries. Saying "none" here is what
        // a server says about an item it deliberately leaves undefined: the client then has no
        // number for it, and the creative menu naming that number is the packet it hangs up on.
        writeVarInt(buffer, 0);
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
    case 2001: // A block coming apart: the puff of its own texture, and the sound that goes with
               // it. Both editions happen to number this one the same, and the number it carries is
               // a block state, which they do not.
        return {.event = 2001, .keepsData = true, .translatesBlock = true};
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
        if (protocol < kSingleFlagProtocol) {
            writeBool(buffer, true);
        }
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
                if (protocol < kSingleFlagProtocol) {
                    writeBool(buffer, true);
                }
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
                    const double z, const float volume, const float pitch, const int protocol,
                    const std::optional<std::int64_t> soundHandle) {
    writeString(buffer, sound);
    writeBlockPosition(buffer, static_cast<std::int32_t>(x * 8.0), static_cast<std::int32_t>(y * 8.0),
                       static_cast<std::int32_t>(z * 8.0));
    writeFloatLE(buffer, volume);
    writeFloatLE(buffer, pitch);
    if (writesNativeShapes(protocol)) {
        writeVarUInt(buffer, 0); // Played once, not looped.
    }
    if (protocol >= kSoundPlaybackProtocol) {
        writeBool(buffer, false); // Heard only by those in range.
    }
    if (protocol >= kServerSoundHandleProtocol) {
        writeBool(buffer, soundHandle.has_value());
        if (soundHandle.has_value()) {
            writeLongLE(buffer, *soundHandle);
        }
    }
    if (protocol >= kSoundPlaybackProtocol) {
        writeBool(buffer, false); // Played from the start.
    }
}

void writeRecordStarted(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                        const std::int64_t soundHandle) {
    writeVarInt(buffer, x);
    writeVarInt(buffer, y);
    writeVarInt(buffer, z);
    writeLongLE(buffer, soundHandle);
}

void writeStopSoundHandle(io::ByteBuffer& buffer, const std::int64_t soundHandle) {
    /// Stop, volume, pitch, fade, seek, pause and resume, each naming its kind; a kind of zero in
    /// any slot but the first is that slot left unset.
    constexpr int kSlots = 7;

    writeLongLE(buffer, soundHandle);
    for (int slot = 0; slot < kSlots; ++slot) {
        writeVarUInt(buffer, 0);
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
                     const std::uint32_t biomeSections, const std::int32_t dimension, const int protocol,
                     const std::span<const std::uint8_t> blockEntities, const std::span<const std::uint8_t> biomes) {
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

    io::ByteBuffer payload{subChunks.size() + biomeSections + biomes.size() + blockEntities.size() + 8};
    payload.writeBytes(subChunks);
    if (!biomes.empty()) {
        payload.writeBytes(biomes);
    } else {
        // Without biomes, one section of the fallback carried up the whole column.
        payload.writeByte(0x01);
        writeVarInt(payload, kFallbackBedrockBiome);
        for (std::uint32_t section = 1; section < biomeSections; ++section) {
            payload.writeByte(static_cast<std::uint8_t>((127U << 1U) | 1U));
        }
    }
    payload.writeByte(0); // No border blocks.
    // And whatever the column holds that is more than a block: the client reads these until the
    // payload runs out, so they carry no count of their own.
    payload.writeBytes(blockEntities);

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
    writeVarInt(payload, kFallbackBedrockBiome);
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

void writeMetadataInt(io::ByteBuffer& buffer, const EntityDataId id, const std::int32_t value, const int protocol) {
    writeMetadataHead(buffer, id, EntityDataFormat::Int, protocol);
    writeVarInt(buffer, value);
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

void writeMetadataVector3i(io::ByteBuffer& buffer, const EntityDataId id, const std::int32_t x, const std::int32_t y,
                           const std::int32_t z, const int protocol) {
    writeMetadataHead(buffer, id, EntityDataFormat::Vector3i, protocol);
    writeVarInt(buffer, x);
    writeVarInt(buffer, y);
    writeVarInt(buffer, z);
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

void writeUpdateAbilities(io::ByteBuffer& buffer, const std::int64_t entityId, const bool operator_,
                          const bool creative) {
    // Which of these the layer decides, and which of them it says yes to. A client told nothing at
    // all cannot mine, build, open a door or hit anything, which looks from the inside like being
    // stuck in the air with the buttons doing nothing.
    constexpr std::uint32_t kBuild = 1U << 0U;
    constexpr std::uint32_t kMine = 1U << 1U;
    constexpr std::uint32_t kDoorsAndSwitches = 1U << 2U;
    constexpr std::uint32_t kOpenContainers = 1U << 3U;
    constexpr std::uint32_t kAttackPlayers = 1U << 4U;
    constexpr std::uint32_t kAttackMobs = 1U << 5U;
    constexpr std::uint32_t kOperatorCommands = 1U << 6U;
    constexpr std::uint32_t kTeleport = 1U << 7U;
    constexpr std::uint32_t kFlying = 1U << 9U;
    constexpr std::uint32_t kMayFly = 1U << 10U;
    constexpr std::uint32_t kInstantBuild = 1U << 11U;
    constexpr std::uint32_t kFlySpeedSet = 1U << 13U;
    constexpr std::uint32_t kWalkSpeedSet = 1U << 14U;

    constexpr std::uint32_t kDecided = kBuild | kMine | kDoorsAndSwitches | kOpenContainers | kAttackPlayers |
                                       kAttackMobs | kOperatorCommands | kTeleport | kFlying | kMayFly | kInstantBuild |
                                       kFlySpeedSet | kWalkSpeedSet;

    // The two speeds are in the layer's list but not switched on: the floats below are what says
    // how fast somebody moves, and a client that is also told the ability is on walks quicker than
    // the number it was given.
    std::uint32_t on = kBuild | kMine | kDoorsAndSwitches | kOpenContainers | kAttackPlayers | kAttackMobs;
    if (creative) {
        on |= kMayFly | kInstantBuild;
    }
    if (operator_) {
        on |= kOperatorCommands | kTeleport;
    }

    writeLongLE(buffer, entityId);
    buffer.writeByte(operator_ ? 2 : 1); // An operator or a member, in the client's own order.
    buffer.writeByte(operator_ ? 2 : 0); // And what it may run, in the same order.

    writeVarUInt(buffer, 1); // One layer,
    writeShortLE(buffer, 1); // the base one everything else is stacked on.
    writeIntLE(buffer, static_cast<std::int32_t>(kDecided));
    writeIntLE(buffer, static_cast<std::int32_t>(on));
    // The three speeds vanilla gives a player. 776 put the vertical one in the middle, so a layer
    // written with 534's two leaves the walking speed where the client looks for the climb.
    writeFloatLE(buffer, 0.05F);
    writeFloatLE(buffer, 1.0F);
    writeFloatLE(buffer, 0.1F);
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

/// An argument this server does not describe. Not `message`, which swallows the rest of the line
/// and confuses the client's suggestions while typing.
constexpr std::int32_t kArgString = 56;

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

std::optional<BedrockPlayerAction> readPlayerAction(io::ByteBuffer& buffer) {
    static_cast<void>(readVarULong(buffer)); // Whose action it is, which is always the sender's own.
    const std::int32_t action = readVarInt(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    switch (action) {
    case static_cast<std::int32_t>(BedrockPlayerAction::StopSleep):
    case static_cast<std::int32_t>(BedrockPlayerAction::Respawn):
    case static_cast<std::int32_t>(BedrockPlayerAction::DimensionChanged):
        return static_cast<BedrockPlayerAction>(action);
    default:
        return std::nullopt;
    }
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
        writeIntLE(buffer, kArgString | kArgValid);
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

namespace {

/// The fields every entity needs before a client will draw it where the server put it: with no
/// gravity and no collision it hangs in the air, and with no box it has no size to be drawn at.
void writeBodyMetadata(io::ByteBuffer& buffer, const float width, const float height, const std::uint64_t flags,
                       const int protocol) {
    writeVarUInt(buffer, 5);
    writeMetadataLong(buffer, EntityDataId::Flags, static_cast<std::int64_t>(flags), protocol);
    writeMetadataShort(buffer, EntityDataId::AirSupplyMax, 300, protocol);
    writeMetadataFloat(buffer, EntityDataId::Width, width, protocol);
    writeMetadataFloat(buffer, EntityDataId::Height, height, protocol);
    writeMetadataFloat(buffer, EntityDataId::Scale, 1.0F, protocol);
}

} // namespace

void writeAddItemEntity(io::ByteBuffer& buffer, const std::int64_t entityId, const BedrockItemStack& item,
                        const float x, const float y, const float z, const float motionX, const float motionY,
                        const float motionZ, const int protocol) {
    /// parity: `ItemEntity`'s own box, which is what decides how big the spinning stack is drawn.
    constexpr float kItemSize = 0.25F;

    writeVarLong(buffer, entityId);
    writeVarULong(buffer, static_cast<std::uint64_t>(entityId));
    writeItemStack(buffer, item, protocol);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeFloatLE(buffer, motionX);
    writeFloatLE(buffer, motionY);
    writeFloatLE(buffer, motionZ);
    writeBodyMetadata(buffer, kItemSize, kItemSize,
                      flagBit(EntityFlag::HasCollision) | flagBit(EntityFlag::HasGravity) |
                          flagBit(EntityFlag::Breathing) | flagBit(EntityFlag::CanShowName),
                      protocol);
    writeBool(buffer, false); // Nobody fished it out of anything.
}

void writeAddEntity(io::ByteBuffer& buffer, const std::int64_t entityId, const std::string_view identifier,
                    const float x, const float y, const float z, const float yaw, const float pitch, const float width,
                    const float height, const std::uint64_t flags, const int protocol) {
    writeVarLong(buffer, entityId);
    writeVarULong(buffer, static_cast<std::uint64_t>(entityId));
    writeString(buffer, identifier);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeFloatLE(buffer, 0.0F); // Standing still as far as this packet goes: the moves come after.
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, pitch);
    writeFloatLE(buffer, yaw);
    writeFloatLE(buffer, yaw); // Head,
    writeFloatLE(buffer, yaw); // and body, which for a standing mob are the same way.
    writeVarUInt(buffer, 0);   // No attributes: the client has its own for every vanilla entity.
    writeBodyMetadata(buffer, width, height, flags, protocol);
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0); // Riding nothing and ridden by nobody.
}

void writeAddBorderCollider(io::ByteBuffer& buffer, const std::int64_t entityId, const float x, const float y,
                            const float z, const int protocol) {
    /// Ten across and twenty-five tall, which is what Geyser settled on: wide enough that a player
    /// running along the border keeps meeting it, tall enough that jumping does not clear it.
    constexpr float kWidth = 10.0F;
    constexpr float kHeight = 25.0F;

    writeVarLong(buffer, entityId);
    writeVarULong(buffer, static_cast<std::uint64_t>(entityId));
    // An armour stand because it has to be something; nothing of it is ever drawn.
    writeString(buffer, "minecraft:armor_stand");
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeVarUInt(buffer, 0); // No attributes.
    writeVarUInt(buffer, 6);
    writeMetadataLong(buffer, EntityDataId::Flags,
                      static_cast<std::int64_t>(flagBit(EntityFlag::Invisible) | flagBit(EntityFlag::Silent) |
                                                flagBit(EntityFlag::NoAi)),
                      protocol);
    // Being walked into lives past the sixty-fourth flag, so it travels in the second word.
    writeMetadataLong(buffer, EntityDataId::Flags2, static_cast<std::int64_t>(flagBit2(EntityFlag::Collidable)),
                      protocol);
    writeMetadataShort(buffer, EntityDataId::AirSupplyMax, 300, protocol);
    writeMetadataFloat(buffer, EntityDataId::Width, kWidth, protocol);
    writeMetadataFloat(buffer, EntityDataId::Height, kHeight, protocol);
    writeMetadataFloat(buffer, EntityDataId::Scale, 1.0F, protocol);
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0); // Riding nothing and ridden by nobody.
}

void writeTakeItemEntity(io::ByteBuffer& buffer, const std::uint64_t itemRuntimeId,
                         const std::uint64_t collectorRuntimeId) {
    writeVarULong(buffer, itemRuntimeId);
    writeVarULong(buffer, collectorRuntimeId);
}

void writeAnimate(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockAnimation action) {
    buffer.writeByte(static_cast<std::uint8_t>(action));
    writeVarULong(buffer, runtimeEntityId);
    writeFloatLE(buffer, 0.0F); // Only a rowing oar carries a number here.
    writeBool(buffer, false);   // And only a swing made by something else names where it came from.
}

void writeMobEquipment(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockItemStack& item,
                       const std::int32_t hotbarSlot, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeItemStack(buffer, item, protocol);
    buffer.writeByte(static_cast<std::uint8_t>(hotbarSlot));
    buffer.writeByte(static_cast<std::uint8_t>(hotbarSlot));
    buffer.writeByte(0); // The bag itself, which is the only place a hand takes from.
}

void writeMobArmorEquipment(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockItemStack& head,
                            const BedrockItemStack& chest, const BedrockItemStack& legs, const BedrockItemStack& feet,
                            const BedrockItemStack& body, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeItemStack(buffer, head, protocol);
    writeItemStack(buffer, chest, protocol);
    writeItemStack(buffer, legs, protocol);
    writeItemStack(buffer, feet, protocol);
    writeItemStack(buffer, body, protocol);
}

void writeSetDisplayObjective(io::ByteBuffer& buffer, const std::string_view slot, const std::string_view objective,
                              const std::string_view title) {
    writeString(buffer, slot);
    writeString(buffer, objective);
    writeString(buffer, title);
    writeString(buffer, "dummy"); // What fills it in, which for a server-drawn board is nothing.
    writeVarInt(buffer, 0);       // Highest first, the way every sidebar is read.
}

void writeRemoveObjective(io::ByteBuffer& buffer, const std::string_view objective) { writeString(buffer, objective); }

void writeSetScore(io::ByteBuffer& buffer, const std::string_view objective,
                   const std::span<const BedrockScoreLine> lines, const int protocol) {
    /// A line of plain text is a player who does not exist, which is what this edition calls one.
    constexpr std::uint8_t kFakePlayer = 3;
    constexpr std::uint8_t kSet = 0;

    const std::string_view named = objective.empty() ? std::string_view{" "} : objective;
    if (protocol < kScoreKindProtocol) {
        buffer.writeByte(kSet);
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(lines.size()));
    for (const BedrockScoreLine& line : lines) {
        const std::string_view name = line.name.empty() ? std::string_view{" "} : std::string_view{line.name};
        if (protocol < kScoreKindProtocol) {
            writeVarLong(buffer, line.lineId);
            writeString(buffer, named);
            writeIntLE(buffer, line.score);
            buffer.writeByte(kFakePlayer);
            writeString(buffer, name);
            continue;
        }
        writeVarUInt(buffer, kFakePlayer);
        writeString(buffer, "changefakeplayer"); // The kind again, as a word, which the client also reads.
        writeVarLong(buffer, line.lineId);
        writeString(buffer, named);
        writeIntLE(buffer, line.score);
        writeString(buffer, name);
    }
}

void writeRemoveScores(io::ByteBuffer& buffer, const std::string_view objective,
                       const std::span<const std::int64_t> lineIds, const int protocol) {
    constexpr std::uint8_t kRemove = 1;
    /// A line being taken away is of no kind at all.
    constexpr std::int32_t kGone = 0;

    if (protocol < kScoreKindProtocol) {
        buffer.writeByte(kRemove);
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(lineIds.size()));
    for (const std::int64_t id : lineIds) {
        if (protocol < kScoreKindProtocol) {
            writeVarLong(buffer, id);
            writeString(buffer, objective);
            writeIntLE(buffer, 0);
            continue;
        }
        writeVarUInt(buffer, kGone);
        writeString(buffer, "remove");
        writeVarLong(buffer, id);
        // No objective: 26.44 wraps a present one in a second flag that 26.40 does not, and an
        // absent one is the same single byte to both.
        writeBool(buffer, false);
    }
}

void writeBossEvent(io::ByteBuffer& buffer, const std::int64_t bossEntityId, const BedrockBossAction action,
                    const std::string_view title, const float progress, const int protocol) {
    writeVarLong(buffer, bossEntityId);
    if (protocol < kBossBarPlayerlessProtocol) {
        writeVarLong(buffer, 0); // Which player it is for, which a server drawing one for everybody
                                 // leaves alone.
    }
    buffer.writeByte(static_cast<std::uint8_t>(action));
    writeString(buffer, title);
    writeString(buffer, ""); // The same title again, filtered, which this server does not filter.
    writeFloatLE(buffer, progress);
    buffer.writeByte(0); // Purple,
    buffer.writeByte(0); // and no notches, which is what a plain bar looks like.
}

void writeTransfer(io::ByteBuffer& buffer, const std::string_view address, const std::uint16_t port,
                   const int protocol) {
    writeString(buffer, address);
    writeShortLE(buffer, port);
    writeBool(buffer, true); // Loads the world again on arrival, which a different server needs.
    if (writesNativeShapes(protocol)) {
        writeBool(buffer, false); // Not joining anybody's gathering.
    }
}

void writeUpdateAdventureSettings(io::ByteBuffer& buffer, const bool immutableWorld) {
    writeBool(buffer, false); // Players may hit mobs,
    writeBool(buffer, false); // and mobs may hit players.
    writeBool(buffer, immutableWorld);
    writeBool(buffer, false); // Name tags are drawn by what the entities carry, not by this.
    writeBool(buffer, true);  // Stepping up a block without jumping, which is that edition's own.
}

void writeSignTag(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                  const std::string_view front, const std::string_view back, const bool waxed) {
    /// What the client calls a sign's own data, which it looks up by this name and no other.
    constexpr std::string_view kId = "Sign";

    nbt::beginCompound(buffer);
    nbt::writeString(buffer, "id", kId);
    nbt::writeInt(buffer, "x", x);
    nbt::writeInt(buffer, "y", y);
    nbt::writeInt(buffer, "z", z);
    nbt::writeByte(buffer, "isMovable", 1);
    nbt::writeByte(buffer, "IsWaxed", waxed ? 1 : 0);
    const auto face = [&buffer](const std::string_view name, const std::string_view text) {
        nbt::beginNamed(buffer, name);
        nbt::writeString(buffer, "Text", text);
        nbt::writeString(buffer, "TextOwner", "");
        nbt::writeInt(buffer, "SignTextColor", -16777216); // Black, which is what an unpainted sign is.
        nbt::writeByte(buffer, "IgnoreLighting", 0);
        nbt::writeByte(buffer, "PersistFormatting", 1);
        nbt::writeByte(buffer, "HideGlowOutline", 0);
        nbt::end(buffer);
    };
    face("FrontText", front);
    face("BackText", back);
    nbt::end(buffer);
}

void writeSignData(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                   const std::string_view front, const std::string_view back, const bool waxed) {
    writeBlockPosition(buffer, x, y, z);
    writeSignTag(buffer, x, y, z, front, back, waxed);
}

void writePlayerHotbar(io::ByteBuffer& buffer, const std::int32_t slot) {
    // The only container a hotbar slot can live in, and the one Java's packet never names.
    constexpr std::uint8_t kInventoryContainer = 0;

    writeVarUInt(buffer, static_cast<std::uint32_t>(slot));
    buffer.writeByte(kInventoryContainer);
    writeBool(buffer, true); // Select it, rather than only redraw the row.
}

void writeLevelSoundEvent(io::ByteBuffer& buffer, const std::int32_t event, const float x, const float y, const float z,
                          const int protocol) {
    // What a numbered sound this server sends carries past its own number: nothing at all.
    constexpr std::int32_t kNoExtraData = -1;

    writeVarUInt(buffer, static_cast<std::uint32_t>(event));
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeVarInt(buffer, kNoExtraData);
    writeString(buffer, "");  // No entity to name: these are the sounds a place makes.
    writeBool(buffer, false); // Not a young one's voice,
    writeBool(buffer, false); // and it gets quieter with distance like everything else.
    writeLongLE(buffer, 0);   // Nothing it belongs to, which 786 added.
    if (protocol >= kServerSoundHandleProtocol) {
        writeBool(buffer, false); // And nowhere for a fire to start, which 975 added.
    }
}

void writeItemFrameBlock(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                         const bool glowing, const int face, const int protocol) {
    writeBlockPosition(buffer, x, y, z);
    writeVarUInt(buffer, itemFrameBlockId(glowing, face, protocol));
    // Tell the neighbours, and say it came over the network rather than from the client itself.
    writeVarUInt(buffer, 0b11);
    writeVarUInt(buffer, 0); // The layer of blocks, not the one water sits in.
}

void writeItemFrameData(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                        const bool glowing, const std::string_view item, const std::int32_t count,
                        const std::int32_t rotation) {
    /// parity: the client looks a frame's data up by these two names and no other.
    constexpr std::string_view kPlain = "ItemFrame";
    constexpr std::string_view kGlowing = "GlowItemFrame";
    /// An eighth of a turn, which is what one step of the other edition's rotation comes to.
    constexpr float kStepDegrees = 45.0F;

    writeBlockPosition(buffer, x, y, z);
    nbt::beginCompound(buffer);
    nbt::writeString(buffer, "id", glowing ? kGlowing : kPlain);
    nbt::writeInt(buffer, "x", x);
    nbt::writeInt(buffer, "y", y);
    nbt::writeInt(buffer, "z", z);
    nbt::writeByte(buffer, "isMovable", 1);
    if (!item.empty()) {
        nbt::beginNamed(buffer, "Item");
        nbt::writeString(buffer, "Name", item);
        nbt::writeByte(buffer, "Count", static_cast<std::int8_t>(count));
        nbt::writeShort(buffer, "Damage", 0);
        nbt::end(buffer);
        nbt::writeFloat(buffer, "ItemDropChance", 1.0F);
        nbt::writeFloat(buffer, "ItemRotation", static_cast<float>(rotation) * kStepDegrees);
    }
    nbt::end(buffer);
}

void writeBannerTag(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                    const std::int32_t base, const std::span<const BedrockBannerLayer> layers, const bool ominous,
                    const std::string_view customName) {
    // What the client calls a banner's own data, which it looks up by this name and no other.
    constexpr std::string_view kId = "Banner";
    // The one banner that edition draws itself rather than from layers.
    constexpr std::int32_t kOminous = 1;

    nbt::beginCompound(buffer);
    nbt::writeString(buffer, "id", kId);
    nbt::writeInt(buffer, "x", x);
    nbt::writeInt(buffer, "y", y);
    nbt::writeInt(buffer, "z", z);
    nbt::writeByte(buffer, "isMovable", 1);
    nbt::writeInt(buffer, "Base", base);
    if (ominous) {
        nbt::writeInt(buffer, "Type", kOminous);
    } else if (!layers.empty()) {
        nbt::beginCompoundList(buffer, "Patterns", static_cast<std::int32_t>(layers.size()));
        for (const BedrockBannerLayer& layer : layers) {
            nbt::writeInt(buffer, "Color", layer.colour);
            nbt::writeString(buffer, "Pattern", layer.pattern);
            nbt::end(buffer);
        }
    }
    if (!customName.empty()) {
        nbt::writeString(buffer, "CustomName", customName);
    }
    nbt::end(buffer);
}

void writeBedTag(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                 const std::uint8_t colour) {
    // What the client calls a bed's own data, which it looks up by this name and no other.
    constexpr std::string_view kId = "Bed";

    nbt::beginCompound(buffer);
    nbt::writeString(buffer, "id", kId);
    nbt::writeInt(buffer, "x", x);
    nbt::writeInt(buffer, "y", y);
    nbt::writeInt(buffer, "z", z);
    nbt::writeByte(buffer, "isMovable", 1);
    nbt::writeByte(buffer, "color", static_cast<std::int8_t>(colour));
    nbt::end(buffer);
}

void writeBedData(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                  const std::uint8_t colour) {
    writeBlockPosition(buffer, x, y, z);
    writeBedTag(buffer, x, y, z, colour);
}

std::optional<BedrockSignEdit> readSignEdit(io::ByteBuffer& buffer) {
    constexpr std::array<std::string_view, 2> kFront{"FrontText", "Text"};

    BedrockSignEdit edit;
    edit.x = readVarInt(buffer);
    edit.y = static_cast<std::int32_t>(readVarUInt(buffer));
    edit.z = readVarInt(buffer);
    const std::optional<std::string> text = nbt::readStringAt(buffer, kFront);
    if (buffer.failed() || !text.has_value()) {
        return std::nullopt;
    }
    edit.text = *text;
    return edit;
}

std::optional<BedrockAnimation2> readAnimate(io::ByteBuffer& buffer) {
    BedrockAnimation2 animation;
    animation.action = readVarInt(buffer);
    animation.runtimeEntityId = readVarULong(buffer);
    return buffer.failed() ? std::nullopt : std::optional{animation};
}

std::optional<BedrockBlockPick> readBlockPickRequest(io::ByteBuffer& buffer) {
    BedrockBlockPick pick;
    pick.x = readVarInt(buffer);
    pick.y = readVarInt(buffer);
    pick.z = readVarInt(buffer);
    static_cast<void>(buffer.readByte()); // Whether the block's own data comes with it, which only
                                          // a creative client asks for and this server always gives.
    pick.hotbarSlot = std::int32_t{buffer.readByte()};
    return buffer.failed() ? std::nullopt : std::optional{pick};
}

std::optional<BedrockEntityPick> readEntityPickRequest(io::ByteBuffer& buffer) {
    BedrockEntityPick pick;
    pick.runtimeEntityId = static_cast<std::uint64_t>(readLongLE(buffer));
    pick.hotbarSlot = std::int32_t{buffer.readByte()};
    return buffer.failed() ? std::nullopt : std::optional{pick};
}

void writeSetSpawnPosition(io::ByteBuffer& buffer, const BedrockDimension dimension, const std::int32_t x,
                           const std::int32_t y, const std::int32_t z) {
    /// parity: `SetSpawnPositionPacket.Type.WORLD_SPAWN`, the one a server sets.
    constexpr std::int32_t kWorldSpawn = 1;

    writeVarInt(buffer, kWorldSpawn);
    writeBlockPosition(buffer, x, y, z);
    writeVarInt(buffer, static_cast<std::int32_t>(dimension));
    writeBlockPosition(buffer, x, y, z);
}

void writeStopSound(io::ByteBuffer& buffer, const std::string_view sound, const bool stopAll) {
    writeString(buffer, sound);
    writeBool(buffer, stopAll);
    writeBool(buffer, false); // Not the legacy music channel.
}

void writePlayerStartItemCooldown(io::ByteBuffer& buffer, const std::string_view category,
                                  const std::int32_t durationTicks) {
    writeString(buffer, category);
    writeVarInt(buffer, durationTicks);
}

std::optional<BedrockCredits> readShowCredits(io::ByteBuffer& buffer) {
    static_cast<void>(readVarULong(buffer)); // Whose poem it is, which is always the reader's own.
    const std::int32_t status = readVarInt(buffer);
    if (buffer.failed() || status < 0 || status > static_cast<std::int32_t>(BedrockCredits::Finished)) {
        return std::nullopt;
    }
    return static_cast<BedrockCredits>(status);
}

std::optional<BedrockEntityEventIn> readEntityEvent(io::ByteBuffer& buffer) {
    BedrockEntityEventIn event;
    event.runtimeEntityId = readVarULong(buffer);
    event.event = buffer.readByte();
    event.data = readVarInt(buffer);
    return buffer.failed() ? std::nullopt : std::optional{event};
}

std::optional<BedrockBookEdit> readBookEdit(io::ByteBuffer& buffer) {
    BedrockBookEdit edit;
    edit.slot = readVarInt(buffer);
    const std::uint32_t action = readVarUInt(buffer);
    if (action > static_cast<std::uint32_t>(BedrockBookEdit::Action::Sign)) {
        return std::nullopt;
    }
    edit.action = static_cast<BedrockBookEdit::Action>(action);
    switch (edit.action) {
    case BedrockBookEdit::Action::ReplacePage:
    case BedrockBookEdit::Action::AddPage:
        edit.page = readVarInt(buffer);
        edit.text = readString(buffer);
        static_cast<void>(readString(buffer)); // The photo on the page, which that edition dropped.
        break;
    case BedrockBookEdit::Action::DeletePage:
        edit.page = readVarInt(buffer);
        break;
    case BedrockBookEdit::Action::SwapPages:
        edit.page = readVarInt(buffer);
        edit.otherPage = readVarInt(buffer);
        break;
    case BedrockBookEdit::Action::Sign:
        edit.title = readString(buffer);
        edit.author = readString(buffer);
        static_cast<void>(readString(buffer)); // The signer's own account number, which nothing here reads.
        break;
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(edit)};
}

std::optional<std::string> readFilterText(io::ByteBuffer& buffer) {
    std::string text = readString(buffer);
    static_cast<void>(buffer.readByte()); // Whether it came from the server, which on the way in it did not.
    if (buffer.failed()) {
        return std::nullopt;
    }
    return text;
}

void writeFilterText(io::ByteBuffer& buffer, const std::string_view text) {
    writeString(buffer, text);
    buffer.writeByte(1); // From the server, which is what marks it as looked over.
}

std::optional<BedrockLecternPage> readLecternUpdate(io::ByteBuffer& buffer, const int protocol) {
    /// Where the flag that said the book was being taken went away: that edition drops it on the
    /// ground itself and tells the server through the block instead.
    constexpr int kNoDropFlagProtocol = 662;

    BedrockLecternPage update;
    update.page = std::int32_t{buffer.readByte()};
    static_cast<void>(buffer.readByte()); // How many pages the client thinks it has.
    update.x = readVarInt(buffer);
    update.y = static_cast<std::int32_t>(readVarUInt(buffer));
    update.z = readVarInt(buffer);
    if (protocol < kNoDropFlagProtocol) {
        static_cast<void>(buffer.readByte());
    }
    return buffer.failed() ? std::nullopt : std::optional{update};
}

std::optional<BedrockCommandBlock> readCommandBlockUpdate(io::ByteBuffer& buffer) {
    BedrockCommandBlock update;
    if (buffer.readByte() == 0) {
        return std::nullopt; // A minecart's, which this server has none of.
    }
    update.x = readVarInt(buffer);
    update.y = static_cast<std::int32_t>(readVarUInt(buffer));
    update.z = readVarInt(buffer);
    update.mode = static_cast<std::int32_t>(readVarUInt(buffer));
    update.needsRedstone = buffer.readByte() != 0;
    update.conditional = buffer.readByte() != 0;
    update.command = readString(buffer);
    static_cast<void>(readString(buffer)); // What it last printed, which the client keeps its own copy of,
    static_cast<void>(readString(buffer)); // the name over it,
    static_cast<void>(readString(buffer)); // and that name once a filter has been over it.
    update.trackOutput = buffer.readByte() != 0;
    update.tickDelay = readIntLE(buffer);
    static_cast<void>(buffer.readByte()); // Whether it runs on the first tick, which only a chain uses.
    return buffer.failed() ? std::nullopt : std::optional{update};
}

std::optional<BedrockCrafterSlot> readCrafterSlotToggle(io::ByteBuffer& buffer) {
    BedrockCrafterSlot toggle;
    toggle.x = readIntLE(buffer);
    toggle.y = readIntLE(buffer);
    toggle.z = readIntLE(buffer);
    toggle.slot = static_cast<std::int8_t>(buffer.readByte());
    toggle.disabled = buffer.readByte() != 0;
    return buffer.failed() ? std::nullopt : std::optional{toggle};
}

/// Where an emote grew the three fields that follow its name.
constexpr int kEmoteGrewProtocol = 729;

std::optional<BedrockEmote> readEmote(io::ByteBuffer& buffer, const int protocol) {
    BedrockEmote emote;
    emote.runtimeEntityId = readVarULong(buffer);
    emote.emoteId = readString(buffer);
    if (protocol >= kEmoteGrewProtocol) {
        emote.duration = static_cast<std::int32_t>(readVarUInt(buffer));
        static_cast<void>(readString(buffer)); // Whose account made it,
        static_cast<void>(readString(buffer)); // and on what.
    }
    emote.flags = buffer.readByte();
    return buffer.failed() ? std::nullopt : std::optional{emote};
}

void writeEmote(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::string_view emoteId,
                const std::int32_t duration, const std::uint8_t flags, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeString(buffer, emoteId);
    if (protocol >= kEmoteGrewProtocol) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(std::max(duration, 0)));
        writeString(buffer, "");
        writeString(buffer, "");
    }
    buffer.writeByte(flags);
}

void writeOpenSign(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                   const bool front) {
    writeBlockPosition(buffer, x, y, z);
    writeBool(buffer, front);
}

void writeSetEntityLink(io::ByteBuffer& buffer, const std::int64_t vehicleId, const std::int64_t riderId,
                        const BedrockRide how, const bool immediate) {
    writeVarLong(buffer, vehicleId);
    writeVarLong(buffer, riderId);
    buffer.writeByte(static_cast<std::uint8_t>(how));
    // Immediate is what a player sitting down themselves gets: no little animation in between.
    writeBool(buffer, immediate);
    writeBool(buffer, false);   // Not the rider's own doing as far as the client is concerned.
    writeFloatLE(buffer, 0.0F); // How fast the vehicle is turning, which only a boat's oars use.
}

void writeSetDifficulty(io::ByteBuffer& buffer, const std::int32_t difficulty) {
    writeVarUInt(buffer, static_cast<std::uint32_t>(difficulty));
}

void writeAddPainting(io::ByteBuffer& buffer, const std::int64_t entityId, const std::uint64_t runtimeEntityId,
                      const float x, const float y, const float z, const std::int32_t direction,
                      const std::string_view motive) {
    writeVarLong(buffer, entityId);
    writeVarULong(buffer, runtimeEntityId);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeVarInt(buffer, direction);
    writeString(buffer, motive);
}

void writeSyncEntityProperty(io::ByteBuffer& buffer, const std::string_view entityType, const std::string_view property,
                             const std::span<const std::string_view> choices) {
    /// What kind of property it is, of which this server sends only the one that picks from a list.
    constexpr std::int32_t kEnumProperty = 3;

    nbt::beginCompound(buffer);
    nbt::beginCompoundList(buffer, "properties", 1);
    nbt::writeString(buffer, "name", property);
    nbt::writeStringList(buffer, "enum", choices);
    nbt::writeInt(buffer, "type", kEnumProperty);
    nbt::end(buffer);
    nbt::writeString(buffer, "type", entityType);
    nbt::end(buffer);
}

void writeEntityIntProperty(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::uint32_t index,
                            const std::int32_t value) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 0); // Nothing in the metadata itself: the property is the message.
    writeVarUInt(buffer, 1);
    writeVarUInt(buffer, index);
    writeVarInt(buffer, value);
    writeVarUInt(buffer, 0);  // No properties that are a fraction.
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

bool hasAnything(const BedrockLook& look) {
    return look.variant != 0 || look.markVariant != 0 || look.colour.has_value() || look.colour2.has_value() ||
           look.scale.has_value() || look.horns.has_value() || look.strength.has_value();
}

void writeEntityLook(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockLook& look,
                     const int protocol) {
    std::uint32_t fields = 0;
    fields += look.variant != 0 ? 1U : 0U;
    fields += look.markVariant != 0 ? 1U : 0U;
    fields += look.colour.has_value() ? 1U : 0U;
    fields += look.colour2.has_value() ? 1U : 0U;
    fields += look.scale.has_value() ? 1U : 0U;
    fields += look.horns.has_value() ? 1U : 0U;
    fields += look.strength.has_value() ? 1U : 0U;

    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, fields);
    if (look.variant != 0) {
        writeMetadataInt(buffer, EntityDataId::Variant, look.variant, protocol);
    }
    if (look.markVariant != 0) {
        writeMetadataInt(buffer, EntityDataId::MarkVariant, look.markVariant, protocol);
    }
    if (look.colour.has_value()) {
        writeMetadataHead(buffer, EntityDataId::Color, EntityDataFormat::Byte, protocol);
        buffer.writeByte(static_cast<std::uint8_t>(*look.colour));
    }
    if (look.colour2.has_value()) {
        writeMetadataHead(buffer, EntityDataId::Color2, EntityDataFormat::Byte, protocol);
        buffer.writeByte(static_cast<std::uint8_t>(*look.colour2));
    }
    if (look.scale.has_value()) {
        writeMetadataFloat(buffer, EntityDataId::Scale, *look.scale, protocol);
    }
    if (look.horns.has_value()) {
        writeMetadataInt(buffer, EntityDataId::GoatHornCount, *look.horns, protocol);
    }
    if (look.strength.has_value()) {
        writeMetadataInt(buffer, EntityDataId::Strength, *look.strength, protocol);
    }
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

void writeEntityFlags(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::uint64_t flags,
                      const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 1);
    writeMetadataLong(buffer, EntityDataId::Flags, static_cast<std::int64_t>(flags), protocol);
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

void writeNameTag(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::string_view nameTag,
                  const bool always, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 2);
    writeMetadataString(buffer, EntityDataId::Name, nameTag, protocol);
    // parity: a name is drawn through walls and at any distance only while this is set, which is
    // what `never` and `hideForOtherTeams` come to once the line itself has been worked out.
    writeMetadataHead(buffer, EntityDataId::NametagAlwaysShow, EntityDataFormat::Byte, protocol);
    buffer.writeByte(always ? 1 : 0);
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

void writeSleeping(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::uint64_t moreFlags,
                   const std::int32_t bedX, const std::int32_t bedY, const std::int32_t bedZ, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 2);
    writeMetadataLong(buffer, EntityDataId::Flags2, static_cast<std::int64_t>(moreFlags), protocol);
    writeMetadataVector3i(buffer, EntityDataId::BedPosition, bedX, bedY, bedZ, protocol);
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

namespace {

/// The head every transaction carries: the legacy request, the slots it names and the client's own
/// idea of what moved in its bags. Answers which kind of transaction follows.
[[nodiscard]] std::uint32_t readTransactionHead(io::ByteBuffer& buffer, const int protocol) {
    // 1001 announced the old slots, the kind and the list each with a flag, and 26.50 kept only the
    // first. Before 1001 there were none.
    const bool flagged = protocol >= kTransactionFlagsProtocol;
    const bool single = protocol >= kSingleFlagProtocol;
    const std::int32_t legacyRequestId = readVarInt(buffer);
    if ((!flagged || buffer.readByte() != 0) && legacyRequestId < -1 && (legacyRequestId & 1) == 0) {
        const std::uint32_t windows = readVarUInt(buffer);
        for (std::uint32_t at = 0; at < windows && !buffer.failed(); ++at) {
            static_cast<void>(buffer.readByte());
            const std::uint32_t slots = readVarUInt(buffer);
            for (std::uint32_t slot = 0; slot < slots && !buffer.failed(); ++slot) {
                static_cast<void>(buffer.readByte());
            }
        }
    }
    if (flagged && !single) {
        static_cast<void>(buffer.readByte()); // The kind is there,
    }
    const std::uint32_t kind = readVarUInt(buffer);
    if (flagged && !single) {
        static_cast<void>(buffer.readByte()); // and so is the list.
    }
    // This server decides for itself what moved in a bag, so the client's list is read and dropped.
    const std::uint32_t actions = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < actions && !buffer.failed(); ++at) {
        readInventorySource(buffer, single ? 1 : flagged ? 2 : 0);
        static_cast<void>(readVarUInt(buffer));
        static_cast<void>(readItem(buffer, protocol));
        static_cast<void>(readItem(buffer, protocol));
    }
    return kind;
}

} // namespace

BedrockTransaction readTransaction(io::ByteBuffer& buffer, const int protocol) {
    /// The five kinds: a plain move, a mismatch, a use, a use on an entity and a release. Only the
    /// last two are acted on here.
    constexpr std::uint32_t kUseOnEntity = 3;
    constexpr std::uint32_t kItemRelease = 4;

    BedrockTransaction transaction;
    const std::uint32_t kind = readTransactionHead(buffer, protocol);
    if (buffer.failed()) {
        return transaction;
    }
    if (kind == kUseOnEntity) {
        BedrockEntityUse use;
        use.runtimeEntityId = readVarULong(buffer);
        use.action = readVarInt(buffer);
        use.hotbarSlot = readVarInt(buffer);
        static_cast<void>(readItem(buffer, protocol)); // What they are holding, which this server knows.
        for (int at = 0; at < 6; ++at) {
            static_cast<void>(readFloatLE(buffer)); // Where they were, and where on it they clicked.
        }
        if (!buffer.failed()) {
            transaction.entityUse = use;
        }
        return transaction;
    }
    if (kind == kItemRelease) {
        BedrockItemRelease release;
        release.action = static_cast<std::int32_t>(readVarUInt(buffer));
        release.hotbarSlot = readVarInt(buffer);
        static_cast<void>(readItem(buffer, protocol)); // What they were holding, which this server knows.
        for (int at = 0; at < 3; ++at) {
            static_cast<void>(readFloatLE(buffer)); // Where they were looking from.
        }
        if (!buffer.failed()) {
            transaction.itemRelease = release;
        }
    }
    return transaction;
}

std::optional<BedrockInteract> readInteract(io::ByteBuffer& buffer) {
    BedrockInteract interact;
    interact.action = std::int32_t{buffer.readByte()};
    interact.runtimeEntityId = readVarULong(buffer);
    // Where the mouse was, which only a mouse-over carries and nothing here reads.
    if (buffer.readByte() != 0) {
        static_cast<void>(readFloatLE(buffer));
        static_cast<void>(readFloatLE(buffer));
        static_cast<void>(readFloatLE(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{interact};
}

std::optional<std::int32_t> readHeldSlot(io::ByteBuffer& buffer, const int protocol) {
    static_cast<void>(readVarULong(buffer));                         // Whose hand it is, which is always the sender's.
    static_cast<void>(readItem(buffer, protocol));                   // What is in it, which this server already knows.
    static_cast<void>(buffer.readByte());                            // Where it sits in the bag,
    const std::int32_t hotbarSlot = std::int32_t{buffer.readByte()}; // and which of the nine it is.
    static_cast<void>(buffer.readByte()); // Out of which container, which for a hand is the bag.
    return buffer.failed() ? std::nullopt : std::optional{hotbarSlot};
}

void writeSelfMetadata(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::int16_t airSupply,
                       const std::int16_t maxAirSupply, const std::uint64_t flags, const std::uint64_t moreFlags,
                       const int protocol) {
    // A player's box and scale, which do not change but are sent with the rest for the same reason:
    // the client has never been told them, and its own default for all three is zero.
    constexpr float kPlayerWidth = 0.6F;
    constexpr float kPlayerHeight = 1.8F;

    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 7);
    writeMetadataLong(buffer, EntityDataId::Flags, static_cast<std::int64_t>(flags), protocol);
    writeMetadataLong(buffer, EntityDataId::Flags2, static_cast<std::int64_t>(moreFlags), protocol);
    writeMetadataShort(buffer, EntityDataId::AirSupply, airSupply, protocol);
    writeMetadataShort(buffer, EntityDataId::AirSupplyMax, maxAirSupply, protocol);
    writeMetadataFloat(buffer, EntityDataId::Width, kPlayerWidth, protocol);
    writeMetadataFloat(buffer, EntityDataId::Height, kPlayerHeight, protocol);
    writeMetadataFloat(buffer, EntityDataId::Scale, 1.0F, protocol);
    writeVarUInt(buffer, 0); // No entity properties, of either kind.
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0); // Which tick it belongs to, which only a replay cares about.
}

std::string_view bannerPatternOf(const std::string_view code) {
    // parity: the short codes that edition names banner patterns by, against this one's own paths.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 43> kPatterns{{
        {"b", "base"},
        {"bl", "square_bottom_left"},
        {"br", "square_bottom_right"},
        {"tl", "square_top_left"},
        {"tr", "square_top_right"},
        {"bs", "stripe_bottom"},
        {"ts", "stripe_top"},
        {"ls", "stripe_left"},
        {"rs", "stripe_right"},
        {"cs", "stripe_center"},
        {"ms", "stripe_middle"},
        {"drs", "stripe_downright"},
        {"dls", "stripe_downleft"},
        {"ss", "small_stripes"},
        {"cr", "cross"},
        {"sc", "straight_cross"},
        {"bt", "triangle_bottom"},
        {"tt", "triangle_top"},
        {"bts", "triangles_bottom"},
        {"tts", "triangles_top"},
        {"ld", "diagonal_left"},
        {"rd", "diagonal_up_right"},
        {"lud", "diagonal_up_left"},
        {"rud", "diagonal_right"},
        {"mc", "circle"},
        {"mr", "rhombus"},
        {"vh", "half_vertical"},
        {"hh", "half_horizontal"},
        {"vhr", "half_vertical_right"},
        {"hhb", "half_horizontal_bottom"},
        {"bo", "border"},
        {"cbo", "curly_border"},
        {"gra", "gradient"},
        {"gru", "gradient_up"},
        {"bri", "bricks"},
        {"glb", "globe"},
        {"cre", "creeper"},
        {"sku", "skull"},
        {"flo", "flower"},
        {"moj", "mojang"},
        {"pig", "piglin"},
        {"flw", "flow"},
        {"gus", "guster"},
    }};
    const auto found = std::ranges::find(kPatterns, code, &std::pair<std::string_view, std::string_view>::first);
    return found == kPatterns.end() ? std::string_view{} : found->second;
}

namespace {

/// parity: `WaypointStyle`'s own two distances, which the other edition's default style keeps.
constexpr float kWaypointNear = 128.0F;
constexpr float kWaypointFar = 332.0F;

/// The four dots Bedrock's own pack draws a waypoint with, nearest first.
constexpr std::array<std::string_view, 4> kWaypointDots{
    "textures/ui/locator_bar_dot_0",
    "textures/ui/locator_bar_dot_1",
    "textures/ui/locator_bar_dot_2",
    "textures/ui/locator_bar_dot_3",
};

/// Which of the four a dot that far away is drawn with.
[[nodiscard]] std::size_t waypointDot(const float distance) {
    if (distance < kWaypointNear) {
        return 0;
    }
    if (distance >= kWaypointFar) {
        return kWaypointDots.size() - 1;
    }
    const float along = (distance - kWaypointNear) / (kWaypointFar - kWaypointNear);
    return 1 + static_cast<std::size_t>(std::floor(along * 2.0F));
}

/// And how big it is drawn: full size for the first two, a little over half for the rest, a fifth
/// once it is as far away as the bar goes.
[[nodiscard]] float waypointIconSize(const float distance) {
    if (distance >= kWaypointFar) {
        return 0.2F;
    }
    return waypointDot(distance) <= 1 ? 1.0F : 0.6F;
}

/// The same choice on 944, which numbers its dots instead and interpolates by the square of the
/// distance rather than the distance.
[[nodiscard]] std::int32_t waypointDotId(const float distance) {
    constexpr float kNearSquared = kWaypointNear * kWaypointNear;
    constexpr float kFarSquared = kWaypointFar * kWaypointFar;
    const float squared = distance * distance;
    if (squared < kNearSquared) {
        return 2;
    }
    if (squared >= kFarSquared) {
        return 5;
    }
    return 3 + static_cast<std::int32_t>(std::floor((squared - kNearSquared) / (kFarSquared - kNearSquared) * 2.0F));
}

/// Which of the waypoint's fields the line carries. parity: the client reads a field only when
/// its bit is set, so a line that sets none moves nothing.
constexpr std::int32_t kWaypointWorldPos = 1;
constexpr std::int32_t kWaypointVisible = 1 << 1;
constexpr std::int32_t kWaypointTexture = 1 << 2;
constexpr std::int32_t kWaypointIconSize = 1 << 3;
constexpr std::int32_t kWaypointColour = 1 << 4;
constexpr std::int32_t kWaypointAuthority = 1 << 5;

} // namespace

void writeLocatorBar(io::ByteBuffer& buffer, const BedrockWaypoint& waypoint, const BedrockWaypointAction action,
                     const int protocol) {
    const bool byPath = protocol >= kWaypointTextureProtocol;
    writeVarUInt(buffer, 1); // One line, which is all this server ever has to say at once.
    writeUuid(buffer, waypoint.groupHigh, waypoint.groupLow);

    // Taking a line off the bar names it and nothing else: the client already has the rest.
    const std::int32_t fields = action == BedrockWaypointAction::Remove
                                    ? 0
                                    : kWaypointWorldPos | kWaypointVisible | kWaypointTexture | kWaypointIconSize |
                                          kWaypointColour | kWaypointAuthority;
    writeIntLE(buffer, fields);

    writeBool(buffer, true);
    writeBool(buffer, true); // Drawn, since a line that is not is a line not sent.

    writeBool(buffer, true);
    writeFloatLE(buffer, waypoint.x);
    writeFloatLE(buffer, waypoint.y);
    writeFloatLE(buffer, waypoint.z);
    writeVarInt(buffer, static_cast<std::int32_t>(waypoint.dimension));

    if (byPath) {
        writeBool(buffer, true);
        writeString(buffer, kWaypointDots[waypointDot(waypoint.distance)]);
        writeBool(buffer, true);
        const float size = waypointIconSize(waypoint.distance);
        writeFloatLE(buffer, size);
        writeFloatLE(buffer, size);
    } else {
        // 944 has no textures of its own: the dot is a number, and its four run from 2 to 5.
        writeBool(buffer, true);
        writeIntLE(buffer, waypointDotId(waypoint.distance));
    }

    writeBool(buffer, true);
    writeIntLE(buffer, static_cast<std::int32_t>(static_cast<std::uint32_t>(waypoint.colour) | 0xFF000000U));

    // A dot with an entity behind it is the client's to move between updates; one without is not.
    writeBool(buffer, true);
    writeBool(buffer, waypoint.entityId.has_value());
    writeBool(buffer, waypoint.entityId.has_value());
    if (waypoint.entityId.has_value()) {
        writeVarLong(buffer, *waypoint.entityId);
    }

    buffer.writeByte(static_cast<std::uint8_t>(action));
}

void writePlayerLocation(io::ByteBuffer& buffer, const std::int64_t entityId,
                         const std::optional<std::array<float, 3>> position, const int protocol) {
    // Pointed at, or taken off the bar again.
    const std::int32_t type = position.has_value() ? 0 : 1;
    if (writesNativeShapes(protocol)) {
        writeVarLong(buffer, entityId);
        writeVarUInt(buffer, static_cast<std::uint32_t>(type));
        writeVarInt(buffer, type);
    } else {
        writeIntLE(buffer, type);
        writeVarLong(buffer, entityId);
    }
    if (position.has_value()) {
        writeFloatLE(buffer, (*position)[0]);
        writeFloatLE(buffer, (*position)[1]);
        writeFloatLE(buffer, (*position)[2]);
    }
}

void writeGameRuleFlag(io::ByteBuffer& buffer, const std::string_view name, const bool value, const int protocol) {
    writeVarUInt(buffer, 1);
    writeString(buffer, name);
    if (writesNativeShapes(protocol)) {
        writeBool(buffer, false); // Not the player's to change.
    }
    writeVarUInt(buffer, 1); // A flag, rather than a whole number or a fraction.
    writeBool(buffer, value);
}

std::optional<BedrockPacketViolation> readPacketViolationWarning(io::ByteBuffer& buffer) {
    BedrockPacketViolation violation;
    violation.type = readVarInt(buffer);
    violation.severity = readVarInt(buffer);
    violation.packetId = readVarInt(buffer);
    violation.context = readString(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    return violation;
}

namespace {

/// Every field of a camera preset past its two names is optional, and this server sets none of
/// them: the four it sends are the client's own, and their behaviour comes with them.
constexpr int kCameraPresetOptionals = 20;

/// The four, in the order their numbers are read back.
constexpr std::array<std::string_view, 4> kCameraPresets{
    "minecraft:first_person",
    "minecraft:free",
    "minecraft:third_person",
    "minecraft:third_person_front",
};

/// A field that is not there, which is a flag of its own and nothing after it.
void writeAbsent(io::ByteBuffer& buffer) { writeBool(buffer, false); }

} // namespace

void writeCameraPresets(io::ByteBuffer& buffer, const int protocol) {
    writeVarUInt(buffer, static_cast<std::uint32_t>(kCameraPresets.size()));
    for (const std::string_view named : kCameraPresets) {
        writeString(buffer, named);
        writeString(buffer, ""); // Inherits from nothing: these are the client's own.
        for (int at = 0; at < kCameraPresetOptionals; ++at) {
            writeAbsent(buffer);
        }
        if (protocol >= kCameraStartRotationProtocol) {
            writeBool(buffer, false); // Not the parent's starting rotation,
            writeAbsent(buffer);      // nor one of its own.
        }
    }
}

namespace {

/// Everything after the set instruction, which this server never sends: a fade, a target, a field
/// of view, a spline, and an entity to hang the camera on.
void writeCameraRest(io::ByteBuffer& buffer) {
    writeAbsent(buffer); // No fade,
    writeAbsent(buffer); // nothing to keep in frame,
    writeAbsent(buffer); // and nothing to stop keeping in frame.
    writeAbsent(buffer); // No field of view of its own,
    writeAbsent(buffer); // no path to follow,
    writeAbsent(buffer); // nothing to hang it on,
    writeAbsent(buffer); // and nothing to take it off.
}

} // namespace

void writeCameraSet(io::ByteBuffer& buffer, const BedrockCamera& camera) {
    writeBool(buffer, true); // There is an instruction,
    writeIntLE(buffer, static_cast<std::int32_t>(camera.preset));
    writeBool(buffer, true); // it slides rather than jumps,
    buffer.writeByte(0);     // at an even pace,
    writeFloatLE(buffer, camera.easeSeconds);
    writeBool(buffer, true); // to here,
    writeFloatLE(buffer, camera.x);
    writeFloatLE(buffer, camera.y);
    writeFloatLE(buffer, camera.z);
    writeBool(buffer, true); // looking this way.
    writeFloatLE(buffer, camera.pitch);
    writeFloatLE(buffer, camera.yaw);
    writeAbsent(buffer);      // Nothing to face,
    writeAbsent(buffer);      // no offset on the screen,
    writeAbsent(buffer);      // none on the entity either,
    writeAbsent(buffer);      // and it is not the default camera.
    writeBool(buffer, false); // Where it starts from is its own business.
    writeAbsent(buffer);      // Not a clear.
    writeCameraRest(buffer);
}

void writeCameraClear(io::ByteBuffer& buffer, const bool asDefault) {
    if (asDefault) {
        // Handing the first person preset back as the default is what gives the client its own
        // camera key again; a clear on its own is ignored while a set is still on its way.
        writeBool(buffer, true);
        writeIntLE(buffer, static_cast<std::int32_t>(BedrockCameraPreset::FirstPerson));
        writeAbsent(buffer); // No slide,
        writeAbsent(buffer); // no place,
        writeAbsent(buffer); // no direction,
        writeAbsent(buffer); // nothing to face,
        writeAbsent(buffer); // no offset on the screen,
        writeAbsent(buffer); // none on the entity,
        writeBool(buffer, true);
        writeBool(buffer, true); // This one is the default.
        writeBool(buffer, false);
        writeAbsent(buffer); // Not a clear: that is the packet after this one.
        writeCameraRest(buffer);
        return;
    }
    writeAbsent(buffer); // No instruction,
    writeBool(buffer, true);
    writeBool(buffer, true); // only take the camera away.
    writeCameraRest(buffer);
}

} // namespace basalt::wire::bedrock
