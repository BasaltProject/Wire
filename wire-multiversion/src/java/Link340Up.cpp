#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>
#include <basalt/wire/multiversion/java/Link340Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
namespace flat = flattening340;
using protocol::ProtocolState;

constexpr std::int32_t kAwardStats = 0x07;
constexpr std::int32_t kSuggestions = 0x0E;
constexpr std::int32_t kWindowItems = 0x14;
constexpr std::int32_t kWindowProperty = 0x15;
constexpr std::int32_t kSetSlot = 0x16;
constexpr std::int32_t kCooldown = 0x17;
constexpr std::int32_t kCustomPayload = 0x18;
constexpr std::int32_t kNamedSound = 0x19;
constexpr std::int32_t kJoinGame = 0x23;
constexpr std::int32_t kMap = 0x24;
constexpr std::int32_t kGhostRecipe = 0x2B;
constexpr std::int32_t kUnlockRecipes = 0x31;
constexpr std::int32_t kRespawn = 0x35;
constexpr std::int32_t kEquipment = 0x3F;
constexpr std::int32_t kObjective = 0x42;
constexpr std::int32_t kTeams = 0x44;
constexpr std::int32_t kSound = 0x49;
constexpr std::int32_t kAdvancements = 0x4D;
constexpr std::int32_t kEffect = 0x4F;

constexpr std::int32_t kBossBar = 0x0C;
constexpr std::int32_t kChat = 0x0F;
constexpr std::int32_t kOpenWindow = 0x13;
constexpr std::int32_t kDisconnect = 0x1A;
constexpr std::int32_t kCombat = 0x2D;
constexpr std::int32_t kTitle = 0x48;
constexpr std::int32_t kTabList = 0x4A;

constexpr std::int32_t kCommands113 = 0x11;
constexpr std::int32_t kStopSound113 = 0x4C;
constexpr std::int32_t kTags113 = 0x55;

constexpr std::int32_t kSuggestionRequest = 0x01;
constexpr std::int32_t kClickWindow = 0x07;
constexpr std::int32_t kServerPayload = 0x09;
constexpr std::int32_t kPlaceRecipe = 0x12;
constexpr std::int32_t kRecipeBook = 0x17;
constexpr std::int32_t kCreativeSlot = 0x1B;

constexpr std::int32_t kEditBook113 = 0x0B;
constexpr std::int32_t kPickItem113 = 0x15;
constexpr std::int32_t kRenameItem113 = 0x1C;
constexpr std::int32_t kSelectTrade113 = 0x1F;
constexpr std::int32_t kSetBeacon113 = 0x20;
constexpr std::int32_t kCommandBlock113 = 0x22;
constexpr std::int32_t kCommandMinecart113 = 0x23;

constexpr std::int32_t kOverworld = 0;
constexpr std::uint8_t kShowIcon = 0x04;
constexpr std::int32_t kNoColour = -1;
constexpr std::int32_t kResetColour = 21;
constexpr std::int16_t kFirstEnchantmentProperty = 4;
constexpr std::int16_t kLastEnchantmentProperty = 6;

constexpr std::array<std::pair<std::string_view, std::string_view>, 8> kChannels = {{
    {"MC|Brand", "minecraft:brand"},
    {"MC|TrList", "minecraft:trader_list"},
    {"MC|BOpen", "minecraft:book_open"},
    {"MC|DebugPath", "minecraft:debug/paths"},
    {"MC|DebugNeighborsUpdate", "minecraft:debug/neighbors_update"},
    {"REGISTER", "minecraft:register"},
    {"UNREGISTER", "minecraft:unregister"},
    {"BungeeCord", "bungeecord:main"},
}};

constexpr std::array<std::string_view, 10> kSoundSources = {"master",  "music",   "record", "weather", "block",
                                                            "hostile", "neutral", "player", "ambient", "voice"};

[[nodiscard]] std::string_view channelUp(std::string_view legacy) {
    for (const auto& [old, modern] : kChannels) {
        if (old == legacy) {
            return modern;
        }
    }
    return legacy.find(':') != std::string_view::npos ? legacy : std::string_view{};
}

[[nodiscard]] std::string_view channelDown(std::string_view modern) {
    for (const auto& [old, renamed] : kChannels) {
        if (renamed == modern) {
            return old;
        }
    }
    return modern;
}

[[nodiscard]] std::vector<std::uint8_t> renamedChannels(std::span<const std::uint8_t> bytes, bool up) {
    const std::string all{bytes.begin(), bytes.end()};
    std::string out;
    std::string_view rest{all};
    while (!rest.empty()) {
        const std::size_t end = rest.find('\0');
        const std::string_view one = rest.substr(0, end);
        const std::string_view renamed = up ? channelUp(one) : channelDown(one);
        if (!renamed.empty()) {
            if (!out.empty()) {
                out += '\0';
            }
            out += renamed;
        }
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
    }
    return {out.begin(), out.end()};
}

[[nodiscard]] std::string renamedSound(std::string_view name) {
    constexpr std::string_view kNamespace = "minecraft:";
    const std::string_view bare = name.starts_with(kNamespace) ? name.substr(kNamespace.size()) : name;
    const auto renames = flat::soundNames();
    const auto found = std::ranges::lower_bound(renames, bare, {}, &flat::Rename::from);
    if (found != renames.end() && found->from == bare) {
        return std::string{found->to};
    }
    return std::string{name};
}

void writeStack(io::ByteBuffer& in, io::ByteBuffer& out) { content340::upStack(in, out); }

/// 1.12.2 has no command tree: one greedy argument leaves every command to the server.
void writeCommandsAndTags(MutablePacket& packet) {
    constexpr std::uint8_t kRootNode = 0;
    constexpr std::uint8_t kArgumentExecutableSuggested = 0x02 | 0x04 | 0x10;
    constexpr std::int32_t kGreedy = 2;
    io::ByteBuffer out;
    pv::varInt::write(out, 2);
    out.writeByte(kRootNode);
    pv::varInt::write(out, 1);
    pv::varInt::write(out, 1);
    out.writeByte(kArgumentExecutableSuggested);
    pv::varInt::write(out, 0);
    pv::writeString(out, "args");
    pv::writeString(out, "brigadier:string");
    pv::varInt::write(out, kGreedy);
    pv::writeString(out, "minecraft:ask_server");
    pv::varInt::write(out, 0);
    packet.forwarded.emplace_back(kCommands113, out.toVector());
    const auto tags = flat::tags();
    packet.forwarded.emplace_back(kTags113, std::vector<std::uint8_t>{tags.begin(), tags.end()});
}

/// Rewrites the chat component the reader stands at, keeping everything before and after it.
void renameText(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::string json = pv::readString(in);
    const std::string renamed = content340::renamedKeys(json);
    pv::writeString(out, renamed.empty() ? json : renamed);
}

/// The chat components in the packets 1.12 sends them in, translation keys renamed for 1.13.
[[nodiscard]] bool renameTexts(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kChat:
    case kDisconnect:
        renameText(in, out);
        break;
    case kTabList:
        renameText(in, out);
        renameText(in, out);
        break;
    case kOpenWindow:
        out.writeByte(in.readByte());
        pv::writeString(out, pv::readString(in));
        renameText(in, out);
        break;
    case kTitle: {
        const std::int32_t action = pv::varInt::read(in);
        pv::varInt::write(out, action);
        if (action <= 2) {
            renameText(in, out);
        }
        break;
    }
    case kBossBar: {
        out.writeBytes(in.readSpan(16));
        const std::int32_t action = pv::varInt::read(in);
        pv::varInt::write(out, action);
        if (action == 0 || action == 3) {
            renameText(in, out);
        }
        break;
    }
    case kCombat: {
        const std::int32_t event = pv::varInt::read(in);
        pv::varInt::write(out, event);
        if (event == 2) {
            pv::varInt::write(out, pv::varInt::read(in));
            out.writeInt(in.readInt());
            renameText(in, out);
        }
        break;
    }
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    out.writeBytes(in.readable());
    packet.body = out.toVector();
    return true;
}

} // namespace

void Link340Up::joined(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    static_cast<void>(in.readInt());
    static_cast<void>(in.readByte());
    skyLight_ = in.readInt() == kOverworld;
    writeCommandsAndTags(packet);
}

bool Link340Up::items(MutablePacket& packet) const {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kWindowItems: {
        out.writeByte(in.readByte());
        const std::int16_t count = in.readShort();
        out.writeShort(count);
        for (std::int16_t slot = 0; slot < count && !in.failed(); ++slot) {
            writeStack(in, out);
        }
        break;
    }
    case kSetSlot:
        out.writeByte(in.readByte());
        out.writeShort(in.readShort());
        writeStack(in, out);
        break;
    case kEquipment:
        pv::varInt::write(out, pv::varInt::read(in));
        pv::varInt::write(out, pv::varInt::read(in));
        writeStack(in, out);
        break;
    case kCooldown: {
        const std::int32_t item = content340::upItem(pv::varInt::read(in), 0);
        if (item == IdMap::kNone) {
            return false;
        }
        pv::varInt::write(out, item);
        pv::varInt::write(out, pv::varInt::read(in));
        break;
    }
    case kWindowProperty: {
        out.writeByte(in.readByte());
        const std::int16_t property = in.readShort();
        std::int16_t value = in.readShort();
        if (property >= kFirstEnchantmentProperty && property <= kLastEnchantmentProperty && value >= 0) {
            value = static_cast<std::int16_t>(flat::enchantmentIds().map(value));
        }
        out.writeShort(property);
        out.writeShort(value);
        break;
    }
    case kAdvancements: {
        out.writeByte(in.readByte());
        const std::int32_t count = pv::readCollectionSize(in);
        pv::varInt::write(out, count);
        for (std::int32_t entry = 0; entry < count && !in.failed(); ++entry) {
            pv::writeString(out, pv::readString(in));
            const bool parent = pv::readBoolean(in);
            pv::writeBoolean(out, parent);
            if (parent) {
                pv::writeString(out, pv::readString(in));
            }
            const bool display = pv::readBoolean(in);
            pv::writeBoolean(out, display);
            if (display) {
                pv::writeString(out, pv::readString(in));
                pv::writeString(out, pv::readString(in));
                writeStack(in, out);
                pv::varInt::write(out, pv::varInt::read(in));
                const std::int32_t flags = in.readInt();
                out.writeInt(flags);
                if ((flags & 1) != 0) {
                    pv::writeString(out, pv::readString(in));
                }
                out.writeFloat(in.readFloat());
                out.writeFloat(in.readFloat());
            }
            const std::int32_t criteria = pv::readCollectionSize(in);
            pv::varInt::write(out, criteria);
            for (std::int32_t one = 0; one < criteria && !in.failed(); ++one) {
                pv::writeString(out, pv::readString(in));
            }
            const std::int32_t requirements = pv::readCollectionSize(in);
            pv::varInt::write(out, requirements);
            for (std::int32_t one = 0; one < requirements && !in.failed(); ++one) {
                const std::int32_t names = pv::readCollectionSize(in);
                pv::varInt::write(out, names);
                for (std::int32_t name = 0; name < names && !in.failed(); ++name) {
                    pv::writeString(out, pv::readString(in));
                }
            }
        }
        out.writeBytes(in.readable());
        break;
    }
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link340Up::customPayload(MutablePacket& packet) const {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::string channel = pv::readString(in);
    if (in.failed()) {
        return false;
    }
    io::ByteBuffer out;
    if (channel == "MC|StopSound") {
        const std::string source = pv::readString(in);
        const std::string sound = pv::readString(in);
        const auto found = std::ranges::find(kSoundSources, source);
        const std::uint8_t flags = (source.empty() ? 0 : 1) | (sound.empty() ? 0 : 2);
        out.writeByte(flags);
        if (!source.empty()) {
            pv::varInt::write(
                out, found == kSoundSources.end() ? 0 : static_cast<std::int32_t>(found - kSoundSources.begin()));
        }
        if (!sound.empty()) {
            pv::writeString(out, renamedSound(sound));
        }
        packet.body = out.toVector();
        packet.overrideId(kStopSound113);
        return !in.failed();
    }
    const std::string_view renamed = channelUp(channel);
    if (renamed.empty()) {
        return false;
    }
    pv::writeString(out, renamed);
    if (channel == "MC|TrList") {
        out.writeInt(in.readInt());
        const std::uint8_t trades = in.readByte();
        out.writeByte(trades);
        for (std::uint8_t trade = 0; trade < trades && !in.failed(); ++trade) {
            writeStack(in, out);
            writeStack(in, out);
            const bool second = pv::readBoolean(in);
            pv::writeBoolean(out, second);
            if (second) {
                writeStack(in, out);
            }
            out.writeByte(in.readByte());
            out.writeInt(in.readInt());
            out.writeInt(in.readInt());
        }
    } else if (channel == "REGISTER" || channel == "UNREGISTER") {
        const std::vector<std::uint8_t> names = renamedChannels(in.readable(), true);
        if (names.empty()) {
            return false;
        }
        out.writeBytes(names);
    } else {
        out.writeBytes(in.readable());
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link340Up::scoreboard(MutablePacket& packet) const {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    pv::writeString(out, pv::readString(in));
    const std::uint8_t mode = in.readByte();
    out.writeByte(mode);
    if (packet.id == kObjective) {
        if (mode == 0 || mode == 2) {
            pv::writeString(out, content340::legacyToJson(pv::readString(in)));
            pv::varInt::write(out, pv::readString(in) == "hearts" ? 1 : 0);
        }
    } else {
        if (mode == 0 || mode == 2) {
            const std::string display = pv::readString(in);
            const std::string prefix = pv::readString(in);
            const std::string suffix = pv::readString(in);
            pv::writeString(out, content340::legacyToJson(display));
            out.writeByte(in.readByte());
            pv::writeString(out, pv::readString(in));
            pv::writeString(out, pv::readString(in));
            const std::int32_t colour = static_cast<std::int8_t>(in.readByte());
            pv::varInt::write(out, colour == kNoColour ? kResetColour : colour);
            pv::writeString(out, content340::legacyToJson(prefix));
            pv::writeString(out, content340::legacyToJson(suffix));
        }
        out.writeBytes(in.readable());
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link340Up::suggestions(MutablePacket& packet) const {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    const std::size_t space = suggestionInput_.rfind(' ');
    std::int32_t start = space == std::string::npos ? 0 : static_cast<std::int32_t>(space) + 1;
    const std::int32_t count = pv::readCollectionSize(in);
    std::vector<std::string> matches;
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        matches.push_back(pv::readString(in));
    }
    if (in.failed()) {
        return false;
    }
    if (start == 0 && suggestionInput_.starts_with('/') &&
        std::ranges::none_of(matches, [](const std::string& one) { return one.starts_with('/'); })) {
        start = 1;
    }
    pv::varInt::write(out, suggestionId_);
    pv::varInt::write(out, start);
    pv::varInt::write(out, static_cast<std::int32_t>(suggestionInput_.size()) - start);
    pv::varInt::write(out, count);
    for (const std::string& one : matches) {
        pv::writeString(out, one);
        pv::writeBoolean(out, false);
    }
    packet.body = out.toVector();
    return true;
}

bool Link340Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kAwardStats:
    case kGhostRecipe:
    case kUnlockRecipes:
        return false;
    case kJoinGame:
        joined(packet);
        return true;
    case kRespawn: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        skyLight_ = in.readInt() == kOverworld;
        kinds_.clear();
        world_.clear();
        legacyAt_.clear();
        variantAt_.clear();
        return true;
    }
    case kWindowItems:
    case kWindowProperty:
    case kSetSlot:
    case kCooldown:
    case kEquipment:
    case kAdvancements:
        return items(packet);
    case kCustomPayload:
        return customPayload(packet);
    case kObjective:
    case kTeams:
        return scoreboard(packet);
    case kSuggestions:
        return suggestions(packet);
    case kSound: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const std::int32_t sound = flat::sounds().map(pv::varInt::read(in));
        if (in.failed() || sound == IdMap::kNone) {
            return false;
        }
        io::ByteBuffer out;
        pv::varInt::write(out, sound);
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        return true;
    }
    case kNamedSound: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        pv::writeString(out, renamedSound(pv::readString(in)));
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        return !in.failed();
    }
    case kChat:
    case kDisconnect:
    case kTabList:
    case kOpenWindow:
    case kTitle:
    case kBossBar:
    case kCombat:
        return renameTexts(packet);
    case kEffect:
        if (packet.body.size() > 1) {
            packet.body.back() |= kShowIcon;
        }
        return true;
    case kMap: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeByte(in.readByte());
        out.writeByte(in.readByte());
        const std::int32_t icons = pv::readCollectionSize(in, 3);
        pv::varInt::write(out, icons);
        for (std::int32_t icon = 0; icon < icons && !in.failed(); ++icon) {
            const std::uint8_t typeAndDirection = in.readByte();
            pv::varInt::write(out, typeAndDirection >> 4);
            out.writeByte(in.readByte());
            out.writeByte(in.readByte());
            out.writeByte(typeAndDirection & 0x0F);
            pv::writeBoolean(out, false);
        }
        out.writeBytes(in.readable());
        packet.body = out.toVector();
        return !in.failed();
    }
    default:
        return world(packet) && entities(packet);
    }
}

bool Link340Up::serverboundForeign(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return false;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kEditBook113: {
        io::ByteBuffer book;
        content340::downStack(in, book);
        pv::writeString(out, pv::readBoolean(in) ? "MC|BSign" : "MC|BEdit");
        out.writeBytes(book.readable());
        break;
    }
    case kPickItem113:
        pv::writeString(out, "MC|PickItem");
        pv::varInt::write(out, pv::varInt::read(in));
        break;
    case kRenameItem113:
        pv::writeString(out, "MC|ItemName");
        pv::writeString(out, pv::readString(in));
        break;
    case kSelectTrade113:
        pv::writeString(out, "MC|TrSel");
        out.writeInt(pv::varInt::read(in));
        break;
    case kSetBeacon113:
        pv::writeString(out, "MC|Beacon");
        out.writeInt(pv::varInt::read(in));
        out.writeInt(pv::varInt::read(in));
        break;
    case kCommandBlock113: {
        const std::int64_t position = in.readLong();
        const std::string command = pv::readString(in);
        const std::int32_t mode = pv::varInt::read(in);
        const std::uint8_t flags = in.readByte();
        pv::writeString(out, "MC|AutoCmd");
        out.writeInt(static_cast<std::int32_t>(position >> 38));
        out.writeInt(static_cast<std::int32_t>((position >> 26) & 0xFFF));
        out.writeInt(static_cast<std::int32_t>((position << 38) >> 38));
        pv::writeString(out, command);
        pv::writeBoolean(out, (flags & 1) != 0);
        pv::writeString(out, mode == 0 ? "SEQUENCE" : mode == 1 ? "AUTO" : "REDSTONE");
        pv::writeBoolean(out, (flags & 2) != 0);
        pv::writeBoolean(out, (flags & 4) != 0);
        break;
    }
    case kCommandMinecart113:
        pv::writeString(out, "MC|AdvCmd");
        out.writeByte(1);
        out.writeInt(pv::varInt::read(in));
        pv::writeString(out, pv::readString(in));
        pv::writeBoolean(out, pv::readBoolean(in));
        break;
    default:
        return false;
    }
    if (in.failed()) {
        return false;
    }
    packet.id = kServerPayload;
    packet.body = out.toVector();
    return true;
}

bool Link340Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kSuggestionRequest: {
        suggestionId_ = pv::varInt::read(in);
        suggestionInput_ = pv::readString(in);
        pv::writeString(out, suggestionInput_.starts_with('/') ? suggestionInput_ : "/" + suggestionInput_);
        pv::writeBoolean(out, false);
        pv::writeBoolean(out, false);
        break;
    }
    case kClickWindow:
        out.writeByte(in.readByte());
        out.writeShort(in.readShort());
        out.writeByte(in.readByte());
        out.writeShort(in.readShort());
        pv::varInt::write(out, pv::varInt::read(in));
        content340::downStack(in, out);
        break;
    case kCreativeSlot:
        out.writeShort(in.readShort());
        content340::downStack(in, out);
        break;
    case kServerPayload: {
        const std::string channel = pv::readString(in);
        const std::string_view legacy = channelDown(channel);
        pv::writeString(out, legacy);
        if (legacy == "REGISTER" || legacy == "UNREGISTER") {
            out.writeBytes(renamedChannels(in.readable(), false));
        } else {
            out.writeBytes(in.readable());
        }
        break;
    }
    case kRecipeBook: {
        const std::int32_t type = pv::varInt::read(in);
        if (type != 1) {
            return false;
        }
        pv::varInt::write(out, type);
        out.writeByte(in.readByte());
        out.writeByte(in.readByte());
        break;
    }
    case kPlaceRecipe:
        return false;
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
