#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

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

std::optional<ResourcePackResponse> readResourcePackResponse(io::ByteBuffer& buffer) {
    const std::uint32_t status = readVarUInt(buffer) + 1;
    if (buffer.failed() || status < 1 || status > 4) {
        return std::nullopt;
    }
    return static_cast<ResourcePackResponse>(status);
}

void writePlayStatus(io::ByteBuffer& buffer, PlayStatus status) { buffer.writeInt(static_cast<std::int32_t>(status)); }

void writeServerToClientHandshake(io::ByteBuffer& buffer, std::string_view jwt) { writeString(buffer, jwt); }

void writeResourcePacksInfo(io::ByteBuffer& buffer) {
    writeBool(buffer, false); // Nothing to accept.
    writeBool(buffer, false); // No addon packs.
    writeBool(buffer, false); // No scripting.
    writeBool(buffer, false); // Vibrant visuals left to the client.
    writeUuid(buffer, 0, 0);
    writeString(buffer, "");
    writeVarUInt(buffer, 0); // No packs.
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

void writeStartGame(io::ByteBuffer& buffer, const StartGameInfo& info) {
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
    buffer.writeByte(1);   // Members by default.
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
    writeBool(buffer, false);
    writeVarInt(buffer, 0); // No editor connection.
    writeBool(buffer, false);

    writeString(buffer, info.levelId);
    writeString(buffer, info.levelName);
    writeString(buffer, ""); // No premium world template.
    writeBool(buffer, false);
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
    writeBool(buffer, true);  // Block ids are hashes of their state, not an order we have to agree on.
    writeBool(buffer, true);  // Sounds come from the server.
    writeBool(buffer, false); // No join info.
    writeString(buffer, info.serverId);
    writeString(buffer, ""); // No scenario.
    writeString(buffer, info.worldId);
    writeString(buffer, ""); // No owner.
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

std::optional<PlayerAuthInput> readPlayerAuthInput(io::ByteBuffer& buffer) {
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

void writeEmptyLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t subChunkCount,
                          std::int32_t dimension) {
    writeVarInt(buffer, chunkX);
    writeVarInt(buffer, chunkZ);
    writeVarInt(buffer, dimension);
    writeVarUInt(buffer, 0);  // No block sections: the column is air.
    writeBool(buffer, false); // Not asking for subchunks one at a time.
    writeBool(buffer, false); // No blob cache.
    writeVarUInt(buffer, 0);  // No blobs.

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

} // namespace basalt::wire::bedrock
