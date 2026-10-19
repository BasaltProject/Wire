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
#include <basalt/wire/common/SafeInt.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>

namespace basalt::wire::bedrock {
namespace {

void writeBool(io::ByteBuffer& buffer, bool value) { buffer.writeByte(value ? 1 : 0); }

/// Only this one field does; every other coordinate on the wire is zigzag.
void writeBlockPosition(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z) {
    writeVarInt(buffer, x);
    writeVarUInt(buffer, static_cast<std::uint32_t>(y));
    writeVarInt(buffer, z);
}

void writeEmptyCompound(io::ByteBuffer& buffer) {
    buffer.writeByte(0x0a);
    writeVarUInt(buffer, 0);
    buffer.writeByte(0x00);
}

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

/// The skin out of the client's own description of itself.
void readSkin(std::string_view payload, LoginRequest& login) {
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
    writeBool(buffer, false);
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
    const std::span<const std::uint8_t> clientBytes = buffer.readSpan(clientLength);
    if (const auto parts =
            decodeJwt(std::string_view{reinterpret_cast<const char*>(clientBytes.data()), clientBytes.size()});
        parts) {
        readSkin(parts->payload, login);
    }

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
    const std::uint32_t status = writesNativeShapes(protocol) ? readVarUInt(buffer) + 1 : buffer.readByte();
    if (buffer.failed() || status < 1 || status > 4) {
        return std::nullopt;
    }
    return static_cast<ResourcePackResponse>(status);
}

void writePlayStatus(io::ByteBuffer& buffer, PlayStatus status) { buffer.writeInt(static_cast<std::int32_t>(status)); }

void writeServerToClientHandshake(io::ByteBuffer& buffer, std::string_view jwt) { writeString(buffer, jwt); }

void writeResourcePacksInfo(io::ByteBuffer& buffer, const int protocol) {
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeUuid(buffer, 0, 0);
    writeString(buffer, "");
    if (writesNativeShapes(protocol)) {
        writeVarUInt(buffer, 0);
    } else {
        writeShortLE(buffer, 0);
    }
}

void writeResourcePackStack(io::ByteBuffer& buffer, std::string_view gameVersion) {
    writeBool(buffer, false);
    writeVarUInt(buffer, 0);
    writeString(buffer, gameVersion);
    writeIntLE(buffer, 0);
    writeBool(buffer, false);
    writeBool(buffer, false);
}

void writeDisconnect(io::ByteBuffer& buffer, std::string_view reason) {
    writeVarInt(buffer, 0);
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

    writeLongLE(buffer, info.seed);
    writeShortLE(buffer, 0);
    writeString(buffer, "plains");
    writeVarInt(buffer, info.dimension);
    writeVarInt(buffer, 1);
    writeVarInt(buffer, info.levelGameType);
    writeBool(buffer, false);
    writeVarInt(buffer, info.difficulty);
    writeBlockPosition(buffer, info.spawnX, info.spawnY, info.spawnZ);
    writeBool(buffer, true);
    writeVarInt(buffer, 0);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeVarInt(buffer, -1);
    writeVarUInt(buffer, 0);
    writeBool(buffer, false);
    writeString(buffer, "");
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeBool(buffer, false);
    writeBool(buffer, true);
    writeBool(buffer, true);
    writeVarInt(buffer, 4);
    writeVarInt(buffer, 4);
    writeBool(buffer, info.commandsEnabled);
    writeBool(buffer, false);
    writeVarUInt(buffer, 0);
    writeIntLE(buffer, 0);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    if (writesNativeShapes(protocol)) {
        buffer.writeByte(1);
    } else {
        writeVarInt(buffer, 1);
    }
    writeIntLE(buffer, 4);
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
    writeBool(buffer, false);
    writeString(buffer, "");
    writeString(buffer, "");
    writeBool(buffer, false);
    buffer.writeByte(0);
    writeBool(buffer, false);
    if (protocol >= kStartGameGrewProtocol) {
        writeVarInt(buffer, 0);
        writeBool(buffer, false);
    }

    writeString(buffer, info.levelId);
    writeString(buffer, info.levelName);
    writeString(buffer, "");
    writeBool(buffer, false);
    writeVarInt(buffer, 0);
    writeBool(buffer, true);
    writeLongLE(buffer, info.currentTick);
    writeVarInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeString(buffer, info.multiplayerCorrelationId);
    writeBool(buffer, true);
    writeString(buffer, "Basalt");
    writeEmptyCompound(buffer);
    writeLongLE(buffer, 0);
    writeUuid(buffer, 0, 0);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, true);
    if (protocol >= kStartGameGrewProtocol && !writesNativeShapes(protocol)) {
        // Whether chat is logged, which only 1001 asks: 975 and below never had it and 2168 stopped asking.
        writeBool(buffer, false);
    }
    writeBool(buffer, false);
    writeString(buffer, info.serverId);
    writeString(buffer, "");
    writeString(buffer, info.worldId);
    writeString(buffer, "");
}

void writeTeleport(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const float x, const float y,
                   const float z, const float yaw, const float pitch, const std::uint64_t tick, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeFloatLE(buffer, pitch);
    writeFloatLE(buffer, yaw);
    writeFloatLE(buffer, yaw);
    buffer.writeByte(2);
    writeBool(buffer, false);
    writeVarULong(buffer, 0);
    // Only a teleport says why it happened, and this one always does.
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
    writeString(buffer, "");
    writeString(buffer, "");
    writeString(buffer, "");
}

void writeSetTime(io::ByteBuffer& buffer, const std::int32_t time) { writeVarInt(buffer, time); }

std::optional<std::string> readCommandRequest(io::ByteBuffer& buffer) {
    std::string command = readString(buffer);
    if (buffer.failed() || command.empty()) {
        return std::nullopt;
    }
    if (command.front() == '/') {
        command.erase(command.begin());
    }
    return command;
}

void writeModalFormRequest(io::ByteBuffer& buffer, const std::uint32_t formId, const std::string_view json) {
    writeVarUInt(buffer, formId);
    writeString(buffer, json);
}

void writeCloseForm(io::ByteBuffer& buffer) { static_cast<void>(buffer); }

std::optional<ModalFormResponse> readModalFormResponse(io::ByteBuffer& buffer) {
    ModalFormResponse response;
    response.formId = readVarUInt(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    if (buffer.readByte() != 0) {
        response.data = readString(buffer);
    }
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

std::string customFormJson(const std::string_view title, const std::span<const FormControl> controls) {
    common::JsonObject form;
    form.set("type", common::JsonValue::of(std::string{"custom_form"}));
    form.set("title", common::JsonValue::of(std::string{title}));
    std::vector<common::JsonValue> content;
    content.reserve(controls.size());
    for (const FormControl& control : controls) {
        common::JsonObject entry;
        entry.set("text", common::JsonValue::of(control.text));
        switch (control.kind) {
        case FormControl::Kind::Label:
            entry.set("type", common::JsonValue::of(std::string{"label"}));
            break;
        case FormControl::Kind::Input:
            entry.set("type", common::JsonValue::of(std::string{"input"}));
            entry.set("placeholder", common::JsonValue::of(control.placeholder));
            entry.set("default", common::JsonValue::of(control.initialText));
            break;
        case FormControl::Kind::Toggle:
            entry.set("type", common::JsonValue::of(std::string{"toggle"}));
            entry.set("default", common::JsonValue::of(control.initialToggle));
            break;
        case FormControl::Kind::Slider:
            entry.set("type", common::JsonValue::of(std::string{"slider"}));
            entry.set("min", common::JsonValue::of(control.min));
            entry.set("max", common::JsonValue::of(control.max));
            if (control.step > 0.0) {
                entry.set("step", common::JsonValue::of(control.step));
            }
            entry.set("default", common::JsonValue::of(control.initialNumber));
            break;
        case FormControl::Kind::Dropdown: {
            entry.set("type", common::JsonValue::of(std::string{"dropdown"}));
            std::vector<common::JsonValue> options;
            options.reserve(control.options.size());
            for (const std::string& option : control.options) {
                options.push_back(common::JsonValue::of(option));
            }
            entry.set("options", common::JsonValue::ofArray(std::move(options)));
            entry.set("default", common::JsonValue::of(static_cast<std::int64_t>(control.initialIndex)));
            break;
        }
        }
        content.push_back(common::JsonValue::ofObject(std::move(entry)));
    }
    form.set("content", common::JsonValue::ofArray(std::move(content)));
    return common::writeJson(common::JsonValue::ofObject(std::move(form)));
}

namespace {

constexpr std::int32_t kFlagSneaking = 8;
constexpr std::int32_t kFlagSprinting = 20;
constexpr std::int32_t kFlagVerticalCollision = 50;
constexpr std::int32_t kFlagItemInteraction = 34;
constexpr std::int32_t kFlagBlockActions = 35;
constexpr std::int32_t kFlagItemStackRequest = 36;
constexpr std::int32_t kFlagStartFlying = 42;
constexpr std::int32_t kFlagStopFlying = 43;

/// One block position as this edition writes it: the height is unsigned, which is not a mistake of ours either.
void readBedrockBlockPosition(io::ByteBuffer& buffer, std::int32_t& x, std::int32_t& y, std::int32_t& z) {
    x = readVarInt(buffer);
    y = static_cast<std::int32_t>(readVarUInt(buffer));
    z = readVarInt(buffer);
}

/// One item as the client sends it back.
[[nodiscard]] std::int32_t readItem(io::ByteBuffer& buffer, const int protocol) {
    if (protocol >= kHeldItemDescriptorProtocol) {
        const std::int32_t id = static_cast<std::int16_t>(readShortLE(buffer));
        static_cast<void>(readShortLE(buffer));
        static_cast<void>(readVarUInt(buffer));
        if (buffer.readByte() != 0) {
            static_cast<void>(readVarInt(buffer));
        }
        static_cast<void>(readVarUInt(buffer));
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
    static_cast<void>(readShortLE(buffer));
    static_cast<void>(readVarUInt(buffer));
    if (buffer.readByte() != 0) {
        static_cast<void>(readVarInt(buffer));
    }
    static_cast<void>(readVarInt(buffer));
    const std::uint32_t extra = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < extra && !buffer.failed(); ++at) {
        static_cast<void>(buffer.readByte());
    }
    return id;
}

/// The reading stops at the first one that is off, the way the client writes them.
[[nodiscard]] bool presentFlags(io::ByteBuffer& buffer, const int flags) {
    for (int at = 0; at < flags; ++at) {
        if (buffer.readByte() == 0) {
            return false;
        }
    }
    return true;
}

void readInventorySource(io::ByteBuffer& buffer, const int flags) {
    const std::uint32_t kind = readVarUInt(buffer);
    if (flags > 0) {
        if (presentFlags(buffer, flags)) {
            static_cast<void>(buffer.readByte());
        }
        if (presentFlags(buffer, flags)) {
            static_cast<void>(readVarUInt(buffer));
        }
        return;
    }
    if (kind == 0 || kind == 100 || kind == 99999) {
        static_cast<void>(readVarInt(buffer));
    } else if (kind == 2) {
        static_cast<void>(readVarUInt(buffer));
    }
}

[[nodiscard]] std::optional<BedrockItemUse> readItemUseTransaction(io::ByteBuffer& buffer, const int protocol) {
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
            static_cast<void>(readVarUInt(buffer));
            static_cast<void>(readItem(buffer, protocol));
            static_cast<void>(readItem(buffer, protocol));
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
        static_cast<void>(buffer.readByte());
    }
    use.itemRuntimeId = readItem(buffer, protocol);
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    use.clickX = readFloatLE(buffer);
    use.clickY = readFloatLE(buffer);
    use.clickZ = readFloatLE(buffer);
    static_cast<void>(readVarUInt(buffer));
    static_cast<void>(native ? std::int32_t{buffer.readByte()} : static_cast<std::int32_t>(readVarUInt(buffer)));
    if (protocol >= kItemCooldownProtocol) {
        static_cast<void>(buffer.readByte());
    }
    if (buffer.failed()) {
        return std::nullopt;
    }
    return use;
}

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

void readIngredient(io::ByteBuffer& buffer, const int protocol) {
    if (writesNativeShapes(protocol)) {
        const std::uint32_t kind = readVarUInt(buffer);
        static_cast<void>(buffer.readByte());
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

void readResultItem(io::ByteBuffer& buffer, const int protocol) {
    if (writesNativeShapes(protocol)) {
        const std::uint32_t kind = readVarUInt(buffer);
        static_cast<void>(buffer.readByte());
        if (kind != 0) {
            static_cast<void>(readString(buffer));
            static_cast<void>(readVarInt(buffer));
        }
        static_cast<void>(readShortLE(buffer));
        static_cast<void>(readVarUInt(buffer));
    } else {
        if (readVarInt(buffer) == 0) {
            return;
        }
        static_cast<void>(readShortLE(buffer));
        static_cast<void>(readVarUInt(buffer));
        static_cast<void>(readVarInt(buffer));
    }
    const std::uint32_t trailing = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < trailing && !buffer.failed(); ++at) {
        static_cast<void>(buffer.readByte());
    }
}

[[nodiscard]] std::optional<BedrockStackRequest> readItemStackRequest(io::ByteBuffer& buffer, const int protocol) {
    const bool native = writesNativeShapes(protocol);
    BedrockStackRequest request;
    request.requestId = readVarInt(buffer);
    const std::uint32_t lines = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < lines && !buffer.failed(); ++at) {
        const std::int32_t id = native ? static_cast<std::int32_t>(readVarUInt(buffer)) : buffer.readByte();
        const RequestAction kind = requestActionOf(id, protocol);
        if (native) {
            static_cast<void>(buffer.readByte());
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
            static_cast<void>(buffer.readByte());
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
            static_cast<void>(readVarInt(buffer));
            static_cast<void>(readVarInt(buffer));
            static_cast<void>(native ? readIntLE(buffer) : readVarInt(buffer));
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
                static_cast<void>(buffer.readByte());
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
            return std::nullopt;
        }
        request.actions.push_back(action);
    }

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

std::vector<BedrockStackRequest> readItemStackRequests(io::ByteBuffer& buffer, const int protocol) {
    std::vector<BedrockStackRequest> requests;
    const std::uint32_t count = readVarUInt(buffer);
    for (std::uint32_t at = 0; at < count && !buffer.failed(); ++at) {
        auto request = readItemStackRequest(buffer, protocol);
        if (!request)
            break;
        requests.push_back(std::move(*request));
    }
    return requests;
}

std::optional<PlayerAuthInput> readPlayerAuthInput(io::ByteBuffer& buffer, const int protocol) {
    PlayerAuthInput input;
    input.pitch = readFloatLE(buffer);
    input.yaw = readFloatLE(buffer);
    input.x = readFloatLE(buffer);
    input.y = readFloatLE(buffer);
    input.z = readFloatLE(buffer);
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    input.headYaw = readFloatLE(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }

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
    static_cast<void>(readVarUInt(buffer));
    static_cast<void>(readVarUInt(buffer));
    static_cast<void>(readVarUInt(buffer));
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readVarULong(buffer));
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    static_cast<void>(readFloatLE(buffer));
    if (buffer.failed()) {
        return std::nullopt;
    }

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

    writeBool(buffer, false);
    buffer.writeByte(withAuthor ? 1 : 0);
    buffer.writeByte(static_cast<std::uint8_t>(type));
    if (withAuthor) {
        writeString(buffer, source);
    }
    // A client with an empty line to draw logs a complaint and draws nothing, so it gets a space.
    writeString(buffer, message.empty() ? std::string_view{" "} : message);
    writeString(buffer, xuid);
    writeString(buffer, "");
    writeBool(buffer, false);
}

std::optional<IncomingText> readText(io::ByteBuffer& buffer) {
    static_cast<void>(buffer.readByte());
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
    writeIntLE(buffer, 0);
}

namespace {

void writeNbtKey(io::ByteBuffer& buffer, std::uint8_t type, std::string_view name) {
    buffer.writeByte(type);
    writeShortLE(buffer, static_cast<std::uint16_t>(name.size()));
    buffer.writeBytes(std::span{reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
}

/// Only a map carries a tag: the map it shows, in the three keys a client reads.
void writeItemUserData(io::ByteBuffer& buffer, const BedrockItemStack& item, bool blocking) {
    io::ByteBuffer data;
    if (item.mapId.has_value()) {
        writeShortLE(data, 0xFFFF);
        data.writeByte(1);
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
    writeIntLE(data, 0);
    writeIntLE(data, 0);
    if (blocking) {
        writeLongLE(data, 0);
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(data.readableBytes()));
    buffer.writeBytes(data.readable());
}

/// The stack as a client of this protocol can draw it: an item it has no name for yet becomes its look-alike.
[[nodiscard]] BedrockItemStack shownTo(BedrockItemStack item, const int protocol) {
    if (const ItemStandIn* standIn = bedrockItemStandIn(item.runtimeId - 1, protocol)) {
        item.runtimeId = standIn->standInPlace + 1;
        item.data = standIn->standInData;
    }
    return item;
}

/// It is the window shape without the flag that says a server is counting stacks.
void writeItemInstance(io::ByteBuffer& buffer, const BedrockItemStack& named, const int protocol) {
    const BedrockItemStack item = shownTo(named, protocol);
    const bool air = item.runtimeId == 0 || item.count == 0;
    if (air && protocol < kAirItemInstanceGrewProtocol) {
        writeVarInt(buffer, 0);
        return;
    }
    writeVarInt(buffer, air ? 0 : item.runtimeId);
    writeShortLE(buffer, static_cast<std::uint16_t>(air ? 0 : item.count));
    writeVarUInt(buffer, air ? 0U : static_cast<std::uint32_t>(item.data));
    writeVarInt(buffer, air ? 0 : item.blockRuntimeId);
    if (air) {
        writeVarUInt(buffer, 0);
        return;
    }
    const std::span<const std::string_view> palette = bedrockItemPalette();
    const std::size_t at = static_cast<std::size_t>(item.runtimeId) - 1;
    const bool blocking = at < palette.size() && palette[at] == "minecraft:shield";

    writeItemUserData(buffer, item, blocking);
}

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
    // Version nine, which is the one that says which subchunk it is rather than leaving the client to count.
    buffer.writeByte(9);
    buffer.writeByte(1);
    buffer.writeByte(static_cast<std::uint8_t>(subChunkIndex));

    std::vector<std::uint16_t> palette;
    std::array<std::uint16_t, kSubChunkBlocks> indices{};
    for (std::size_t over = 0; over < kSubChunkBlocks; ++over) {
        // This is the only place the two orders have to meet.
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
    writeVarUInt(buffer, 1);
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
    std::uint32_t netId = 1;
    for (std::size_t at = 0; at < offered; ++at) {
        if (!known(at)) {
            continue;
        }
        writeVarUInt(buffer, netId++);
        writeItemInstance(buffer, BedrockItemStack{.runtimeId = static_cast<std::int32_t>(at) + 1, .count = 1},
                          protocol);
        writeVarUInt(buffer, 0);
    }
}

void writeGameRulesChanged(io::ByteBuffer& buffer, const int protocol) {
    constexpr std::uint32_t kBool = 1;
    constexpr std::uint32_t kInt = 2;

    const bool editable = writesNativeShapes(protocol);
    const auto flag = [&buffer, editable](const std::string_view name, const bool value) {
        writeString(buffer, name);
        if (editable) {
            writeBool(buffer, false);
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
        if (editable) {
            writeIntLE(buffer, value);
        } else {
            writeVarUInt(buffer, static_cast<std::uint32_t>(value));
        }
    };

    writeVarUInt(buffer, 5);
    // Off, so the client never heals anybody by itself: hearts come from this server or not at all.
    flag("naturalregeneration", false);
    // On, so the client never empties a bag on death.
    flag("keepinventory", true);
    // Recipes are unlocked by being shown, not by the client deciding it has earned them.
    flag("recipesunlock", true);
    number("spawnradius", 0);
    flag("locatorBar", protocol >= kLocatorBarProtocol);
}

namespace {

/// The client reads the word, not the number.
constexpr std::string_view kDescriptorByName = "name";
constexpr std::string_view kDescriptorByTag = "item_tag";

constexpr std::int32_t kAnyAux = 32767;

/// Which screen a recipe belongs to when it does not say: a table, or a hand, which are the same word to that client.
constexpr std::string_view kCraftingTableTag = "crafting_table";

void writeIngredient(io::ByteBuffer& buffer, const BedrockIngredient& ingredient) {
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

void writeItemInstance(io::ByteBuffer& buffer, const BedrockItemStack& item) {
    writeVarInt(buffer, item.runtimeId);
    writeShortLE(buffer, static_cast<std::uint16_t>(item.count));
    writeVarUInt(buffer, static_cast<std::uint32_t>(item.data));
    writeVarInt(buffer, item.blockRuntimeId);
    writeVarUInt(buffer, 10);
    writeShortLE(buffer, 0);
    writeIntLE(buffer, 0);
    writeIntLE(buffer, 0);
}

void writeNoUnlocking(io::ByteBuffer& buffer) {
    writeVarInt(buffer, 0);
    writeBool(buffer, true);
    writeVarUInt(buffer, 0);
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
    writeVarUInt(buffer, 1);
    writeItemInstance(buffer, recipe.result);
    writeUuid(buffer, 0, static_cast<std::uint64_t>(at) + 1);
    writeString(buffer, recipe.tag.empty() ? kCraftingTableTag : recipe.tag);
    writeVarInt(buffer, 0);
    if (recipe.shaped) {
        writeBool(buffer, true);
    }
    writeBool(buffer, true);
    writeNoUnlocking(buffer);
    writeVarUInt(buffer, static_cast<std::uint32_t>(recipe.netId));
}

} // namespace

namespace {

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

/// Every enchantment that edition has, in its own order: the number on the wire is the place in this list.
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
    constexpr std::uint8_t kTradeContainer = 15;

    buffer.writeByte(static_cast<std::uint8_t>(containerId));
    buffer.writeByte(kTradeContainer);
    writeVarInt(buffer, 0);
    writeVarInt(buffer, tier);
    writeVarLong(buffer, traderId);
    writeVarLong(buffer, playerId);
    writeString(buffer, displayName);
    writeBool(buffer, true);
    writeBool(buffer, true);

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
    // From there the packet is eleven lists, one per kind, and this server fills two of them.
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
    constexpr int kEmptyKinds = 8;
    for (int empty = 0; empty < kEmptyKinds; ++empty) {
        writeVarUInt(buffer, 0);
    }
    writeBool(buffer, true);
}

void writeSetCommandsEnabled(io::ByteBuffer& buffer, const bool enabled) { writeBool(buffer, enabled); }

void writeSpawnParticle(io::ByteBuffer& buffer, const BedrockDimension dimension, const std::string_view particle,
                        const float x, const float y, const float z) {
    buffer.writeByte(static_cast<std::uint8_t>(dimension));
    writeVarLong(buffer, -1);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeString(buffer, particle);
    writeBool(buffer, false);
}

void writeEntityEvent(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockEntityEvent event,
                      const std::int32_t data, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    buffer.writeByte(static_cast<std::uint8_t>(event));
    writeVarInt(buffer, data);
    if (protocol >= kEntityEventGrewProtocol) {
        writeBool(buffer, false);
    }
}

void writeSetEntityMotion(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const float x, const float y,
                          const float z) {
    writeVarULong(buffer, runtimeEntityId);
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeVarULong(buffer, 0);
}

namespace {

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
    writeBool(buffer, false);
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
        writeShortLE(buffer, static_cast<std::uint16_t>(at + 1));
        writeBool(buffer, false);
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
    writeBool(buffer, false);
    writeItemStack(buffer, BedrockItemStack{}, protocol);
}

BedrockLevelEffect bedrockLevelEffect(const std::int32_t javaEvent, const std::int32_t data) {
    // parity: the nine `LevelEvent`s this server raises, each one read out of Geyser's own switch.
    switch (javaEvent) {
    case 2001:
        return {.event = 2001, .keepsData = true, .translatesBlock = true};
    case 1001:
        return {.sound = "block.dispenser.fail"};
    case 1050:
        return {.sound = "block.crafter.fail"};
    case 1500:
        return {.event = 2005, .sound = data != 0 ? "block.composter.fill_success" : "block.composter.fill"};
    case 1502:
        return {.event = 2016, .sound = "block.redstone_torch.burnout", .x = 0.0F, .y = 0.5F, .z = 0.0F};
    case 1505:
        return {.event = 2005, .keepsData = true};
    case 3002:
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
    writeVarLong(buffer, -1);
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
    static_cast<void>(buffer.readByte());
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
        writeBool(buffer, false);
        writeVarUInt(buffer, static_cast<std::uint32_t>(container.slots.size()));
        for (const BedrockResponseSlot& slot : container.slots) {
            buffer.writeByte(static_cast<std::uint8_t>(slot.slot));
            buffer.writeByte(static_cast<std::uint8_t>(slot.slot));
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
                writeBool(buffer, false);
            } else {
                writeVarInt(buffer, slot.stackNetworkId);
                writeString(buffer, "");
                writeString(buffer, "");
            }
            writeVarInt(buffer, 0);
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
    writeVarULong(buffer, 0);
    writeBool(buffer, ambient);
}

void writePlaySound(io::ByteBuffer& buffer, const std::string_view sound, const double x, const double y,
                    const double z, const float volume, const float pitch, const int protocol,
                    const std::optional<std::int64_t> soundHandle) {
    writeString(buffer, sound);
    writeBlockPosition(buffer, common::safeInt32(x * 8.0), common::safeInt32(y * 8.0), common::safeInt32(z * 8.0));
    writeFloatLE(buffer, volume);
    writeFloatLE(buffer, pitch);
    if (writesNativeShapes(protocol)) {
        writeVarUInt(buffer, 0);
    }
    if (protocol >= kSoundPlaybackProtocol) {
        writeBool(buffer, false);
    }
    if (protocol >= kServerSoundHandleProtocol) {
        writeBool(buffer, soundHandle.has_value());
        if (soundHandle.has_value()) {
            writeLongLE(buffer, *soundHandle);
        }
    }
    if (protocol >= kSoundPlaybackProtocol) {
        writeBool(buffer, false);
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
    writeVarUInt(buffer, 0b11);
    writeVarUInt(buffer, 0);
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
        writeBool(buffer, false);
        writeBool(buffer, false);
        writeVarUInt(buffer, 0);
    } else {
        writeBool(buffer, false);
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
    payload.writeByte(0);
    payload.writeBytes(blockEntities);

    writeVarUInt(buffer, static_cast<std::uint32_t>(payload.readableBytes()));
    buffer.writeBytes(payload.readable());
}

void writeEmptyLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t subChunkCount,
                          std::int32_t dimension, const int protocol) {
    writeVarInt(buffer, chunkX);
    writeVarInt(buffer, chunkZ);
    writeVarInt(buffer, dimension);
    writeVarUInt(buffer, 0);
    if (writesNativeShapes(protocol)) {
        writeBool(buffer, false);
        writeBool(buffer, false);
        writeVarUInt(buffer, 0);
    } else {
        writeBool(buffer, false);
    }

    io::ByteBuffer payload{static_cast<std::size_t>(subChunkCount) + 4};
    // The first is a palette of a single entry, which is what zero bits per block means.
    payload.writeByte(0x01);
    writeVarInt(payload, kFallbackBedrockBiome);
    for (std::uint32_t section = 1; section < subChunkCount; ++section) {
        payload.writeByte(static_cast<std::uint8_t>((127U << 1U) | 1U));
    }
    payload.writeByte(0);

    writeVarUInt(buffer, static_cast<std::uint32_t>(payload.readableBytes()));
    buffer.writeBytes(payload.readable());
}

namespace {

void writeImage(io::ByteBuffer& buffer, const std::uint32_t width, const std::uint32_t height,
                const std::span<const std::uint8_t> pixels) {
    writeIntLE(buffer, static_cast<std::int32_t>(width));
    writeIntLE(buffer, static_cast<std::int32_t>(height));
    writeVarUInt(buffer, static_cast<std::uint32_t>(pixels.size()));
    buffer.writeBytes(pixels);
}

void writeSkin(io::ByteBuffer& buffer, const SkinInfo& skin, const int protocol) {
    if (!writesNativeShapes(protocol)) {
        writeString(buffer, skin.skinId);
        writeString(buffer, "");
        writeString(buffer, skin.resourcePatch);
        writeImage(buffer, skin.width, skin.height, skin.image);
        writeIntLE(buffer, 0);
        writeImage(buffer, 0, 0, {});
        writeString(buffer, "");
        writeString(buffer, "");
        writeString(buffer, "");
        writeString(buffer, "");
        writeString(buffer, skin.skinId);
        writeString(buffer, "wide");
        writeString(buffer, "#0");
        writeIntLE(buffer, 0);
        writeIntLE(buffer, 0);
        writeBool(buffer, false);
        writeBool(buffer, false);
        writeBool(buffer, false);
        writeBool(buffer, true);
        writeBool(buffer, false); // It does not override what the player chose, because there is none.
        return;
    }

    writeString(buffer, skin.skinId);
    writeString(buffer, "");
    writeString(buffer, skin.resourcePatch);
    writeImage(buffer, skin.width, skin.height, skin.image);
    writeVarUInt(buffer, 0);
    writeImage(buffer, 0, 0, {});
    writeString(buffer, "");
    writeString(buffer, "");
    writeString(buffer, "");
    writeString(buffer, "");
    writeString(buffer, skin.skinId);
    buffer.writeByte(1);
    writeIntLE(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, true);
    writeBool(buffer, false); // It does not override what the player chose, because there is none.
    writeString(buffer, "true");
    writeString(buffer, "");
}

void writeByteAngle(io::ByteBuffer& buffer, const float degrees) {
    buffer.writeByte(static_cast<std::uint8_t>(common::safeInt32(std::trunc(degrees / (360.0F / 256.0F))) & 0xFF));
}

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
    if (!writesNativeShapes(protocol)) {
        buffer.writeByte(0);
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(entries.size()));
    for (const PlayerListEntry& entry : entries) {
        if (writesNativeShapes(protocol)) {
            writeVarUInt(buffer, 1);
            buffer.writeByte(0);
        }
        writeUuid(buffer, entry.uuidHigh, entry.uuidLow);
        writeVarLong(buffer, entry.entityId);
        writeString(buffer, entry.name);
        writeString(buffer, entry.xuid);
        writeString(buffer, "");
        writeIntLE(buffer, 0);
        writeSkin(buffer, entry.skin, protocol);
        writeBool(buffer, false);
        writeBool(buffer, false);
        writeBool(buffer, false);
        writeIntLE(buffer, 0);
    }
    if (!writesNativeShapes(protocol)) {
        for (std::size_t at = 0; at < entries.size(); ++at) {
            writeBool(buffer, true);
        }
    }
}

void writePlayerListRemove(io::ByteBuffer& buffer, const std::span<const std::pair<std::uint64_t, std::uint64_t>> uuids,
                           const int protocol) {
    if (!writesNativeShapes(protocol)) {
        buffer.writeByte(1);
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
    writeString(buffer, "");
    writeFloatLE(buffer, info.x);
    writeFloatLE(buffer, info.y);
    writeFloatLE(buffer, info.z);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, info.pitch);
    writeFloatLE(buffer, info.yaw);
    writeFloatLE(buffer, info.yaw);
    writeAirItem(buffer, protocol);
    writeVarInt(buffer, info.gameType);

    writeVarUInt(buffer, 5);
    writeMetadataLong(buffer, EntityDataId::Flags,
                      flagBit(EntityFlag::HasCollision) | flagBit(EntityFlag::HasGravity) |
                          flagBit(EntityFlag::Breathing) | flagBit(EntityFlag::CanShowName),
                      protocol);
    writeMetadataString(buffer, EntityDataId::Name, info.name, protocol);
    writeMetadataFloat(buffer, EntityDataId::Scale, 1.0F, protocol);
    writeMetadataFloat(buffer, EntityDataId::Width, 0.6F, protocol);
    writeMetadataFloat(buffer, EntityDataId::Height, 1.8F, protocol);

    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);

    writeLongLE(buffer, info.entityId);
    buffer.writeByte(1);
    buffer.writeByte(0);
    writeVarUInt(buffer, 0);

    writeVarUInt(buffer, 0);
    writeString(buffer, "");
    writeIntLE(buffer, 0);
}

void writeUpdateAbilities(io::ByteBuffer& buffer, const std::int64_t entityId, const bool operator_,
                          const bool creative) {
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

    std::uint32_t on = kBuild | kMine | kDoorsAndSwitches | kOpenContainers | kAttackPlayers | kAttackMobs;
    if (creative) {
        on |= kMayFly | kInstantBuild;
    }
    if (operator_) {
        on |= kOperatorCommands | kTeleport;
    }

    writeLongLE(buffer, entityId);
    buffer.writeByte(operator_ ? 2 : 1);
    buffer.writeByte(operator_ ? 2 : 0);

    writeVarUInt(buffer, 1);
    writeShortLE(buffer, 1);
    writeIntLE(buffer, static_cast<std::int32_t>(kDecided));
    writeIntLE(buffer, static_cast<std::int32_t>(on));
    // The three speeds vanilla gives a player.
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

constexpr std::int32_t kArgValid = 0x100000;

/// An argument this server does not describe.
constexpr std::int32_t kArgString = 56;

} // namespace

void writeChangeDimension(io::ByteBuffer& buffer, const BedrockDimension dimension, const float x, const float y,
                          const float z, const bool respawn) {
    writeVarInt(buffer, static_cast<std::int32_t>(dimension));
    writeFloatLE(buffer, x);
    writeFloatLE(buffer, y);
    writeFloatLE(buffer, z);
    writeBool(buffer, respawn);
    writeBool(buffer, false);
}

void writeDeathInfo(io::ByteBuffer& buffer, const std::string_view cause) {
    writeString(buffer, cause);
    writeVarUInt(buffer, 0);
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
    static_cast<void>(readVarULong(buffer));
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
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);

    writeVarUInt(buffer, static_cast<std::uint32_t>(commands.size()));
    for (const CommandInfo& command : commands) {
        writeString(buffer, command.name);
        writeString(buffer, command.description);
        writeShortLE(buffer, 0);
        writeString(buffer, "any");
        writeIntLE(buffer, -1);
        writeVarUInt(buffer, 0);
        writeVarUInt(buffer, 1);
        writeBool(buffer, false);
        writeVarUInt(buffer, 1);
        writeString(buffer, "args");
        writeIntLE(buffer, kArgString | kArgValid);
        writeBool(buffer, true);
        buffer.writeByte(0);
    }

    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
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
        writeFloatLE(buffer, attribute.minimum);
        writeFloatLE(buffer, attribute.maximum);
        writeFloatLE(buffer, attribute.defaultValue);
        writeString(buffer, attribute.name);
        writeVarUInt(buffer, 0);
    }
    writeVarULong(buffer, tick);
}

namespace {

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
    writeBool(buffer, false);
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
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, 0.0F);
    writeFloatLE(buffer, pitch);
    writeFloatLE(buffer, yaw);
    writeFloatLE(buffer, yaw);
    writeFloatLE(buffer, yaw);
    writeVarUInt(buffer, 0);
    writeBodyMetadata(buffer, width, height, flags, protocol);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
}

void writeAddBorderCollider(io::ByteBuffer& buffer, const std::int64_t entityId, const float x, const float y,
                            const float z, const int protocol) {
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
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 6);
    writeMetadataLong(buffer, EntityDataId::Flags,
                      static_cast<std::int64_t>(flagBit(EntityFlag::Invisible) | flagBit(EntityFlag::Silent) |
                                                flagBit(EntityFlag::NoAi)),
                      protocol);
    writeMetadataLong(buffer, EntityDataId::Flags2, static_cast<std::int64_t>(flagBit2(EntityFlag::Collidable)),
                      protocol);
    writeMetadataShort(buffer, EntityDataId::AirSupplyMax, 300, protocol);
    writeMetadataFloat(buffer, EntityDataId::Width, kWidth, protocol);
    writeMetadataFloat(buffer, EntityDataId::Height, kHeight, protocol);
    writeMetadataFloat(buffer, EntityDataId::Scale, 1.0F, protocol);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
}

void writeTakeItemEntity(io::ByteBuffer& buffer, const std::uint64_t itemRuntimeId,
                         const std::uint64_t collectorRuntimeId) {
    writeVarULong(buffer, itemRuntimeId);
    writeVarULong(buffer, collectorRuntimeId);
}

void writeAnimate(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockAnimation action) {
    buffer.writeByte(static_cast<std::uint8_t>(action));
    writeVarULong(buffer, runtimeEntityId);
    writeFloatLE(buffer, 0.0F);
    writeBool(buffer, false);
}

void writeMobEquipment(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const BedrockItemStack& item,
                       const std::int32_t hotbarSlot, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeItemStack(buffer, item, protocol);
    buffer.writeByte(static_cast<std::uint8_t>(hotbarSlot));
    buffer.writeByte(static_cast<std::uint8_t>(hotbarSlot));
    buffer.writeByte(0);
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
    writeString(buffer, "dummy");
    writeVarInt(buffer, 0);
}

void writeRemoveObjective(io::ByteBuffer& buffer, const std::string_view objective) { writeString(buffer, objective); }

void writeSetScore(io::ByteBuffer& buffer, const std::string_view objective,
                   const std::span<const BedrockScoreLine> lines, const int protocol) {
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
        writeString(buffer, "changefakeplayer");
        writeVarLong(buffer, line.lineId);
        writeString(buffer, named);
        writeIntLE(buffer, line.score);
        writeString(buffer, name);
    }
}

void writeRemoveScores(io::ByteBuffer& buffer, const std::string_view objective,
                       const std::span<const std::int64_t> lineIds, const int protocol) {
    constexpr std::uint8_t kRemove = 1;
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
        writeBool(buffer, false);
    }
}

void writeBossEvent(io::ByteBuffer& buffer, const std::int64_t bossEntityId, const BedrockBossAction action,
                    const std::string_view title, const float progress, const int protocol) {
    writeVarLong(buffer, bossEntityId);
    if (protocol < kBossBarPlayerlessProtocol) {
        writeVarLong(buffer, 0);
    }
    buffer.writeByte(static_cast<std::uint8_t>(action));
    writeString(buffer, title);
    writeString(buffer, "");
    writeFloatLE(buffer, progress);
    buffer.writeByte(0);
    buffer.writeByte(0);
}

void writeTransfer(io::ByteBuffer& buffer, const std::string_view address, const std::uint16_t port,
                   const int protocol) {
    writeString(buffer, address);
    writeShortLE(buffer, port);
    writeBool(buffer, true);
    if (writesNativeShapes(protocol)) {
        writeBool(buffer, false);
    }
}

void writeUpdateAdventureSettings(io::ByteBuffer& buffer, const bool immutableWorld) {
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeBool(buffer, immutableWorld);
    writeBool(buffer, false);
    writeBool(buffer, true);
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
        nbt::writeInt(buffer, "SignTextColor", -16777216);
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
    writeBool(buffer, true);
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
    writeString(buffer, "");
    writeBool(buffer, false);
    writeBool(buffer, false);
    writeLongLE(buffer, 0);
    if (protocol >= kServerSoundHandleProtocol) {
        writeBool(buffer, false);
    }
}

void writeItemFrameBlock(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                         const bool glowing, const int face, const int protocol) {
    writeBlockPosition(buffer, x, y, z);
    writeVarUInt(buffer, itemFrameBlockId(glowing, face, protocol));
    // Tell the neighbours, and say it came over the network rather than from the client itself.
    writeVarUInt(buffer, 0b11);
    writeVarUInt(buffer, 0);
}

void writeItemFrameData(io::ByteBuffer& buffer, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                        const bool glowing, const std::string_view item, const std::int32_t count,
                        const std::int32_t rotation) {
    /// parity: the client looks a frame's data up by these two names and no other.
    constexpr std::string_view kPlain = "ItemFrame";
    constexpr std::string_view kGlowing = "GlowItemFrame";
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
    static_cast<void>(buffer.readByte());
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
    /// parity: `SetSpawnPositionPacket.Type.WORLD_SPAWN`.
    constexpr std::int32_t kWorldSpawn = 1;

    writeVarInt(buffer, kWorldSpawn);
    writeBlockPosition(buffer, x, y, z);
    writeVarInt(buffer, static_cast<std::int32_t>(dimension));
    writeBlockPosition(buffer, x, y, z);
}

void writeStopSound(io::ByteBuffer& buffer, const std::string_view sound, const bool stopAll) {
    writeString(buffer, sound);
    writeBool(buffer, stopAll);
    writeBool(buffer, false);
}

void writePlayerStartItemCooldown(io::ByteBuffer& buffer, const std::string_view category,
                                  const std::int32_t durationTicks) {
    writeString(buffer, category);
    writeVarInt(buffer, durationTicks);
}

std::optional<BedrockCredits> readShowCredits(io::ByteBuffer& buffer) {
    static_cast<void>(readVarULong(buffer));
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
        static_cast<void>(readString(buffer));
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
        static_cast<void>(readString(buffer));
        break;
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(edit)};
}

std::optional<std::string> readFilterText(io::ByteBuffer& buffer) {
    std::string text = readString(buffer);
    static_cast<void>(buffer.readByte());
    if (buffer.failed()) {
        return std::nullopt;
    }
    return text;
}

void writeFilterText(io::ByteBuffer& buffer, const std::string_view text) {
    writeString(buffer, text);
    buffer.writeByte(1);
}

std::optional<BedrockLecternPage> readLecternUpdate(io::ByteBuffer& buffer, const int protocol) {
    constexpr int kNoDropFlagProtocol = 662;

    BedrockLecternPage update;
    update.page = std::int32_t{buffer.readByte()};
    static_cast<void>(buffer.readByte());
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
        return std::nullopt;
    }
    update.x = readVarInt(buffer);
    update.y = static_cast<std::int32_t>(readVarUInt(buffer));
    update.z = readVarInt(buffer);
    update.mode = static_cast<std::int32_t>(readVarUInt(buffer));
    update.needsRedstone = buffer.readByte() != 0;
    update.conditional = buffer.readByte() != 0;
    update.command = readString(buffer);
    static_cast<void>(readString(buffer));
    static_cast<void>(readString(buffer));
    static_cast<void>(readString(buffer));
    update.trackOutput = buffer.readByte() != 0;
    update.tickDelay = readIntLE(buffer);
    static_cast<void>(buffer.readByte());
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

constexpr int kEmoteGrewProtocol = 729;

std::optional<BedrockEmote> readEmote(io::ByteBuffer& buffer, const int protocol) {
    BedrockEmote emote;
    emote.runtimeEntityId = readVarULong(buffer);
    emote.emoteId = readString(buffer);
    if (protocol >= kEmoteGrewProtocol) {
        emote.duration = static_cast<std::int32_t>(readVarUInt(buffer));
        static_cast<void>(readString(buffer));
        static_cast<void>(readString(buffer));
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
    writeBool(buffer, immediate);
    writeBool(buffer, false);
    writeFloatLE(buffer, 0.0F);
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
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 1);
    writeVarUInt(buffer, index);
    writeVarInt(buffer, value);
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0);
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
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0);
}

void writeEntityFlags(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::uint64_t flags,
                      const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 1);
    writeMetadataLong(buffer, EntityDataId::Flags, static_cast<std::int64_t>(flags), protocol);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0);
}

void writeNameTag(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::string_view nameTag,
                  const bool always, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 2);
    writeMetadataString(buffer, EntityDataId::Name, nameTag, protocol);
    writeMetadataHead(buffer, EntityDataId::NametagAlwaysShow, EntityDataFormat::Byte, protocol);
    buffer.writeByte(always ? 1 : 0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0);
}

void writeSleeping(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::uint64_t moreFlags,
                   const std::int32_t bedX, const std::int32_t bedY, const std::int32_t bedZ, const int protocol) {
    writeVarULong(buffer, runtimeEntityId);
    writeVarUInt(buffer, 2);
    writeMetadataLong(buffer, EntityDataId::Flags2, static_cast<std::int64_t>(moreFlags), protocol);
    writeMetadataVector3i(buffer, EntityDataId::BedPosition, bedX, bedY, bedZ, protocol);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0);
}

namespace {

[[nodiscard]] std::uint32_t readTransactionHead(io::ByteBuffer& buffer, const int protocol) {
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
        static_cast<void>(buffer.readByte());
    }
    const std::uint32_t kind = readVarUInt(buffer);
    if (flagged && !single) {
        static_cast<void>(buffer.readByte());
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
    /// Only the last two are acted on here.
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
        static_cast<void>(readItem(buffer, protocol));
        for (int at = 0; at < 6; ++at) {
            static_cast<void>(readFloatLE(buffer));
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
        static_cast<void>(readItem(buffer, protocol));
        for (int at = 0; at < 3; ++at) {
            static_cast<void>(readFloatLE(buffer));
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
    static_cast<void>(readVarULong(buffer));
    static_cast<void>(readItem(buffer, protocol));
    static_cast<void>(buffer.readByte());
    const std::int32_t hotbarSlot = std::int32_t{buffer.readByte()};
    static_cast<void>(buffer.readByte());
    return buffer.failed() ? std::nullopt : std::optional{hotbarSlot};
}

void writeSelfMetadata(io::ByteBuffer& buffer, const std::uint64_t runtimeEntityId, const std::int16_t airSupply,
                       const std::int16_t maxAirSupply, const std::uint64_t flags, const std::uint64_t moreFlags,
                       const int protocol) {
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
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
    writeVarULong(buffer, 0);
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

[[nodiscard]] std::size_t waypointDot(const float distance) {
    if (distance < kWaypointNear) {
        return 0;
    }
    if (distance >= kWaypointFar) {
        return kWaypointDots.size() - 1;
    }
    const float along = (distance - kWaypointNear) / (kWaypointFar - kWaypointNear);
    return 1 + static_cast<std::size_t>(std::max<std::int64_t>(common::safeInt(std::floor(along * 2.0F)), 0));
}

[[nodiscard]] float waypointIconSize(const float distance) {
    if (distance >= kWaypointFar) {
        return 0.2F;
    }
    return waypointDot(distance) <= 1 ? 1.0F : 0.6F;
}

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
    return 3 + common::safeInt32(std::floor((squared - kNearSquared) / (kFarSquared - kNearSquared) * 2.0F));
}

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
    writeVarUInt(buffer, 1);
    writeUuid(buffer, waypoint.groupHigh, waypoint.groupLow);

    // Taking a line off the bar names it and nothing else: the client already has the rest.
    const std::int32_t fields = action == BedrockWaypointAction::Remove
                                    ? 0
                                    : kWaypointWorldPos | kWaypointVisible | kWaypointTexture | kWaypointIconSize |
                                          kWaypointColour | kWaypointAuthority;
    writeIntLE(buffer, fields);

    writeBool(buffer, true);
    writeBool(buffer, true);

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
        writeBool(buffer, false);
    }
    writeVarUInt(buffer, 1);
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

constexpr int kCameraPresetOptionals = 20;

/// The four, in the order their numbers are read back.
constexpr std::array<std::string_view, 4> kCameraPresets{
    "minecraft:first_person",
    "minecraft:free",
    "minecraft:third_person",
    "minecraft:third_person_front",
};

void writeAbsent(io::ByteBuffer& buffer) { writeBool(buffer, false); }

} // namespace

void writeCameraPresets(io::ByteBuffer& buffer, const int protocol) {
    writeVarUInt(buffer, static_cast<std::uint32_t>(kCameraPresets.size()));
    for (const std::string_view named : kCameraPresets) {
        writeString(buffer, named);
        writeString(buffer, "");
        for (int at = 0; at < kCameraPresetOptionals; ++at) {
            writeAbsent(buffer);
        }
        if (protocol >= kCameraStartRotationProtocol) {
            writeBool(buffer, false);
            writeAbsent(buffer);
        }
    }
}

namespace {

void writeCameraRest(io::ByteBuffer& buffer) {
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeAbsent(buffer);
}

} // namespace

void writeCameraSet(io::ByteBuffer& buffer, const BedrockCamera& camera) {
    writeBool(buffer, true);
    writeIntLE(buffer, static_cast<std::int32_t>(camera.preset));
    writeBool(buffer, true);
    buffer.writeByte(0);
    writeFloatLE(buffer, camera.easeSeconds);
    writeBool(buffer, true);
    writeFloatLE(buffer, camera.x);
    writeFloatLE(buffer, camera.y);
    writeFloatLE(buffer, camera.z);
    writeBool(buffer, true);
    writeFloatLE(buffer, camera.pitch);
    writeFloatLE(buffer, camera.yaw);
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeAbsent(buffer);
    writeBool(buffer, false);
    writeAbsent(buffer);
    writeCameraRest(buffer);
}

void writeCameraClear(io::ByteBuffer& buffer, const bool asDefault) {
    if (asDefault) {
        writeBool(buffer, true);
        writeIntLE(buffer, static_cast<std::int32_t>(BedrockCameraPreset::FirstPerson));
        writeAbsent(buffer);
        writeAbsent(buffer);
        writeAbsent(buffer);
        writeAbsent(buffer);
        writeAbsent(buffer);
        writeAbsent(buffer);
        writeBool(buffer, true);
        writeBool(buffer, true);
        writeBool(buffer, false);
        writeAbsent(buffer);
        writeCameraRest(buffer);
        return;
    }
    writeAbsent(buffer);
    writeBool(buffer, true);
    writeBool(buffer, true);
    writeCameraRest(buffer);
}

} // namespace basalt::wire::bedrock
