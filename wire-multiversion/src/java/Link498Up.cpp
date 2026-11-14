#include <basalt/wire/multiversion/java/Link498Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kSpawnMob = 0x03;
constexpr std::int32_t kSpawnPlayer = 0x05;
constexpr std::int32_t kChunk = 0x21;
constexpr std::int32_t kParticles = 0x23;
constexpr std::int32_t kLogin = 0x25;
constexpr std::int32_t kRespawn = 0x3A;
constexpr std::int32_t kEntityDataNew = 0x44;

constexpr std::int32_t kColumnSide = 16;
constexpr std::int32_t kColumns = kColumnSide * kColumnSide;
constexpr std::int32_t kBiomeSide = 4;
constexpr std::int32_t kBiomeLayers = 64;

} // namespace

bool Link498Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kLogin:
        out.writeBytes(in.readSpan(4 + 1 + 4));
        out.writeLong(0);
        out.writeByte(in.readByte());
        pv::writeString(out, pv::readString(in));
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeByte(in.readByte());
        pv::writeBoolean(out, true);
        break;
    case kRespawn:
        out.writeInt(in.readInt());
        out.writeLong(0);
        out.writeBytes(in.readable());
        in = io::ByteBuffer{};
        break;
    case kParticles:
        out.writeInt(in.readInt());
        out.writeByte(in.readByte());
        for (int axis = 0; axis < 3; ++axis) {
            out.writeDouble(static_cast<double>(in.readFloat()));
        }
        out.writeBytes(in.readable());
        in = io::ByteBuffer{};
        break;
    case kSpawnMob:
    case kSpawnPlayer: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        out.writeBytes(in.readSpan(16));
        if (packet.id == kSpawnMob) {
            pv::varInt::write(out, pv::varInt::read(in));
            out.writeBytes(in.readSpan(24 + 3 + 6));
        } else {
            out.writeBytes(in.readSpan(24 + 2));
        }
        if (in.failed()) {
            return false;
        }
        io::ByteBuffer data;
        pv::varInt::write(data, entity);
        data.writeBytes(in.readable());
        packet.forwarded.emplace_back(kEntityDataNew, data.toVector());
        in = io::ByteBuffer{};
        break;
    }
    case kChunk: {
        out.writeInt(in.readInt());
        out.writeInt(in.readInt());
        const std::uint8_t full = in.readByte();
        out.writeByte(full);
        pv::varInt::write(out, pv::varInt::read(in));
        const auto heightmaps = readNamedNbt(in);
        if (in.failed()) {
            return false;
        }
        out.writeBytes(heightmaps->raw);
        const std::int32_t length = pv::varInt::read(in);
        const std::size_t biomeBytes = full != 0 ? static_cast<std::size_t>(kColumns) * 4 : 0;
        if (length < 0 || static_cast<std::size_t>(length) < biomeBytes) {
            return false;
        }
        const std::span<const std::uint8_t> data = in.readSpan(static_cast<std::size_t>(length));
        const std::size_t sectionBytes = data.size() - biomeBytes;
        if (full != 0) {
            io::ByteBuffer columns = io::ByteBuffer::wrap(
                std::vector<std::uint8_t>{data.begin() + static_cast<std::ptrdiff_t>(sectionBytes), data.end()});
            std::vector<std::int32_t> ids(kColumns);
            for (std::int32_t column = 0; column < kColumns; ++column) {
                ids[static_cast<std::size_t>(column)] = columns.readInt();
            }
            for (std::int32_t layer = 0; layer < kBiomeLayers; ++layer) {
                for (std::int32_t z = 0; z < kBiomeSide; ++z) {
                    for (std::int32_t x = 0; x < kBiomeSide; ++x) {
                        out.writeInt(ids[static_cast<std::size_t>((z * kBiomeSide) * kColumnSide + x * kBiomeSide)]);
                    }
                }
            }
        }
        pv::varInt::write(out, static_cast<std::int32_t>(sectionBytes));
        out.writeBytes(data.first(sectionBytes));
        out.writeBytes(in.readable());
        in = io::ByteBuffer{};
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

} // namespace basalt::wire::multiversion::java
