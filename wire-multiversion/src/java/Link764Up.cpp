#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/java/CommandParsers.hpp>
#include <basalt/wire/multiversion/java/Link764Up.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kConfigDisconnect = 1;
constexpr std::int32_t kConfigResourcePack = 6;
constexpr std::int32_t kConfigPop = 6;
constexpr std::int32_t kConfigPush = 7;

constexpr std::int32_t kBossEvent = 10;
constexpr std::int32_t kCommandSuggestions = 16;
constexpr std::int32_t kCommands = 17;
constexpr std::int32_t kDisconnect = 27;
constexpr std::int32_t kDisguisedChat = 28;
constexpr std::int32_t kGameEvent = 32;
constexpr std::int32_t kInitializeBorder = 35;
constexpr std::int32_t kLogin = 41;
constexpr std::int32_t kMapItemData = 42;
constexpr std::int32_t kOpenScreen = 49;
constexpr std::int32_t kPlayerChat = 55;
constexpr std::int32_t kCombatKill = 58;
constexpr std::int32_t kPlayerInfoUpdate = 60;
constexpr std::int32_t kResourcePack = 66;
constexpr std::int32_t kRespawn = 67;
constexpr std::int32_t kServerData = 71;
constexpr std::int32_t kActionBar = 72;
constexpr std::int32_t kSetObjective = 90;
constexpr std::int32_t kSetTeam = 92;
constexpr std::int32_t kSetScore = 93;
constexpr std::int32_t kSubtitle = 95;
constexpr std::int32_t kTitle = 97;
constexpr std::int32_t kSystemChat = 103;
constexpr std::int32_t kTabList = 104;

constexpr std::int32_t kResetScore765 = 66;
constexpr std::int32_t kPop765 = 67;
constexpr std::int32_t kPush765 = 68;

constexpr std::int32_t kResourcePackStatus764 = 39;
constexpr std::int32_t kSetJigsaw764 = 47;
constexpr std::int32_t kFirstShifted = 18;
constexpr std::int32_t kLoadingEvent = 13;

std::int32_t parserIn765(std::int32_t parser) { return parser >= kFirstShifted ? parser + 1 : parser; }

void copyVarInt(io::ByteBuffer& in, io::ByteBuffer& out) { pv::varInt::write(out, pv::varInt::read(in)); }

void copyString(io::ByteBuffer& in, io::ByteBuffer& out) { pv::writeString(out, pv::readString(in)); }

void copyBytes(io::ByteBuffer& in, io::ByteBuffer& out, std::size_t count) { out.writeBytes(in.readSpan(count)); }

void convertComponent(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::string json = pv::readString(in);
    const auto parsed = common::parseJson(json);
    if (!parsed.has_value()) {
        in.markFailed();
        return;
    }
    pv::writeAnonymousNbt(out, text::componentNbt::serializeLegacy(text::componentJson::deserialize(*parsed)));
}

void convertOptionalComponent(io::ByteBuffer& in, io::ByteBuffer& out) {
    const bool present = pv::readBoolean(in);
    pv::writeBoolean(out, present);
    if (present) {
        convertComponent(in, out);
    }
}

void copyRest(io::ByteBuffer& in, io::ByteBuffer& out) { out.writeBytes(in.readable()); }

std::vector<std::uint8_t> loadingEvent() {
    io::ByteBuffer out;
    out.writeByte(kLoadingEvent);
    out.writeFloat(0.0F);
    return out.toVector();
}

std::array<std::uint8_t, 16> packId(std::string_view url) {
    std::array<std::uint8_t, 16> id{};
    std::uint64_t high = 0xcbf29ce484222325ULL;
    std::uint64_t low = 0x84222325cbf29ce4ULL;
    for (const char each : url) {
        high = (high ^ static_cast<std::uint8_t>(each)) * 0x100000001b3ULL;
        low = (low ^ static_cast<std::uint8_t>(each)) * 0x100000001b3ULL + high;
    }
    for (std::size_t index = 0; index < 8; ++index) {
        id[index] = static_cast<std::uint8_t>(high >> (56 - 8 * index));
        id[8 + index] = static_cast<std::uint8_t>(low >> (56 - 8 * index));
    }
    id[6] = static_cast<std::uint8_t>((id[6] & 0x0F) | 0x30);
    id[8] = static_cast<std::uint8_t>((id[8] & 0x3F) | 0x80);
    return id;
}

void splitResourcePack(MutablePacket& packet, std::int32_t pop, std::int32_t push) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    const std::string url = pv::readString(in);
    const std::string hash = pv::readString(in);
    const auto id = packId(url);
    out.writeBytes(id);
    pv::writeString(out, url);
    pv::writeString(out, hash);
    pv::writeBoolean(out, pv::readBoolean(in));
    convertOptionalComponent(in, out);
    if (in.failed()) {
        return;
    }
    io::ByteBuffer popBody;
    pv::writeBoolean(popBody, false);
    packet.body = popBody.toVector();
    packet.overrideId(pop);
    packet.forwarded.emplace_back(push, out.toVector());
}

void rewriteScore(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    const std::string owner = pv::readString(in);
    const std::int32_t action = pv::varInt::read(in);
    const std::string objective = pv::readString(in);
    if (in.failed()) {
        return;
    }
    pv::writeString(out, owner);
    if (action == 1) {
        pv::writeBoolean(out, !objective.empty());
        if (!objective.empty()) {
            pv::writeString(out, objective);
        }
        packet.overrideId(kResetScore765);
    } else {
        pv::writeString(out, objective);
        copyVarInt(in, out);
        pv::writeBoolean(out, false);
        pv::writeBoolean(out, false);
    }
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteObjective(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyString(in, out);
    const std::uint8_t action = in.readByte();
    out.writeByte(action);
    if (action == 0 || action == 2) {
        convertComponent(in, out);
        copyVarInt(in, out);
        pv::writeBoolean(out, false);
    }
    copyRest(in, out);
}

void rewriteTeam(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyString(in, out);
    const std::uint8_t mode = in.readByte();
    out.writeByte(mode);
    if (mode == 0 || mode == 2) {
        convertComponent(in, out);
        out.writeByte(in.readByte());
        copyString(in, out);
        copyString(in, out);
        copyVarInt(in, out);
        convertComponent(in, out);
        convertComponent(in, out);
    }
    copyRest(in, out);
}

void rewriteBossEvent(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyBytes(in, out, 16);
    const std::int32_t action = pv::varInt::read(in);
    pv::varInt::write(out, action);
    if (action == 0 || action == 3) {
        convertComponent(in, out);
    }
    copyRest(in, out);
}

void rewriteSuggestions(io::ByteBuffer& in, io::ByteBuffer& out) {
    for (int index = 0; index < 3; ++index) {
        copyVarInt(in, out);
    }
    const std::int32_t count = pv::readCollectionSize(in, 2);
    pv::varInt::write(out, count);
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        copyString(in, out);
        convertOptionalComponent(in, out);
    }
}

void rewriteMap(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyVarInt(in, out);
    copyBytes(in, out, 2);
    const bool icons = pv::readBoolean(in);
    pv::writeBoolean(out, icons);
    if (icons) {
        const std::int32_t count = pv::readCollectionSize(in, 5);
        pv::varInt::write(out, count);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            copyVarInt(in, out);
            copyBytes(in, out, 3);
            convertOptionalComponent(in, out);
        }
    }
    copyRest(in, out);
}

void rewritePlayerChat(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyBytes(in, out, 16);
    copyVarInt(in, out);
    const bool signature = pv::readBoolean(in);
    pv::writeBoolean(out, signature);
    if (signature) {
        copyBytes(in, out, 256);
    }
    copyString(in, out);
    copyBytes(in, out, 16);
    const std::int32_t seen = pv::readCollectionSize(in);
    pv::varInt::write(out, seen);
    for (std::int32_t index = 0; index < seen && !in.failed(); ++index) {
        const std::int32_t entry = pv::varInt::read(in);
        pv::varInt::write(out, entry);
        if (entry == 0) {
            copyBytes(in, out, 256);
        }
    }
    convertOptionalComponent(in, out);
    const std::int32_t filter = pv::varInt::read(in);
    pv::varInt::write(out, filter);
    if (filter == 2) {
        const std::int32_t words = pv::readCollectionSize(in, 8);
        pv::varInt::write(out, words);
        copyBytes(in, out, static_cast<std::size_t>(words) * 8);
    }
    copyVarInt(in, out);
    convertComponent(in, out);
    convertOptionalComponent(in, out);
}

void rewriteDisguisedChat(io::ByteBuffer& in, io::ByteBuffer& out) {
    convertComponent(in, out);
    copyVarInt(in, out);
    convertComponent(in, out);
    convertOptionalComponent(in, out);
}

void rewritePlayerInfo(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::uint8_t actions = in.readByte();
    out.writeByte(actions);
    const std::int32_t entries = pv::readCollectionSize(in, 17);
    pv::varInt::write(out, entries);
    for (std::int32_t entry = 0; entry < entries && !in.failed(); ++entry) {
        copyBytes(in, out, 16);
        if ((actions & 0x01) != 0) {
            copyString(in, out);
            const std::int32_t properties = pv::readCollectionSize(in, 3);
            pv::varInt::write(out, properties);
            for (std::int32_t index = 0; index < properties && !in.failed(); ++index) {
                copyString(in, out);
                copyString(in, out);
                const bool signedProperty = pv::readBoolean(in);
                pv::writeBoolean(out, signedProperty);
                if (signedProperty) {
                    copyString(in, out);
                }
            }
        }
        if ((actions & 0x02) != 0) {
            const bool session = pv::readBoolean(in);
            pv::writeBoolean(out, session);
            if (session) {
                copyBytes(in, out, 16 + 8);
                for (int part = 0; part < 2; ++part) {
                    const std::int32_t length = pv::readCollectionSize(in);
                    pv::varInt::write(out, length);
                    copyBytes(in, out, static_cast<std::size_t>(length));
                }
            }
        }
        if ((actions & 0x04) != 0) {
            copyVarInt(in, out);
        }
        if ((actions & 0x08) != 0) {
            out.writeByte(in.readByte());
        }
        if ((actions & 0x10) != 0) {
            copyVarInt(in, out);
        }
        if ((actions & 0x20) != 0) {
            convertOptionalComponent(in, out);
        }
    }
}

template <typename Block>
void transform(MutablePacket& packet, Block&& block) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    std::forward<Block>(block)(in, out);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

} // namespace

bool Link764Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration) {
        if (packet.id == kConfigDisconnect) {
            transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) { convertComponent(in, out); });
        } else if (packet.id == kConfigResourcePack) {
            splitResourcePack(packet, kConfigPop, kConfigPush);
        }
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kBossEvent:
        transform(packet, rewriteBossEvent);
        break;
    case kCommandSuggestions:
        transform(packet, rewriteSuggestions);
        break;
    case kCommands: {
        commandParsers::Layout layout;
        layout.scoreHolder = 29;
        layout.time = 40;
        layout.firstRegistry = 41;
        layout.lastRegistry = 44;
        static_cast<void>(commandParsers::remap(packet, layout, parserIn765));
        break;
    }
    case kDisconnect:
    case kActionBar:
    case kSubtitle:
    case kTitle:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) { convertComponent(in, out); });
        break;
    case kSystemChat:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            convertComponent(in, out);
            copyRest(in, out);
        });
        break;
    case kDisguisedChat:
        transform(packet, rewriteDisguisedChat);
        break;
    case kInitializeBorder:
    case kLogin:
    case kRespawn:
        packet.forwarded.emplace_back(kGameEvent, loadingEvent());
        break;
    case kMapItemData:
        transform(packet, rewriteMap);
        break;
    case kOpenScreen:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            copyVarInt(in, out);
            convertComponent(in, out);
        });
        break;
    case kPlayerChat:
        transform(packet, rewritePlayerChat);
        break;
    case kCombatKill:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            convertComponent(in, out);
        });
        break;
    case kPlayerInfoUpdate:
        transform(packet, rewritePlayerInfo);
        break;
    case kResourcePack:
        splitResourcePack(packet, kPop765, kPush765);
        break;
    case kServerData:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            convertComponent(in, out);
            copyRest(in, out);
        });
        break;
    case kSetObjective:
        transform(packet, rewriteObjective);
        break;
    case kSetTeam:
        transform(packet, rewriteTeam);
        break;
    case kSetScore:
        rewriteScore(packet);
        break;
    case kTabList:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            convertComponent(in, out);
            convertComponent(in, out);
        });
        break;
    default:
        break;
    }
    return true;
}

bool Link764Up::serverbound(ProtocolState state, MutablePacket& packet) {
    const bool pack = state == ProtocolState::Configuration || state == ProtocolState::Play;
    if (pack && packet.id == (state == ProtocolState::Play ? kResourcePackStatus764 : 5)) {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        in.skip(16);
        const std::int32_t action = pv::varInt::read(in);
        if (in.failed() || action == 4) {
            return false;
        }
        io::ByteBuffer out;
        pv::varInt::write(out, action > 4 ? 2 : action);
        packet.body = out.toVector();
        return true;
    }
    if (state == ProtocolState::Play && packet.id == kSetJigsaw764) {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        copyBytes(in, out, 8);
        for (int index = 0; index < 5; ++index) {
            copyString(in, out);
        }
        if (!in.failed()) {
            packet.body = out.toVector();
        }
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
