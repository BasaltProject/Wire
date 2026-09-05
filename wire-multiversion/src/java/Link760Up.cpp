#include <basalt/wire/multiversion/java/CommandParsers.hpp>
#include <basalt/wire/multiversion/java/Link760Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kChatPreview = 12;
constexpr std::int32_t kCommands = 15;
constexpr std::int32_t kHideMessage = 24;
constexpr std::int32_t kNamedSound = 23;
constexpr std::int32_t kPlayerChat = 51;
constexpr std::int32_t kPlayerInfo = 55;
constexpr std::int32_t kServerData = 66;
constexpr std::int32_t kShouldDisplayChatPreview = 78;
constexpr std::int32_t kEntitySound = 95;
constexpr std::int32_t kSound = 96;

constexpr std::int32_t kProfilelessChat761 = 24;
constexpr std::int32_t kPlayerInfoUpdate761 = 54;
constexpr std::int32_t kPlayerRemove761 = 53;
constexpr std::int32_t kSound761 = 94;

constexpr std::int32_t kChatCommand = 4;
constexpr std::int32_t kChatMessage = 5;
constexpr std::int32_t kMessageAcknowledgement = 3;
constexpr std::int32_t kChatSessionUpdate = 32;

constexpr std::int32_t kFirstSayCommand = 3;
constexpr std::int32_t kParserMobEffect = 34;
constexpr std::int32_t kParserItemEnchantment = 39;
constexpr std::int32_t kParserEntitySummon = 40;

void copyVarInt(io::ByteBuffer& in, io::ByteBuffer& out) { pv::varInt::write(out, pv::varInt::read(in)); }

void copyString(io::ByteBuffer& in, io::ByteBuffer& out) { pv::writeString(out, pv::readString(in)); }

void copyBytes(io::ByteBuffer& in, io::ByteBuffer& out, std::size_t count) { out.writeBytes(in.readSpan(count)); }

void copyRest(io::ByteBuffer& in, io::ByteBuffer& out) { out.writeBytes(in.readable()); }

template <typename Block>
void transform(MutablePacket& packet, Block&& block) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    std::forward<Block>(block)(in, out);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

std::int32_t parserIn761(std::int32_t parser) {
    switch (parser) {
    case kParserMobEffect:
    case kParserItemEnchantment:
    case kParserEntitySummon:
        return 43;
    default:
        break;
    }
    if (parser >= 35 && parser <= 38) {
        return parser - 1;
    }
    if (parser == 41) {
        return 38;
    }
    if (parser == 42) {
        return 40;
    }
    if (parser == 43) {
        return 41;
    }
    if (parser == 44) {
        return 43;
    }
    return parser;
}

const char* registryOfRemovedParser(std::int32_t parser) {
    switch (parser) {
    case kParserMobEffect:
        return "minecraft:mob_effect";
    case kParserItemEnchantment:
        return "minecraft:enchantment";
    case kParserEntitySummon:
        return "minecraft:entity_type";
    default:
        return nullptr;
    }
}

void skipOptionalString(io::ByteBuffer& in) {
    if (pv::readBoolean(in)) {
        static_cast<void>(pv::readString(in));
    }
}

std::string textComponent(const std::string& plain) {
    std::string json = "{\"text\":\"";
    for (const char c : plain) {
        if (c == '"' || c == '\\') {
            json.push_back('\\');
        }
        if (static_cast<unsigned char>(c) < 0x20) {
            continue;
        }
        json.push_back(c);
    }
    return json + "\"}";
}

void rewritePlayerChat(io::ByteBuffer& in, io::ByteBuffer& out) {
    if (pv::readBoolean(in)) {
        in.skip(static_cast<std::size_t>(pv::readCollectionSize(in)));
    }
    in.skip(16);
    in.skip(static_cast<std::size_t>(pv::readCollectionSize(in)));
    const std::string plain = pv::readString(in);
    skipOptionalString(in);
    in.skip(16);
    const std::int32_t previous = pv::readCollectionSize(in, 17);
    for (std::int32_t index = 0; index < previous && !in.failed(); ++index) {
        in.skip(16);
        in.skip(static_cast<std::size_t>(pv::readCollectionSize(in)));
    }
    std::string content;
    const bool hasUnsigned = pv::readBoolean(in);
    if (hasUnsigned) {
        content = pv::readString(in);
    }
    const std::int32_t filter = pv::varInt::read(in);
    if (filter == 2) {
        in.skip(static_cast<std::size_t>(pv::readCollectionSize(in, 8)) * 8);
    }
    const std::int32_t type = pv::varInt::read(in);
    const std::string name = pv::readString(in);
    const bool hasTarget = pv::readBoolean(in);
    std::string target;
    if (hasTarget) {
        target = pv::readString(in);
    }
    if (in.failed()) {
        return;
    }
    pv::writeString(out, hasUnsigned ? content : textComponent(plain));
    pv::varInt::write(out, type >= kFirstSayCommand ? type - 2 : 0);
    pv::writeString(out, name);
    pv::writeBoolean(out, hasTarget);
    if (hasTarget) {
        pv::writeString(out, target);
    }
}

void rewritePlayerInfo(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    const std::int32_t action = pv::varInt::read(in);
    const std::int32_t entries = pv::readCollectionSize(in, 17);
    if (action == 4) {
        pv::varInt::write(out, entries);
        for (std::int32_t index = 0; index < entries && !in.failed(); ++index) {
            copyBytes(in, out, 16);
        }
        if (!in.failed()) {
            packet.body = out.toVector();
            packet.overrideId(kPlayerRemove761);
        }
        return;
    }
    constexpr std::uint8_t kBits[] = {0x01 | 0x04 | 0x08 | 0x10 | 0x20, 0x04, 0x10, 0x20};
    out.writeByte(kBits[action < 4 ? action : 0]);
    pv::varInt::write(out, entries);
    for (std::int32_t index = 0; index < entries && !in.failed(); ++index) {
        copyBytes(in, out, 16);
        if (action == 0) {
            copyString(in, out);
            const std::int32_t properties = pv::readCollectionSize(in, 3);
            pv::varInt::write(out, properties);
            for (std::int32_t property = 0; property < properties && !in.failed(); ++property) {
                copyString(in, out);
                copyString(in, out);
                const bool signedProperty = pv::readBoolean(in);
                pv::writeBoolean(out, signedProperty);
                if (signedProperty) {
                    copyString(in, out);
                }
            }
            copyVarInt(in, out);
            pv::writeBoolean(out, true);
            copyVarInt(in, out);
            const bool name = pv::readBoolean(in);
            pv::writeBoolean(out, name);
            if (name) {
                copyString(in, out);
            }
            if (pv::readBoolean(in)) {
                in.skip(8);
                in.skip(static_cast<std::size_t>(pv::readCollectionSize(in)));
                in.skip(static_cast<std::size_t>(pv::readCollectionSize(in)));
            }
        } else if (action == 1 || action == 2) {
            copyVarInt(in, out);
        } else if (action == 3) {
            const bool name = pv::readBoolean(in);
            pv::writeBoolean(out, name);
            if (name) {
                copyString(in, out);
            }
        }
    }
    if (!in.failed()) {
        packet.body = out.toVector();
        packet.overrideId(kPlayerInfoUpdate761);
    }
}

void rewriteSound(io::ByteBuffer& in, io::ByteBuffer& out) {
    pv::varInt::write(out, pv::varInt::read(in) + 1);
    copyRest(in, out);
}

void rewriteNamedSound(MutablePacket& packet) {
    transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
        const std::string name = pv::readString(in);
        pv::varInt::write(out, 0);
        pv::writeString(out, name);
        pv::writeBoolean(out, false);
        copyRest(in, out);
    });
    packet.overrideId(kSound761);
}

} // namespace

bool Link760Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kChatPreview:
    case kHideMessage:
    case kShouldDisplayChatPreview:
        return false;
    case kPlayerChat:
        transform(packet, rewritePlayerChat);
        packet.overrideId(kProfilelessChat761);
        break;
    case kPlayerInfo:
        rewritePlayerInfo(packet);
        break;
    case kServerData:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            for (int part = 0; part < 2; ++part) {
                const bool present = pv::readBoolean(in);
                pv::writeBoolean(out, present);
                if (present) {
                    copyString(in, out);
                }
            }
            static_cast<void>(in.readByte());
            copyRest(in, out);
        });
        break;
    case kNamedSound:
        rewriteNamedSound(packet);
        break;
    case kSound:
    case kEntitySound:
        transform(packet, rewriteSound);
        break;
    case kCommands: {
        commandParsers::Layout layout;
        layout.scoreHolder = 29;
        layout.time = -1;
        layout.firstRegistry = 43;
        layout.lastRegistry = 44;
        layout.addedRegistry = registryOfRemovedParser;
        static_cast<void>(commandParsers::remap(packet, layout, parserIn761));
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link760Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kChatSessionUpdate:
    case kMessageAcknowledgement:
        return false;
    case kChatMessage:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyString(in, out);
            copyBytes(in, out, 16);
            pv::varInt::write(out, 0);
            pv::writeBoolean(out, false);
            pv::varInt::write(out, 0);
            pv::writeBoolean(out, false);
            in.skip(in.readableBytes());
        });
        break;
    case kChatCommand:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyString(in, out);
            copyBytes(in, out, 16);
            pv::varInt::write(out, 0);
            pv::writeBoolean(out, false);
            pv::varInt::write(out, 0);
            pv::writeBoolean(out, false);
            in.skip(in.readableBytes());
        });
        break;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
