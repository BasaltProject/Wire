#include <basalt/wire/multiversion/java/Link758Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kAddEntity = 0;
constexpr std::int32_t kAddMob = 2;
constexpr std::int32_t kAddPainting = 3;
constexpr std::int32_t kVibrationSignal = 5;
constexpr std::int32_t kAckDigging = 8;
constexpr std::int32_t kChat = 15;
constexpr std::int32_t kCommands = 18;
constexpr std::int32_t kNamedSound = 25;
constexpr std::int32_t kLogin = 38;
constexpr std::int32_t kPlayerInfo = 54;
constexpr std::int32_t kRespawn = 61;
constexpr std::int32_t kEntitySound = 92;
constexpr std::int32_t kSound = 93;
constexpr std::int32_t kUpdateMobEffect = 101;

constexpr std::int32_t kAddEntity759 = 0;
constexpr std::int32_t kSystemChat759 = 95;
constexpr std::int32_t kPaintingType = 60;

constexpr std::int32_t kChatMessage759 = 4;
constexpr std::int32_t kChatCommand759 = 3;
constexpr std::int32_t kChatMessage758 = 3;

constexpr std::int32_t kPlayerAction = 26;
constexpr std::int32_t kUseItemOn = 46;
constexpr std::int32_t kUseItem = 47;

constexpr std::int32_t kAckPacket = 8;

constexpr std::int32_t kMaxText = 262144;

void copyVarInt(io::ByteBuffer& in, io::ByteBuffer& out) { pv::varInt::write(out, pv::varInt::read(in)); }

void copyString(io::ByteBuffer& in, io::ByteBuffer& out) { pv::writeString(out, pv::readString(in, kMaxText)); }

void copyBytes(io::ByteBuffer& in, io::ByteBuffer& out, std::size_t count) { out.writeBytes(in.readSpan(count)); }

void copyRest(io::ByteBuffer& in, io::ByteBuffer& out) { out.writeBytes(in.readable()); }

struct Parser {
    std::string_view name;
    std::int32_t id;
};

constexpr std::array<Parser, 48> kParsers = {{
    {"brigadier:bool", 0},
    {"brigadier:float", 1},
    {"brigadier:double", 2},
    {"brigadier:integer", 3},
    {"brigadier:long", 4},
    {"brigadier:string", 5},
    {"minecraft:entity", 6},
    {"minecraft:game_profile", 7},
    {"minecraft:block_pos", 8},
    {"minecraft:column_pos", 9},
    {"minecraft:vec3", 10},
    {"minecraft:vec2", 11},
    {"minecraft:block_state", 12},
    {"minecraft:block_predicate", 13},
    {"minecraft:item_stack", 14},
    {"minecraft:item_predicate", 15},
    {"minecraft:color", 16},
    {"minecraft:component", 17},
    {"minecraft:message", 18},
    {"minecraft:nbt_compound_tag", 19},
    {"minecraft:nbt_tag", 20},
    {"minecraft:nbt_path", 21},
    {"minecraft:objective", 22},
    {"minecraft:objective_criteria", 23},
    {"minecraft:operation", 24},
    {"minecraft:particle", 25},
    {"minecraft:angle", 26},
    {"minecraft:rotation", 27},
    {"minecraft:scoreboard_slot", 28},
    {"minecraft:score_holder", 29},
    {"minecraft:swizzle", 30},
    {"minecraft:team", 31},
    {"minecraft:item_slot", 32},
    {"minecraft:resource_location", 33},
    {"minecraft:mob_effect", 34},
    {"minecraft:function", 35},
    {"minecraft:entity_anchor", 36},
    {"minecraft:int_range", 37},
    {"minecraft:float_range", 38},
    {"minecraft:item_enchantment", 39},
    {"minecraft:entity_summon", 40},
    {"minecraft:dimension", 41},
    {"minecraft:time", 42},
    {"minecraft:resource_or_tag", 43},
    {"minecraft:resource", 44},
    {"minecraft:template_mirror", 45},
    {"minecraft:template_rotation", 46},
    {"minecraft:uuid", 47},
}};

std::int32_t parserId(std::string_view name) {
    for (const Parser& parser : kParsers) {
        if (parser.name == name) {
            return parser.id;
        }
    }
    return -1;
}

void copyParserProperties(std::string_view name, io::ByteBuffer& in, io::ByteBuffer& out) {
    if (name == "brigadier:float" || name == "brigadier:double" || name == "brigadier:integer" ||
        name == "brigadier:long") {
        const std::uint8_t flags = in.readByte();
        out.writeByte(flags);
        const std::size_t width = name == "brigadier:double" || name == "brigadier:long" ? 8 : 4;
        copyBytes(in, out, width * (static_cast<std::size_t>(flags & 1) + static_cast<std::size_t>((flags >> 1) & 1)));
    } else if (name == "brigadier:string") {
        copyVarInt(in, out);
    } else if (name == "minecraft:entity" || name == "minecraft:score_holder") {
        copyBytes(in, out, 1);
    } else if (name == "minecraft:resource_or_tag" || name == "minecraft:resource") {
        const std::string registry = pv::readString(in, kMaxText);
        pv::writeString(out, registry == "minecraft:worldgen/configured_structure_feature"
                                 ? std::string{"minecraft:worldgen/structure"}
                                 : registry);
    }
}

bool rewriteCommands(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::int32_t count = pv::readCollectionSize(in, 1);
    pv::varInt::write(out, count);
    for (std::int32_t node = 0; node < count && !in.failed(); ++node) {
        const std::uint8_t flags = in.readByte();
        out.writeByte(flags);
        const std::int32_t children = pv::readCollectionSize(in, 1);
        pv::varInt::write(out, children);
        for (std::int32_t child = 0; child < children && !in.failed(); ++child) {
            copyVarInt(in, out);
        }
        if ((flags & 0x08) != 0) {
            copyVarInt(in, out);
        }
        const std::uint8_t type = flags & 0x03;
        if (type == 1 || type == 2) {
            copyString(in, out);
        }
        if (type != 2) {
            continue;
        }
        const std::string parser = pv::readString(in, kMaxText);
        const std::int32_t id = parserId(parser);
        if (id < 0) {
            return false;
        }
        pv::varInt::write(out, id);
        copyParserProperties(parser, in, out);
        if ((flags & 0x10) != 0) {
            copyString(in, out);
        }
    }
    copyVarInt(in, out);
    return !in.failed();
}

std::int32_t paintingDirection(std::int32_t direction) {
    switch (direction) {
    case 0:
        return 3;
    case 1:
        return 4;
    case 2:
        return 2;
    case 3:
        return 5;
    default:
        return 2;
    }
}

} // namespace

std::string Link758Up::dimensionKey(const nbt::NbtTag& dimension) const {
    const nbt::NbtCompound* wanted = dimension.get<nbt::NbtCompound>();
    std::string best;
    std::size_t bestShared = 0;
    for (const auto& [name, element] : dimensions_) {
        const nbt::NbtCompound* candidate = element.get<nbt::NbtCompound>();
        if (wanted == nullptr || candidate == nullptr) {
            continue;
        }
        std::size_t shared = 0;
        for (std::size_t index = 0; index < wanted->size(); ++index) {
            const nbt::NbtTag* other = candidate->find(wanted->names()[index]);
            shared += other != nullptr && *other == wanted->values()[index] ? 1 : 0;
        }
        if (best.empty() || shared > bestShared) {
            best = name;
            bestShared = shared;
        }
    }
    return best.empty() ? std::string{"minecraft:overworld"} : best;
}

bool Link758Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kVibrationSignal:
    case kAckDigging:
        return false;
    case kAddEntity:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            copyBytes(in, out, 16);
            copyVarInt(in, out);
            copyBytes(in, out, 24);
            const std::uint8_t pitch = in.readByte();
            const std::uint8_t yaw = in.readByte();
            out.writeByte(pitch);
            out.writeByte(yaw);
            out.writeByte(yaw);
            pv::varInt::write(out, in.readInt());
            copyRest(in, out);
        });
        break;
    case kAddMob:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            copyBytes(in, out, 16);
            copyVarInt(in, out);
            copyBytes(in, out, 24);
            const std::uint8_t yaw = in.readByte();
            const std::uint8_t pitch = in.readByte();
            out.writeByte(pitch);
            out.writeByte(yaw);
            copyBytes(in, out, 1);
            pv::varInt::write(out, 0);
            copyRest(in, out);
        });
        packet.overrideId(kAddEntity759);
        break;
    case kAddPainting:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            copyBytes(in, out, 16);
            static_cast<void>(pv::varInt::read(in));
            const std::int64_t position = in.readLong();
            const std::int32_t direction = in.readByte();
            const auto x = static_cast<double>(position >> 38);
            const auto z = static_cast<double>((position << 26) >> 38);
            const auto y = static_cast<double>((position << 52) >> 52);
            pv::varInt::write(out, kPaintingType);
            out.writeDouble(x + 0.5);
            out.writeDouble(y + 0.5);
            out.writeDouble(z + 0.5);
            out.writeByte(0);
            out.writeByte(0);
            out.writeByte(0);
            pv::varInt::write(out, paintingDirection(direction));
            out.writeShort(0);
            out.writeShort(0);
            out.writeShort(0);
        });
        packet.overrideId(kAddEntity759);
        break;
    case kChat:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyString(in, out);
            const std::int32_t position = in.readByte();
            pv::varInt::write(out, position == 0 ? 1 : position);
        });
        packet.overrideId(kSystemChat759);
        break;
    case kCommands:
        if (!decode(packet, [&](io::ByteBuffer& in) {
                io::ByteBuffer out;
                if (!rewriteCommands(in, out)) {
                    return false;
                }
                packet.body = out.toVector();
                return true;
            })) {
            return false;
        }
        break;
    case kNamedSound:
    case kSound:
    case kEntitySound:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyRest(in, out);
            out.writeLong(0);
        });
        break;
    case kUpdateMobEffect:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyRest(in, out);
            pv::writeBoolean(out, false);
        });
        break;
    case kPlayerInfo:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            const std::int32_t action = pv::varInt::read(in);
            const std::int32_t entries = pv::readCollectionSize(in, 17);
            pv::varInt::write(out, action);
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
                    copyVarInt(in, out);
                    const bool display = pv::readBoolean(in);
                    pv::writeBoolean(out, display);
                    if (display) {
                        copyString(in, out);
                    }
                    pv::writeBoolean(out, false);
                } else if (action == 1 || action == 2) {
                    copyVarInt(in, out);
                } else if (action == 3) {
                    const bool display = pv::readBoolean(in);
                    pv::writeBoolean(out, display);
                    if (display) {
                        copyString(in, out);
                    }
                }
            }
        });
        break;
    case kLogin: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        copyBytes(in, out, 7);
        const std::int32_t worlds = pv::readCollectionSize(in, 1);
        pv::varInt::write(out, worlds);
        for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
            copyString(in, out);
        }
        const nbt::NbtTag codec = pv::readAnonymousNbt(in);
        pv::writeAnonymousNbt(out, codec);
        const nbt::NbtTag dimension = pv::readAnonymousNbt(in);
        if (in.failed()) {
            break;
        }
        dimensions_.clear();
        const nbt::NbtCompound* registries = codec.get<nbt::NbtCompound>();
        const nbt::NbtCompound* types = registries ? registries->getCompound("minecraft:dimension_type") : nullptr;
        const nbt::NbtList* values = types ? types->getList("value") : nullptr;
        if (values != nullptr) {
            for (const nbt::NbtTag& entry : *values) {
                const nbt::NbtCompound* compound = entry.get<nbt::NbtCompound>();
                const nbt::NbtTag* element = compound ? compound->find("element") : nullptr;
                if (element != nullptr) {
                    dimensions_.emplace_back(std::string{compound->getString("name")}, *element);
                }
            }
        }
        pv::writeString(out, dimensionKey(dimension));
        copyRest(in, out);
        pv::writeBoolean(out, false);
        packet.body = out.toVector();
        break;
    }
    case kRespawn: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        const nbt::NbtTag dimension = pv::readAnonymousNbt(in);
        if (in.failed()) {
            break;
        }
        pv::writeString(out, dimensionKey(dimension));
        copyRest(in, out);
        pv::writeBoolean(out, false);
        packet.body = out.toVector();
        break;
    }
    default:
        break;
    }
    if (!pendingAcks_.empty()) {
        for (const std::int32_t sequence : pendingAcks_) {
            io::ByteBuffer body;
            pv::varInt::write(body, sequence);
            packet.extras.emplace_back(kAckPacket, body.toVector());
        }
        pendingAcks_.clear();
    }
    return true;
}

bool Link758Up::serverboundForeign(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return false;
    }
    if (packet.id != kChatMessage759 && packet.id != kChatCommand759) {
        return false;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    std::string text = pv::readString(in, kMaxText);
    if (in.failed()) {
        return false;
    }
    if (packet.id == kChatCommand759) {
        text.insert(text.begin(), '/');
    }
    packet.id = kChatMessage758;
    io::ByteBuffer out;
    pv::writeString(out, text);
    packet.body = out.toVector();
    return true;
}

bool Link758Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    if (packet.id != kPlayerAction && packet.id != kUseItemOn && packet.id != kUseItem) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    if (packet.id == kPlayerAction) {
        copyVarInt(in, out);
        copyBytes(in, out, 9);
    } else if (packet.id == kUseItemOn) {
        copyVarInt(in, out);
        copyBytes(in, out, 8);
        copyVarInt(in, out);
        copyBytes(in, out, 13);
    } else {
        copyVarInt(in, out);
    }
    const std::int32_t sequence = pv::varInt::read(in);
    if (in.failed()) {
        return false;
    }
    pendingAcks_.push_back(sequence);
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
