#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace basalt::wire::protocol::play {

/// Clientbound, id 0x06: one block entity's data, for a change that is not a change of block.
///
/// A chunk carries its block entities with it, so this is what keeps them up to date afterwards: the
/// text written on a sign, a chest that was renamed.
struct BlockEntityDataPacket {
    static constexpr std::int32_t kId = 0x06;

    BlockPos pos;
    std::int32_t typeId = 0;
    std::optional<nbt::NbtCompound> tag;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BlockEntityDataPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BlockEntityDataPacket&, const BlockEntityDataPacket&) = default;
};

/// Clientbound, id 0x07: a one-off event on a block, as two bytes the block itself gives meaning to.
///
/// It is how a chest lid opens, a note block plays and a piston is drawn sliding: none of that is a
/// change of state, so nothing else carries it. What the two bytes mean is up to the block, which is
/// why they are named after the packet and not after any one use.
struct BlockEventPacket {
    static constexpr std::int32_t kId = 0x07;

    BlockPos pos;
    std::uint8_t b0 = 0;
    std::uint8_t b1 = 0;
    std::int32_t blockId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BlockEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BlockEventPacket&, const BlockEventPacket&) = default;
};

/// Clientbound, id 0x3c: open the sign editor. The client refuses to edit a sign it was not asked
/// to edit, so placing one without this leaves a blank sign and no way to write on it.
struct OpenSignEditorPacket {
    static constexpr std::int32_t kId = 0x3c;

    BlockPos pos;
    bool isFrontText = true;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<OpenSignEditorPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const OpenSignEditorPacket&, const OpenSignEditorPacket&) = default;
};

/// Serverbound, id 0x3d: the four lines someone typed.
///
/// They arrive as plain text, not as components: what the client sends is exactly what was typed,
/// and the server is what turns it into the text a sign shows.
struct SignUpdatePacket {
    static constexpr std::int32_t kId = 0x3d;
    static constexpr std::size_t kLines = 4;
    /// parity: ServerboundSignUpdatePacket.MAX_STRING_LENGTH, which is not the sign's own limit.
    static constexpr std::int32_t kMaxLineLength = 384;

    BlockPos pos;
    bool isFrontText = true;
    std::array<std::string, kLines> lines;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SignUpdatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SignUpdatePacket&, const SignUpdatePacket&) = default;
};

/// Serverbound, id 0x36: what somebody typed into a command block's screen.
///
/// parity: `ServerboundSetCommandBlockPacket`. The three modes are the block itself (an impulse, a
/// repeating or a chain block), so accepting one means putting a different block down; the flags are
/// a single byte and vanilla names them one, two and four.
struct SetCommandBlockPacket {
    static constexpr std::int32_t kId = 0x36;

    /// parity: `CommandBlockEntity.Mode`, in the enum's own order, which is what the byte carries.
    enum class Mode : std::int32_t { Sequence, Auto, Redstone };

    static constexpr std::int32_t kTrackOutputFlag = 1;
    static constexpr std::int32_t kConditionalFlag = 2;
    static constexpr std::int32_t kAutomaticFlag = 4;

    BlockPos pos;
    std::string command;
    Mode mode = Mode::Redstone;
    bool trackOutput = false;
    bool conditional = false;
    bool automatic = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetCommandBlockPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetCommandBlockPacket&, const SetCommandBlockPacket&) = default;
};

} // namespace basalt::wire::protocol::play
