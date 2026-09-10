#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

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

void OpenSignEditorPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    writeBoolean(buffer, isFrontText);
}

std::optional<OpenSignEditorPacket> OpenSignEditorPacket::read(io::ByteBuffer& buffer) {
    OpenSignEditorPacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.isFrontText = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SignUpdatePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    writeBoolean(buffer, isFrontText);
    for (const std::string& line : lines) {
        writeString(buffer, line);
    }
}

std::optional<SignUpdatePacket> SignUpdatePacket::read(io::ByteBuffer& buffer) {
    SignUpdatePacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.isFrontText = readBoolean(buffer);
    for (std::string& line : packet.lines) {
        line = readString(buffer, kMaxLineLength);
    }
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

} // namespace basalt::wire::protocol::play
