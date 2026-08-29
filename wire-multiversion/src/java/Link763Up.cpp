#include <basalt/wire/multiversion/java/Link763Up.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kNamedEntitySpawn = 3;
constexpr std::int32_t kTileEntityData = 8;
constexpr std::int32_t kWindowItems = 18;
constexpr std::int32_t kSetSlot = 20;
constexpr std::int32_t kMapChunk = 36;
constexpr std::int32_t kWorldParticles = 38;
constexpr std::int32_t kLogin = 40;
constexpr std::int32_t kTradeList = 42;
constexpr std::int32_t kRespawn = 65;
constexpr std::int32_t kEntityMetadata = 82;
constexpr std::int32_t kEntityEquipment = 85;
constexpr std::int32_t kNbtQueryResponse = 102;
constexpr std::int32_t kEntityEffect = 108;

constexpr std::int32_t kWindowClick = 11;
constexpr std::int32_t kSetCreativeSlot = 43;

constexpr std::int32_t kSpawnEntity764 = 1;
constexpr std::int32_t kMoreSlots = 0x80;
constexpr std::int32_t kEndOfMetadata = 0xFF;

constexpr std::int32_t kBlockParticle = 2;
constexpr std::int32_t kBlockMarkerParticle = 3;
constexpr std::int32_t kDustParticle = 14;
constexpr std::int32_t kDustTransitionParticle = 15;
constexpr std::int32_t kFallingDustParticle = 25;
constexpr std::int32_t kSculkChargeParticle = 31;
constexpr std::int32_t kItemParticle = 40;
constexpr std::int32_t kVibrationParticle = 41;
constexpr std::int32_t kShriekParticle = 93;

void copyVarInt(io::ByteBuffer& in, io::ByteBuffer& out) { pv::varInt::write(out, pv::varInt::read(in)); }

void copyString(io::ByteBuffer& in, io::ByteBuffer& out) { pv::writeString(out, pv::readString(in)); }

void copyBytes(io::ByteBuffer& in, io::ByteBuffer& out, std::size_t count) { out.writeBytes(in.readSpan(count)); }

void copyVarLong(io::ByteBuffer& in, io::ByteBuffer& out) {
    for (int index = 0; index < 10 && !in.failed(); ++index) {
        const std::uint8_t byte = in.readByte();
        out.writeByte(byte);
        if ((byte & 0x80) == 0) {
            return;
        }
    }
    in.markFailed();
}

void copyRest(io::ByteBuffer& in, io::ByteBuffer& out) { out.writeBytes(in.readable()); }

/// A network NBT value whose root has a name (1.20.1) or none (1.20.2): the name is always empty on the wire.
void convertNbt(io::ByteBuffer& in, io::ByteBuffer& out, bool toAnonymous) {
    const std::uint8_t type = in.readByte();
    if (in.failed()) {
        return;
    }
    out.writeByte(type);
    if (type == 0) {
        return;
    }
    if (toAnonymous) {
        const std::int32_t length = static_cast<std::uint16_t>(in.readShort());
        in.skip(static_cast<std::size_t>(length));
    } else {
        out.writeShort(0);
    }
    if (in.failed()) {
        return;
    }
    std::vector<std::uint8_t> window;
    window.reserve(in.readableBytes() + 1);
    window.push_back(type);
    const auto rest = in.readable();
    window.insert(window.end(), rest.begin(), rest.end());
    const auto parsed = nbt::nbtIO::readAnonymous(window);
    if (!parsed) {
        in.markFailed();
        return;
    }
    out.writeBytes(std::span<const std::uint8_t>{window}.subspan(1, parsed->bytesConsumed - 1));
    in.skip(parsed->bytesConsumed - 1);
}

void convertOptionalNbt(io::ByteBuffer& in, io::ByteBuffer& out, bool toAnonymous) { convertNbt(in, out, toAnonymous); }

void convertStack(io::ByteBuffer& in, io::ByteBuffer& out, bool toAnonymous) {
    const bool present = pv::readBoolean(in);
    pv::writeBoolean(out, present);
    if (!present) {
        return;
    }
    copyVarInt(in, out);
    out.writeByte(in.readByte());
    convertNbt(in, out, toAnonymous);
}

void convertParticleData(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t particle, bool toAnonymous) {
    switch (particle) {
    case kBlockParticle:
    case kBlockMarkerParticle:
    case kFallingDustParticle:
    case kShriekParticle:
        copyVarInt(in, out);
        break;
    case kDustParticle:
        copyBytes(in, out, 16);
        break;
    case kDustTransitionParticle:
        copyBytes(in, out, 28);
        break;
    case kSculkChargeParticle:
        copyBytes(in, out, 4);
        break;
    case kItemParticle:
        convertStack(in, out, toAnonymous);
        break;
    case kVibrationParticle: {
        const std::string kind = pv::readString(in);
        pv::writeString(out, kind);
        if (kind == "minecraft:block") {
            copyBytes(in, out, 8);
        } else {
            copyVarInt(in, out);
            copyVarInt(in, out);
        }
        copyVarInt(in, out);
        break;
    }
    default:
        break;
    }
}

void convertEntityData(io::ByteBuffer& in, io::ByteBuffer& out) {
    while (!in.failed()) {
        const std::uint8_t index = in.readByte();
        out.writeByte(index);
        if (index == kEndOfMetadata) {
            return;
        }
        const std::int32_t type = pv::varInt::read(in);
        pv::varInt::write(out, type);
        switch (type) {
        case 0:
        case 8:
            copyBytes(in, out, 1);
            break;
        case 1:
        case 12:
        case 14:
        case 15:
        case 19:
        case 20:
        case 21:
        case 22:
        case 24:
        case 25:
            copyVarInt(in, out);
            break;
        case 2:
            copyVarLong(in, out);
            break;
        case 3:
            copyBytes(in, out, 4);
            break;
        case 4:
        case 5:
            copyString(in, out);
            break;
        case 6:
            if (pv::readBoolean(in)) {
                pv::writeBoolean(out, true);
                copyString(in, out);
            } else {
                pv::writeBoolean(out, false);
            }
            break;
        case 7:
            convertStack(in, out, true);
            break;
        case 9:
        case 26:
            copyBytes(in, out, 12);
            break;
        case 10:
            copyBytes(in, out, 8);
            break;
        case 11:
            if (pv::readBoolean(in)) {
                pv::writeBoolean(out, true);
                copyBytes(in, out, 8);
            } else {
                pv::writeBoolean(out, false);
            }
            break;
        case 13:
            if (pv::readBoolean(in)) {
                pv::writeBoolean(out, true);
                copyBytes(in, out, 16);
            } else {
                pv::writeBoolean(out, false);
            }
            break;
        case 16:
            convertNbt(in, out, true);
            break;
        case 17: {
            const std::int32_t particle = pv::varInt::read(in);
            pv::varInt::write(out, particle);
            convertParticleData(in, out, particle, true);
            break;
        }
        case 18:
            copyVarInt(in, out);
            copyVarInt(in, out);
            copyVarInt(in, out);
            break;
        case 23:
            if (pv::readBoolean(in)) {
                pv::writeBoolean(out, true);
                copyString(in, out);
                copyBytes(in, out, 8);
            } else {
                pv::writeBoolean(out, false);
            }
            break;
        case 27:
            copyBytes(in, out, 16);
            break;
        default:
            in.markFailed();
            break;
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

void rewriteLogin(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::int32_t entityId = in.readInt();
    const bool hardcore = pv::readBoolean(in);
    const std::uint8_t gamemode = in.readByte();
    const std::uint8_t previous = in.readByte();
    const std::int32_t worlds = pv::readCollectionSize(in, 1);
    std::vector<std::string> names;
    for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
        names.push_back(pv::readString(in));
    }
    {
        io::ByteBuffer codec;
        convertNbt(in, codec, true);
    }
    const std::string worldType = pv::readString(in);
    const std::string worldName = pv::readString(in);
    const std::int64_t seed = in.readLong();
    const std::int32_t maxPlayers = pv::varInt::read(in);
    const std::int32_t view = pv::varInt::read(in);
    const std::int32_t simulation = pv::varInt::read(in);
    const bool reduced = pv::readBoolean(in);
    const bool respawnScreen = pv::readBoolean(in);
    const bool debug = pv::readBoolean(in);
    const bool flat = pv::readBoolean(in);
    const bool death = pv::readBoolean(in);
    std::string deathDimension;
    std::int64_t deathPosition = 0;
    if (death) {
        deathDimension = pv::readString(in);
        deathPosition = in.readLong();
    }
    const std::int32_t cooldown = pv::varInt::read(in);
    if (in.failed()) {
        return;
    }
    out.writeInt(entityId);
    pv::writeBoolean(out, hardcore);
    pv::varInt::write(out, static_cast<std::int32_t>(names.size()));
    for (const std::string& name : names) {
        pv::writeString(out, name);
    }
    pv::varInt::write(out, maxPlayers);
    pv::varInt::write(out, view);
    pv::varInt::write(out, simulation);
    pv::writeBoolean(out, reduced);
    pv::writeBoolean(out, respawnScreen);
    pv::writeBoolean(out, false);
    pv::writeString(out, worldType);
    pv::writeString(out, worldName);
    out.writeLong(seed);
    out.writeByte(gamemode);
    out.writeByte(previous);
    pv::writeBoolean(out, debug);
    pv::writeBoolean(out, flat);
    pv::writeBoolean(out, death);
    if (death) {
        pv::writeString(out, deathDimension);
        out.writeLong(deathPosition);
    }
    pv::varInt::write(out, cooldown);
}

void rewriteRespawn(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyString(in, out);
    copyString(in, out);
    copyBytes(in, out, 8 + 2 + 2);
    const std::uint8_t keep = in.readByte();
    const bool death = pv::readBoolean(in);
    pv::writeBoolean(out, death);
    if (death) {
        copyString(in, out);
        copyBytes(in, out, 8);
    }
    copyVarInt(in, out);
    out.writeByte(keep);
}

void rewritePlayerSpawn(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t playerType) {
    copyVarInt(in, out);
    copyBytes(in, out, 16);
    pv::varInt::write(out, playerType);
    copyBytes(in, out, 24);
    const std::uint8_t yaw = in.readByte();
    const std::uint8_t pitch = in.readByte();
    out.writeByte(pitch);
    out.writeByte(yaw);
    out.writeByte(yaw);
    pv::varInt::write(out, 0);
    out.writeShort(0);
    out.writeShort(0);
    out.writeShort(0);
}

void rewriteChunk(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyBytes(in, out, 8);
    convertNbt(in, out, true);
    const std::int32_t size = pv::varInt::read(in);
    pv::varInt::write(out, size);
    if (size < 0) {
        in.markFailed();
        return;
    }
    copyBytes(in, out, static_cast<std::size_t>(size));
    const std::int32_t entities = pv::readCollectionSize(in, 4);
    pv::varInt::write(out, entities);
    for (std::int32_t index = 0; index < entities && !in.failed(); ++index) {
        copyBytes(in, out, 3);
        copyVarInt(in, out);
        convertOptionalNbt(in, out, true);
    }
    copyRest(in, out);
}

} // namespace

bool Link763Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kNamedEntitySpawn:
        transform(packet, [&](io::ByteBuffer& in, io::ByteBuffer& out) {
            rewritePlayerSpawn(in, out, tables().playerEntityType);
        });
        packet.overrideId(kSpawnEntity764);
        break;
    case kTileEntityData:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyBytes(in, out, 8);
            copyVarInt(in, out);
            convertOptionalNbt(in, out, true);
        });
        break;
    case kWindowItems:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            out.writeByte(in.readByte());
            copyVarInt(in, out);
            const std::int32_t count = pv::readCollectionSize(in, 1);
            pv::varInt::write(out, count);
            for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
                convertStack(in, out, true);
            }
            convertStack(in, out, true);
        });
        break;
    case kSetSlot:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            out.writeByte(in.readByte());
            copyVarInt(in, out);
            copyBytes(in, out, 2);
            convertStack(in, out, true);
        });
        break;
    case kMapChunk:
        transform(packet, rewriteChunk);
        break;
    case kWorldParticles:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            const std::int32_t particle = pv::varInt::read(in);
            pv::varInt::write(out, particle);
            copyBytes(in, out, 1 + 24 + 12 + 4 + 4);
            convertParticleData(in, out, particle, true);
        });
        break;
    case kLogin:
        transform(packet, rewriteLogin);
        break;
    case kTradeList:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            const std::int32_t trades = pv::readCollectionSize(in, 20);
            pv::varInt::write(out, trades);
            for (std::int32_t index = 0; index < trades && !in.failed(); ++index) {
                convertStack(in, out, true);
                convertStack(in, out, true);
                convertStack(in, out, true);
                copyBytes(in, out, 1 + 4 * 4 + 4 + 4);
            }
            copyRest(in, out);
        });
        break;
    case kRespawn:
        transform(packet, rewriteRespawn);
        break;
    case kEntityMetadata:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            convertEntityData(in, out);
            copyRest(in, out);
        });
        break;
    case kEntityEquipment:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            std::uint8_t slot = 0;
            do {
                slot = in.readByte();
                out.writeByte(slot);
                convertStack(in, out, true);
            } while ((slot & kMoreSlots) != 0 && !in.failed());
        });
        break;
    case kNbtQueryResponse:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            convertOptionalNbt(in, out, true);
        });
        break;
    case kEntityEffect:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            copyVarInt(in, out);
            copyBytes(in, out, 1);
            copyVarInt(in, out);
            copyBytes(in, out, 1);
            const bool factor = pv::readBoolean(in);
            pv::writeBoolean(out, factor);
            if (factor) {
                convertNbt(in, out, true);
            }
        });
        break;
    default:
        break;
    }
    return true;
}

bool Link763Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    if (packet.id == kWindowClick) {
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            out.writeByte(in.readByte());
            copyVarInt(in, out);
            copyBytes(in, out, 2 + 1);
            copyVarInt(in, out);
            const std::int32_t count = pv::readCollectionSize(in, 3);
            pv::varInt::write(out, count);
            for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
                copyBytes(in, out, 2);
                convertStack(in, out, false);
            }
            convertStack(in, out, false);
        });
    } else if (packet.id == kSetCreativeSlot) {
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyBytes(in, out, 2);
            convertStack(in, out, false);
        });
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
