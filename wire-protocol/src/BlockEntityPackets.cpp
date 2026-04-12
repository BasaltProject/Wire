#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <algorithm>

namespace basalt::wire::protocol::play {

void BlockEntityDataPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    varInt::write(buffer, typeId);
    // An absent tag is written as TAG_End, which is one byte and what vanilla sends for "nothing".
    writeAnonymousNbt(buffer, tag ? nbt::NbtTag::ofCompound(*tag) : nbt::NbtTag{});
}

std::optional<BlockEntityDataPacket> BlockEntityDataPacket::read(io::ByteBuffer& buffer) {
    BlockEntityDataPacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.typeId = varInt::read(buffer);
    const nbt::NbtTag tag = readAnonymousNbt(buffer);
    if (const auto* compound = tag.get<nbt::NbtCompound>()) {
        packet.tag = *compound;
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void BlockEventPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    buffer.writeByte(b0);
    buffer.writeByte(b1);
    varInt::write(buffer, blockId);
}

std::optional<BlockEventPacket> BlockEventPacket::read(io::ByteBuffer& buffer) {
    BlockEventPacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.b0 = buffer.readByte();
    packet.b1 = buffer.readByte();
    packet.blockId = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

namespace {

// parity: SignTextSlot, BACK 0 and FRONT 1, anything else reading as back.
void writeSignSlot(io::ByteBuffer& buffer, bool front) { varInt::write(buffer, front ? 1 : 0); }

bool readSignSlot(io::ByteBuffer& buffer) { return varInt::read(buffer) == 1; }

} // namespace

void OpenSignEditorPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    writeSignSlot(buffer, isFrontText);
}

std::optional<OpenSignEditorPacket> OpenSignEditorPacket::read(io::ByteBuffer& buffer) {
    OpenSignEditorPacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.isFrontText = readSignSlot(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SignUpdatePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    for (const std::string& line : lines) {
        writeString(buffer, line);
    }
    writeSignSlot(buffer, isFrontText);
}

std::optional<SignUpdatePacket> SignUpdatePacket::read(io::ByteBuffer& buffer) {
    SignUpdatePacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    for (std::string& line : packet.lines) {
        line = readString(buffer, kMaxLineLength);
    }
    packet.isFrontText = readSignSlot(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SetCommandBlockPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    writeString(buffer, command);
    varInt::write(buffer, static_cast<std::int32_t>(mode));
    std::int32_t flags = 0;
    if (trackOutput) {
        flags |= kTrackOutputFlag;
    }
    if (conditional) {
        flags |= kConditionalFlag;
    }
    if (automatic) {
        flags |= kAutomaticFlag;
    }
    buffer.writeByte(static_cast<std::uint8_t>(flags));
}

std::optional<SetCommandBlockPacket> SetCommandBlockPacket::read(io::ByteBuffer& buffer) {
    SetCommandBlockPacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.command = readString(buffer);
    const std::int32_t mode = varInt::read(buffer);
    // A mode the client should never send lands on the impulse block, which is what an unknown enum
    // value does in vanilla's own `readEnum`: it would throw there, and here it is the harmless one.
    packet.mode = mode == static_cast<std::int32_t>(Mode::Sequence) ? Mode::Sequence
                  : mode == static_cast<std::int32_t>(Mode::Auto)   ? Mode::Auto
                                                                    : Mode::Redstone;
    const std::int32_t flags = buffer.readByte();
    packet.trackOutput = (flags & kTrackOutputFlag) != 0;
    packet.conditional = (flags & kConditionalFlag) != 0;
    packet.automatic = (flags & kAutomaticFlag) != 0;
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SetStructureBlockPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    varInt::write(buffer, static_cast<std::int32_t>(update));
    varInt::write(buffer, static_cast<std::int32_t>(mode));
    writeString(buffer, name);
    for (const std::int32_t axis : offset) {
        buffer.writeByte(static_cast<std::uint8_t>(axis));
    }
    for (const std::int32_t axis : size) {
        buffer.writeByte(static_cast<std::uint8_t>(axis));
    }
    varInt::write(buffer, mirror);
    varInt::write(buffer, rotation);
    writeString(buffer, data);
    buffer.writeFloat(integrity);
    varLong::write(buffer, seed);
    std::int32_t flags = 0;
    flags |= ignoreEntities ? kIgnoreEntitiesFlag : 0;
    flags |= showAir ? kShowAirFlag : 0;
    flags |= showBoundingBox ? kShowBoundingBoxFlag : 0;
    flags |= strict ? kStrictFlag : 0;
    buffer.writeByte(static_cast<std::uint8_t>(flags));
}

std::optional<SetStructureBlockPacket> SetStructureBlockPacket::read(io::ByteBuffer& buffer) {
    SetStructureBlockPacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    const std::int32_t update = varInt::read(buffer);
    const std::int32_t mode = varInt::read(buffer);
    // parity: `readEnum`, which refuses an ordinal past the end.
    if (update < 0 || update > static_cast<std::int32_t>(Update::ScanArea) || mode < 0 ||
        mode > static_cast<std::int32_t>(Mode::Data)) {
        return std::nullopt;
    }
    packet.update = static_cast<Update>(update);
    packet.mode = static_cast<Mode>(mode);
    packet.name = readString(buffer);
    for (std::int32_t& axis : packet.offset) {
        axis = std::clamp<std::int32_t>(static_cast<std::int8_t>(buffer.readByte()), -kMaxOffset, kMaxOffset);
    }
    for (std::int32_t& axis : packet.size) {
        axis = std::clamp<std::int32_t>(static_cast<std::int8_t>(buffer.readByte()), 0, kMaxSize);
    }
    packet.mirror = varInt::read(buffer);
    packet.rotation = varInt::read(buffer);
    if (packet.mirror < 0 || packet.mirror > 2 || packet.rotation < 0 || packet.rotation > 3) {
        return std::nullopt;
    }
    packet.data = readString(buffer, kMaxDataLength);
    packet.integrity = std::clamp(buffer.readFloat(), 0.0F, 1.0F);
    packet.seed = varLong::read(buffer);
    const std::int32_t flags = buffer.readByte();
    packet.ignoreEntities = (flags & kIgnoreEntitiesFlag) != 0;
    packet.strict = (flags & kStrictFlag) != 0;
    packet.showAir = (flags & kShowAirFlag) != 0;
    packet.showBoundingBox = (flags & kShowBoundingBoxFlag) != 0;
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
