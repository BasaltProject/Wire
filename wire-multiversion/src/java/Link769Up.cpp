#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/CommandParsers.hpp>
#include <basalt/wire/multiversion/java/Link769Up.hpp>
#include <basalt/wire/multiversion/java/NewRegistries770.hpp>
#include <basalt/wire/multiversion/java/Stack769.hpp>
#include <basalt/wire/multiversion/java/Tables769.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
namespace configuration = protocol::configuration;
using protocol::ProtocolState;

constexpr std::int32_t kConfigDisconnect = 2;
constexpr std::int32_t kConfigFinish = 3;
constexpr std::int32_t kConfigRegistryData = 7;

constexpr std::int32_t kAddEntity = 1;
constexpr std::int32_t kAddExperienceOrb = 2;
constexpr std::int32_t kBlockEntityData = 7;
constexpr std::int32_t kBossEvent = 10;
constexpr std::int32_t kCommands = 17;
constexpr std::int32_t kSetEquipment = 96;
constexpr std::int32_t kMerchantOffers = 46;
constexpr std::int32_t kRecipeBookAdd = 68;
constexpr std::int32_t kContainerSetContent = 19;
constexpr std::int32_t kContainerSetSlot = 21;
constexpr std::int32_t kDisconnect = 29;
constexpr std::int32_t kDisguisedChat = 30;
constexpr std::int32_t kLevelChunk = 40;
constexpr std::int32_t kLogin = 44;
constexpr std::int32_t kOpenScreen = 53;
constexpr std::int32_t kPlayerChat = 59;
constexpr std::int32_t kPlayerCombatKill = 62;
constexpr std::int32_t kSetActionBar = 81;
constexpr std::int32_t kSetCursorItem = 90;
constexpr std::int32_t kSetPlayerInventory = 102;
constexpr std::int32_t kSetObjective = 100;
constexpr std::int32_t kSetPlayerTeam = 103;
constexpr std::int32_t kSetScore = 104;
constexpr std::int32_t kSetSubtitle = 106;
constexpr std::int32_t kSetTitle = 108;
constexpr std::int32_t kSystemChat = 115;
constexpr std::int32_t kTabList = 116;
constexpr std::int32_t kUpdateAdvancements = 123;
constexpr std::int32_t kUpdateMobEffect = 125;

constexpr std::int32_t kChat = 7;
constexpr std::int32_t kChatCommandSigned = 6;
constexpr std::int32_t kContainerClick = 16;
constexpr std::int32_t kSetCreativeModeSlot = 54;

constexpr std::int32_t kExperienceOrbType = 47;
constexpr std::int32_t kNauseaEffect = 8;
constexpr std::uint8_t kBlendFlag = 0x08;
constexpr std::int32_t kSignBlockEntity = 7;
constexpr std::int32_t kHangingSignBlockEntity = 8;
constexpr std::size_t kSignatureBytes = 256;

template <typename Packet>
void retext(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    auto value = Packet::read(input);
    if (!value.has_value()) {
        return;
    }
    io::ByteBuffer output;
    value->write(output);
    packet.body = output.toVector();
}

void copyText(io::ByteBuffer& in, io::ByteBuffer& out) {
    protocol::writeAnonymousNbt(
        out, text::componentNbt::serialize(text::componentNbt::deserialize(protocol::readAnonymousNbt(in))));
}

void copyOptionalText(io::ByteBuffer& in, io::ByteBuffer& out) {
    const bool present = protocol::readBoolean(in);
    protocol::writeBoolean(out, present);
    if (present) {
        copyText(in, out);
    }
}

void copyVarInt(io::ByteBuffer& in, io::ByteBuffer& out) { protocol::varInt::write(out, protocol::varInt::read(in)); }

void copyString(io::ByteBuffer& in, io::ByteBuffer& out) { protocol::writeString(out, protocol::readString(in)); }

void copyBytes(io::ByteBuffer& in, io::ByteBuffer& out, std::size_t count) { out.writeBytes(in.readSpan(count)); }

nbt::NbtTag textFromJson(std::string_view json) {
    auto parsed = common::parseJson(json);
    if (!parsed.has_value()) {
        return text::componentNbt::serialize(text::Component{});
    }
    return text::componentNbt::serialize(text::componentJson::deserialize(*parsed));
}

void convertSignMessages(nbt::NbtCompound& side) {
    for (const std::string_view key : {"messages", "filtered_messages"}) {
        const nbt::NbtList* list = side.getList(key);
        if (list == nullptr || list->elementType() != nbt::NbtType::String) {
            continue;
        }
        nbt::NbtList converted{nbt::NbtType::Compound};
        for (const nbt::NbtTag& message : list->elements()) {
            const std::string* json = message.get<std::string>();
            const nbt::NbtTag text = textFromJson(json == nullptr ? std::string_view{} : std::string_view{*json});
            converted.push(text);
        }
        side.set(key, nbt::NbtTag::ofList(std::move(converted)));
    }
}

void convertBlockEntity(std::int32_t type, nbt::NbtCompound& tag) {
    if (type == kSignBlockEntity || type == kHangingSignBlockEntity) {
        for (const std::string_view side : {"front_text", "back_text"}) {
            if (nbt::NbtTag* found = tag.find(side)) {
                if (auto* compound = found->get<nbt::NbtCompound>()) {
                    convertSignMessages(*compound);
                }
            }
        }
    }
    if (const nbt::NbtTag* name = tag.find("CustomName")) {
        if (const std::string* json = name->get<std::string>()) {
            tag.set("CustomName", textFromJson(*json));
        }
    }
}

std::int32_t heightmapType(std::string_view name) {
    for (std::int32_t type = 0; type <= static_cast<std::int32_t>(play::HeightmapType::MotionBlockingNoLeaves);
         ++type) {
        if (play::heightmapName(static_cast<play::HeightmapType>(type)) == name) {
            return type;
        }
    }
    return -1;
}

void rewriteLevelChunk(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    out.writeInt(in.readInt());
    out.writeInt(in.readInt());
    const nbt::NbtTag heightmaps = protocol::readAnonymousNbt(in);
    const auto* compound = heightmaps.get<nbt::NbtCompound>();
    if (in.failed() || compound == nullptr) {
        return;
    }
    std::vector<std::pair<std::int32_t, std::span<const std::int64_t>>> kept;
    for (const std::string& name : compound->names()) {
        const std::int32_t type = heightmapType(name);
        const auto data = compound->getLongArray(name);
        if (type >= 0) {
            kept.emplace_back(type, data);
        }
    }
    protocol::varInt::write(out, static_cast<std::int32_t>(kept.size()));
    for (const auto& [type, data] : kept) {
        protocol::varInt::write(out, type);
        protocol::varInt::write(out, static_cast<std::int32_t>(data.size()));
        for (const std::int64_t word : data) {
            out.writeLong(word);
        }
    }
    protocol::writeByteArray(out, protocol::readByteArray(in));
    const std::int32_t blockEntities = protocol::readCollectionSize(in, 5);
    protocol::varInt::write(out, blockEntities);
    for (std::int32_t i = 0; i < blockEntities && !in.failed(); ++i) {
        auto info = play::ChunkBlockEntityInfo::read(in);
        if (!info.has_value()) {
            return;
        }
        if (info->tag.has_value()) {
            convertBlockEntity(info->typeId, *info->tag);
        }
        info->write(out);
    }
    out.writeBytes(in.readable());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteBlockEntityData(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    out.writeLong(in.readLong());
    const std::int32_t type = protocol::varInt::read(in);
    protocol::varInt::write(out, type);
    const nbt::NbtTag tag = protocol::readAnonymousNbt(in);
    if (in.failed()) {
        return;
    }
    if (const auto* compound = tag.get<nbt::NbtCompound>()) {
        nbt::NbtCompound converted = *compound;
        convertBlockEntity(type, converted);
        protocol::writeAnonymousNbt(out, nbt::NbtTag::ofCompound(std::move(converted)));
    } else {
        protocol::writeAnonymousNbt(out, tag);
    }
    packet.body = out.toVector();
}

void rewritePlayerChat(MutablePacket& packet, std::int32_t index) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    protocol::varInt::write(out, index);
    copyBytes(in, out, 16);
    copyVarInt(in, out);
    const bool signature = protocol::readBoolean(in);
    protocol::writeBoolean(out, signature);
    if (signature) {
        copyBytes(in, out, kSignatureBytes);
    }
    copyString(in, out);
    out.writeLong(in.readLong());
    out.writeLong(in.readLong());
    const std::int32_t lastSeen = protocol::readCollectionSize(in);
    protocol::varInt::write(out, lastSeen);
    for (std::int32_t i = 0; i < lastSeen && !in.failed(); ++i) {
        const std::int32_t id = protocol::varInt::read(in);
        protocol::varInt::write(out, id);
        if (id == 0) {
            copyBytes(in, out, kSignatureBytes);
        }
    }
    copyOptionalText(in, out);
    const std::int32_t mask = protocol::varInt::read(in);
    protocol::varInt::write(out, mask);
    if (mask == 2) {
        const std::int32_t words = protocol::readCollectionSize(in, 8);
        protocol::varInt::write(out, words);
        for (std::int32_t i = 0; i < words && !in.failed(); ++i) {
            out.writeLong(in.readLong());
        }
    }
    copyVarInt(in, out);
    copyText(in, out);
    copyOptionalText(in, out);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

std::int32_t visibilityId(std::string_view rule) {
    if (rule == "never") {
        return 1;
    }
    if (rule == "hideForOtherTeams") {
        return 2;
    }
    return rule == "hideForOwnTeam" ? 3 : 0;
}

std::int32_t collisionId(std::string_view rule) {
    if (rule == "never") {
        return 1;
    }
    if (rule == "pushOtherTeams") {
        return 2;
    }
    return rule == "pushOwnTeam" ? 3 : 0;
}

void rewritePlayerTeam(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyString(in, out);
    const std::uint8_t method = in.readByte();
    out.writeByte(method);
    if (method == 0 || method == 2) {
        copyText(in, out);
        out.writeByte(in.readByte());
        protocol::varInt::write(out, visibilityId(protocol::readString(in)));
        protocol::varInt::write(out, collisionId(protocol::readString(in)));
        copyVarInt(in, out);
        copyText(in, out);
        copyText(in, out);
    }
    out.writeBytes(in.readable());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteMobEffect(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyVarInt(in, out);
    const std::int32_t effect = protocol::varInt::read(in);
    protocol::varInt::write(out, effect);
    copyVarInt(in, out);
    copyVarInt(in, out);
    std::uint8_t flags = in.readByte();
    if (effect == kNauseaEffect) {
        flags |= kBlendFlag;
    }
    out.writeByte(flags);
    out.writeBytes(in.readable());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

std::string clientAsset(const std::string& background) {
    const std::size_t colon = background.find(':');
    const std::string space = colon == std::string::npos ? "minecraft" : background.substr(0, colon);
    std::string path = colon == std::string::npos ? background : background.substr(colon + 1);
    constexpr std::string_view kFolder = "textures/";
    constexpr std::string_view kSuffix = ".png";
    if (path.starts_with(kFolder) && path.ends_with(kSuffix)) {
        path = path.substr(kFolder.size(), path.size() - kFolder.size() - kSuffix.size());
    }
    return space + ":" + path;
}

void rewriteAdvancements(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    protocol::writeBoolean(out, protocol::readBoolean(in));
    const std::int32_t added = protocol::readCollectionSize(in);
    protocol::varInt::write(out, added);
    for (std::int32_t i = 0; i < added && !in.failed(); ++i) {
        copyString(in, out);
        if (protocol::readBoolean(in)) {
            protocol::writeBoolean(out, true);
            copyString(in, out);
        } else {
            protocol::writeBoolean(out, false);
        }
        if (protocol::readBoolean(in)) {
            protocol::writeBoolean(out, true);
            copyText(in, out);
            copyText(in, out);
            if (!stack769::convert(in, out)) {
                return;
            }
            copyVarInt(in, out);
            const std::int32_t flags = in.readInt();
            out.writeInt(flags);
            if ((flags & 1) != 0) {
                protocol::writeString(out, clientAsset(protocol::readString(in)));
            }
            out.writeFloat(in.readFloat());
            out.writeFloat(in.readFloat());
        } else {
            protocol::writeBoolean(out, false);
        }
        const std::int32_t requirements = protocol::readCollectionSize(in);
        protocol::varInt::write(out, requirements);
        for (std::int32_t r = 0; r < requirements && !in.failed(); ++r) {
            const std::int32_t names = protocol::readCollectionSize(in);
            protocol::varInt::write(out, names);
            for (std::int32_t n = 0; n < names && !in.failed(); ++n) {
                copyString(in, out);
            }
        }
        protocol::writeBoolean(out, protocol::readBoolean(in));
    }
    out.writeBytes(in.readable());
    protocol::writeBoolean(out, true);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

constexpr std::int32_t kFirstShiftedParser = 47;
constexpr std::int32_t kLastRegistryParser = 46;

std::int32_t shiftedParser(std::int32_t parser) { return parser >= kFirstShiftedParser ? parser + 1 : parser; }

bool copyCost(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyVarInt(in, out);
    copyVarInt(in, out);
    const std::int32_t exact = protocol::readCollectionSize(in, 2);
    protocol::varInt::write(out, exact);
    for (std::int32_t i = 0; i < exact && !in.failed(); ++i) {
        copyVarInt(in, out);
        copyVarInt(in, out);
    }
    return !in.failed();
}

void rewriteMerchantOffers(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyVarInt(in, out);
    const std::int32_t offers = protocol::readCollectionSize(in);
    protocol::varInt::write(out, offers);
    for (std::int32_t i = 0; i < offers && !in.failed(); ++i) {
        if (!copyCost(in, out) || !stack769::convert(in, out)) {
            return;
        }
        const bool second = protocol::readBoolean(in);
        protocol::writeBoolean(out, second);
        if (second && !copyCost(in, out)) {
            return;
        }
        out.writeBytes(in.readSpan(1 + 4 * 5 + 4));
    }
    out.writeBytes(in.readable());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteEquipment(MutablePacket& packet) {
    constexpr std::uint8_t kContinue = 0x80;
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyVarInt(in, out);
    std::uint8_t slot = kContinue;
    while ((slot & kContinue) != 0 && !in.failed()) {
        slot = in.readByte();
        out.writeByte(slot);
        if (!stack769::convert(in, out)) {
            return;
        }
    }
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteCursor(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    if (stack769::convert(in, out) && !in.failed()) {
        packet.body = out.toVector();
    }
}

void rewritePlayerInventory(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyVarInt(in, out);
    if (stack769::convert(in, out) && !in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteSlot(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyVarInt(in, out);
    copyVarInt(in, out);
    out.writeShort(in.readShort());
    if (stack769::convert(in, out) && !in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteContent(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyVarInt(in, out);
    copyVarInt(in, out);
    const std::int32_t count = protocol::readCollectionSize(in);
    protocol::varInt::write(out, count);
    for (std::int32_t i = 0; i < count && !in.failed(); ++i) {
        if (!stack769::convert(in, out)) {
            return;
        }
    }
    if (stack769::convert(in, out) && !in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteExperienceOrb(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyVarInt(in, out);
    out.writeLong(0);
    out.writeLong(0);
    protocol::varInt::write(out, kExperienceOrbType);
    out.writeDouble(in.readDouble());
    out.writeDouble(in.readDouble());
    out.writeDouble(in.readDouble());
    out.writeByte(0);
    out.writeByte(0);
    out.writeByte(0);
    protocol::varInt::write(out, in.readShort());
    out.writeShort(0);
    out.writeShort(0);
    out.writeShort(0);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void reshapeVariant(const std::string& registry, nbt::NbtCompound& element) {
    if (registry == "minecraft:wolf_variant") {
        if (element.contains("assets")) {
            return;
        }
        nbt::NbtCompound assets;
        assets.putString("wild", std::string{element.getString("wild_texture")});
        assets.putString("tame", std::string{element.getString("tame_texture")});
        assets.putString("angry", std::string{element.getString("angry_texture")});
        element.remove("wild_texture");
        element.remove("tame_texture");
        element.remove("angry_texture");
        element.remove("biomes");
        element.set("assets", nbt::NbtTag::ofCompound(std::move(assets)));
    } else if (registry == "minecraft:trim_pattern") {
        element.remove("template_item");
    } else if (registry == "minecraft:trim_material") {
        element.remove("ingredient");
    }
}

void rewriteRegistry(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    auto data = configuration::RegistryDataPacket::read(in);
    if (!data.has_value()) {
        return;
    }
    for (configuration::RegistryEntry& entry : data->entries) {
        nbt::NbtCompound* element = entry.value ? entry.value->get<nbt::NbtCompound>() : nullptr;
        if (element != nullptr) {
            reshapeVariant(data->registryId, *element);
        }
    }
    io::ByteBuffer out;
    data->write(out);
    packet.body = out.toVector();
}

} // namespace

Link769Up::Link769Up(const VersionTables& upward, std::int32_t backendVersion)
    : RenumberingLink(upward), intModelData_(backendVersion < protocolVersions::kV1_21_4) {}

bool Link769Up::clientbound(ProtocolState state, MutablePacket& packet) {
    const stack769::IntModelDataScope scope(intModelData_);
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration) {
        messageIndex_ = 0;
        if (packet.id == kConfigRegistryData) {
            rewriteRegistry(packet);
        } else if (packet.id == kConfigFinish) {
            const auto added = registriesAddedIn1_21_5();
            const std::vector<std::uint8_t> finishBody = packet.body;
            packet.overrideId(tables().packets.toOlder(state, kConfigRegistryData));
            packet.body.assign(added[0].begin(), added[0].end());
            for (std::size_t index = 1; index < added.size(); ++index) {
                packet.extras.emplace_back(kConfigRegistryData,
                                           std::vector<std::uint8_t>{added[index].begin(), added[index].end()});
            }
            packet.extras.emplace_back(kConfigFinish, finishBody);
        } else if (packet.id == kConfigDisconnect) {
            retext<configuration::ConfigurationDisconnectPacket>(packet);
        }
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kLogin:
        messageIndex_ = 0;
        break;
    case kAddExperienceOrb:
        rewriteExperienceOrb(packet);
        packet.overrideId(tables().packets.toOlder(state, kAddEntity));
        break;
    case kLevelChunk:
        rewriteLevelChunk(packet);
        break;
    case kBlockEntityData:
        rewriteBlockEntityData(packet);
        break;
    case kPlayerChat:
        rewritePlayerChat(packet, messageIndex_++);
        break;
    case kSetPlayerTeam:
        rewritePlayerTeam(packet);
        break;
    case kUpdateMobEffect:
        rewriteMobEffect(packet);
        break;
    case kUpdateAdvancements:
        rewriteAdvancements(packet);
        break;
    case kCommands:
        static_cast<void>(commandParsers::remap(packet, kLastRegistryParser, shiftedParser));
        break;
    case kSetEquipment:
        rewriteEquipment(packet);
        break;
    case kMerchantOffers:
        rewriteMerchantOffers(packet);
        break;
    case kRecipeBookAdd:
        return false;
    case kSetCursorItem:
        rewriteCursor(packet);
        break;
    case kSetPlayerInventory:
        rewritePlayerInventory(packet);
        break;
    case kContainerSetSlot:
        rewriteSlot(packet);
        break;
    case kContainerSetContent:
        rewriteContent(packet);
        break;
    case kSystemChat:
        retext<play::SystemChatPacket>(packet);
        break;
    case kTabList:
        retext<play::TabListPacket>(packet);
        break;
    case kDisconnect:
        retext<play::PlayDisconnectPacket>(packet);
        break;
    case kOpenScreen:
        retext<play::OpenScreenPacket>(packet);
        break;
    case kBossEvent:
        retext<play::BossEventPacket>(packet);
        break;
    case kSetObjective:
        retext<play::SetObjectivePacket>(packet);
        break;
    case kSetScore:
        retext<play::SetScorePacket>(packet);
        break;
    case kDisguisedChat:
        retext<play::DisguisedChatPacket>(packet);
        break;
    case kPlayerCombatKill:
        retext<play::PlayerCombatKillPacket>(packet);
        break;
    case kSetTitle:
    case kSetSubtitle:
    case kSetActionBar:
        retext<play::TitleTextPacket>(packet);
        break;
    default:
        break;
    }
    return true;
}

bool Link769Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::serverbound(state, packet) || state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case kChat:
    case kChatCommandSigned:
        if (!packet.body.empty()) {
            packet.body.pop_back();
        }
        break;
    case kContainerClick: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        copyVarInt(in, out);
        copyVarInt(in, out);
        out.writeShort(in.readShort());
        out.writeByte(in.readByte());
        copyVarInt(in, out);
        if (in.failed()) {
            return true;
        }
        protocol::varInt::write(out, 0);
        protocol::varInt::write(out, 0);
        packet.body = out.toVector();
        break;
    }
    case kSetCreativeModeSlot: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        out.writeShort(in.readShort());
        if (stack769::convertToServer(in, out) && !in.failed()) {
            packet.body = out.toVector();
        }
        break;
    }
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
