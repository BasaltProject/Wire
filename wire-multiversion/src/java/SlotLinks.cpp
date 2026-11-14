#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/multiversion/java/SlotLinks.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kContainerContent = 0x15;
constexpr std::int32_t kContainerSlot = 0x17;
constexpr std::int32_t kCustomPayload = 0x19;
constexpr std::int32_t kParticles = 0x24;
constexpr std::int32_t kEntityData = 0x3F;
constexpr std::int32_t kEquipment = 0x42;
constexpr std::int32_t kAdvancements = 0x51;
constexpr std::int32_t kContainerClick = 0x08;
constexpr std::int32_t kEditBook = 0x0B;
constexpr std::int32_t kCreativeSlot = 0x24;

constexpr std::int32_t kBlockParticle = 3;
constexpr std::int32_t kDustParticle = 11;
constexpr std::int32_t kFallingDustParticle = 20;
constexpr std::int32_t kItemParticle = 27;
constexpr std::uint8_t kEndOfData = 0xFF;
constexpr std::int32_t kHasBackground = 1;

void copyNbt(io::ByteBuffer& in, io::ByteBuffer& out) {
    const auto tag = readNamedNbt(in);
    if (tag) {
        out.writeBytes(tag->raw);
    }
}

void copyString(io::ByteBuffer& in, io::ByteBuffer& out) { pv::writeString(out, pv::readString(in)); }

void climbStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::int16_t id = in.readShort();
    if (id < 0) {
        pv::writeBoolean(out, false);
        return;
    }
    pv::writeBoolean(out, true);
    pv::varInt::write(out, id);
    out.writeByte(in.readByte());
    copyNbt(in, out);
}

void descendStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    if (in.readByte() == 0) {
        out.writeShort(-1);
        return;
    }
    out.writeShort(static_cast<std::int16_t>(pv::varInt::read(in)));
    out.writeByte(in.readByte());
    copyNbt(in, out);
}

void copyParticleData(std::int32_t particle, io::ByteBuffer& in, io::ByteBuffer& out) {
    switch (particle) {
    case kBlockParticle:
    case kFallingDustParticle:
        pv::varInt::write(out, pv::varInt::read(in));
        break;
    case kDustParticle:
        out.writeBytes(in.readSpan(4 * 4));
        break;
    case kItemParticle:
        climbStack(in, out);
        break;
    default:
        break;
    }
}

[[nodiscard]] bool entityData(io::ByteBuffer& in, io::ByteBuffer& out) {
    pv::varInt::write(out, pv::varInt::read(in));
    while (!in.failed()) {
        const std::uint8_t index = in.readByte();
        out.writeByte(index);
        if (index == kEndOfData) {
            return true;
        }
        const std::int32_t type = pv::varInt::read(in);
        pv::varInt::write(out, type);
        switch (type) {
        case 0:
        case 7:
            out.writeByte(in.readByte());
            break;
        case 1:
        case 11:
        case 13:
            pv::varInt::write(out, pv::varInt::read(in));
            break;
        case 2:
            out.writeBytes(in.readSpan(4));
            break;
        case 3:
        case 4:
            copyString(in, out);
            break;
        case 5: {
            const std::uint8_t present = in.readByte();
            out.writeByte(present);
            if (present != 0) {
                copyString(in, out);
            }
            break;
        }
        case 6:
            climbStack(in, out);
            break;
        case 8:
            out.writeBytes(in.readSpan(12));
            break;
        case 9:
            out.writeBytes(in.readSpan(8));
            break;
        case 10:
        case 12: {
            const std::uint8_t present = in.readByte();
            out.writeByte(present);
            if (present != 0) {
                out.writeBytes(in.readSpan(type == 10 ? 8 : 16));
            }
            break;
        }
        case 14:
            copyNbt(in, out);
            break;
        case 15: {
            const std::int32_t particle = pv::varInt::read(in);
            pv::varInt::write(out, particle);
            copyParticleData(particle, in, out);
            break;
        }
        default:
            return false;
        }
    }
    return false;
}

void advancements(io::ByteBuffer& in, io::ByteBuffer& out) {
    out.writeByte(in.readByte());
    const std::int32_t count = pv::varInt::read(in);
    pv::varInt::write(out, count);
    for (std::int32_t i = 0; i < count && !in.failed(); ++i) {
        copyString(in, out);
        const std::uint8_t parent = in.readByte();
        out.writeByte(parent);
        if (parent != 0) {
            copyString(in, out);
        }
        const std::uint8_t display = in.readByte();
        out.writeByte(display);
        if (display != 0) {
            copyString(in, out);
            copyString(in, out);
            climbStack(in, out);
            pv::varInt::write(out, pv::varInt::read(in));
            const std::int32_t flags = in.readInt();
            out.writeInt(flags);
            if ((flags & kHasBackground) != 0) {
                copyString(in, out);
            }
            out.writeBytes(in.readSpan(8));
        }
        const std::int32_t criteria = pv::varInt::read(in);
        pv::varInt::write(out, criteria);
        for (std::int32_t c = 0; c < criteria && !in.failed(); ++c) {
            copyString(in, out);
        }
        const std::int32_t requirements = pv::varInt::read(in);
        pv::varInt::write(out, requirements);
        for (std::int32_t r = 0; r < requirements && !in.failed(); ++r) {
            const std::int32_t names = pv::varInt::read(in);
            pv::varInt::write(out, names);
            for (std::int32_t n = 0; n < names && !in.failed(); ++n) {
                copyString(in, out);
            }
        }
    }
    out.writeBytes(in.readable());
}

[[nodiscard]] bool tradeList(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::string channel = pv::readString(in);
    pv::writeString(out, channel);
    if (channel != "minecraft:trader_list") {
        return false;
    }
    out.writeInt(in.readInt());
    const std::uint8_t trades = in.readByte();
    out.writeByte(trades);
    for (std::uint8_t trade = 0; trade < trades && !in.failed(); ++trade) {
        climbStack(in, out);
        climbStack(in, out);
        const std::uint8_t second = in.readByte();
        out.writeByte(second);
        if (second != 0) {
            climbStack(in, out);
        }
        out.writeBytes(in.readSpan(1 + 4 + 4));
    }
    return true;
}

} // namespace

bool Link401Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kContainerContent: {
        out.writeByte(in.readByte());
        const std::int16_t count = in.readShort();
        out.writeShort(count);
        for (std::int16_t slot = 0; slot < count && !in.failed(); ++slot) {
            climbStack(in, out);
        }
        break;
    }
    case kContainerSlot:
        out.writeBytes(in.readSpan(1 + 2));
        climbStack(in, out);
        break;
    case kEquipment:
        pv::varInt::write(out, pv::varInt::read(in));
        pv::varInt::write(out, pv::varInt::read(in));
        climbStack(in, out);
        break;
    case kEntityData:
        if (!entityData(in, out)) {
            return false;
        }
        break;
    case kParticles: {
        const std::int32_t particle = in.readInt();
        out.writeInt(particle);
        out.writeBytes(in.readSpan(1 + 4 * 6 + 4 + 4));
        copyParticleData(particle, in, out);
        break;
    }
    case kAdvancements:
        advancements(in, out);
        break;
    case kCustomPayload:
        if (!tradeList(in, out)) {
            return true;
        }
        break;
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link401Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kContainerClick:
        out.writeBytes(in.readSpan(1 + 2 + 1 + 2));
        pv::varInt::write(out, pv::varInt::read(in));
        descendStack(in, out);
        break;
    case kCreativeSlot:
        out.writeShort(in.readShort());
        descendStack(in, out);
        break;
    case kEditBook:
        descendStack(in, out);
        out.writeBytes(in.readable());
        break;
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link393Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.id != kEditBook) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    const std::int16_t id = in.readShort();
    out.writeShort(id);
    if (id >= 0) {
        out.writeByte(in.readByte());
        copyNbt(in, out);
    }
    out.writeByte(in.readByte());
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
