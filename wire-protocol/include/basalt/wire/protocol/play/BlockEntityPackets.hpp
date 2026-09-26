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
/// How a chest lid opens, a note block plays and a piston is drawn sliding: none of it is a change of
/// state. The bytes are named after the packet because each block reads them its own way.
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

/// Clientbound, id 0x3d: open the sign editor. The client refuses to edit a sign it was not asked
/// to edit, so placing one without this leaves a blank sign and no way to write on it.
struct OpenSignEditorPacket {
    static constexpr std::int32_t kId = 0x3d;

    BlockPos pos;
    bool isFrontText = true;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<OpenSignEditorPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const OpenSignEditorPacket&, const OpenSignEditorPacket&) = default;
};

/// Serverbound, id 0x3e: the four lines someone typed.
///
/// They arrive as plain text, not as components: what the client sends is exactly what was typed,
/// and the server is what turns it into the text a sign shows.
struct SignUpdatePacket {
    static constexpr std::int32_t kId = 0x3e;
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

/// Serverbound, id 0x37: what somebody typed into a command block's screen.
///
/// parity: `ServerboundSetCommandBlockPacket`. The mode is the block itself (impulse, repeating or
/// chain), so accepting one puts a different block down; the flags are one byte (1, 2, 4).
struct SetCommandBlockPacket {
    static constexpr std::int32_t kId = 0x37;

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

/// Serverbound, id 0x3c: what somebody set in a structure block's screen, and the button they pressed.
///
/// parity: `ServerboundSetStructureBlockPacket`. The offset and size travel as clamped bytes and the
/// four switches as one flag byte; the enums are varints in their declared order.
struct SetStructureBlockPacket {
    static constexpr std::int32_t kId = 0x3c;

    /// parity: `StructureBlockEntity.UpdateType`.
    enum class Update : std::int32_t { UpdateData, SaveArea, LoadArea, ScanArea };
    /// parity: `StructureMode`.
    enum class Mode : std::int32_t { Save, Load, Corner, Data };

    static constexpr std::int32_t kMaxOffset = 48;
    static constexpr std::int32_t kMaxSize = 48;
    static constexpr std::int32_t kMaxDataLength = 128;
    static constexpr std::int32_t kIgnoreEntitiesFlag = 1;
    static constexpr std::int32_t kShowAirFlag = 2;
    static constexpr std::int32_t kShowBoundingBoxFlag = 4;
    static constexpr std::int32_t kStrictFlag = 8;

    BlockPos pos;
    Update update = Update::UpdateData;
    Mode mode = Mode::Load;
    std::string name;
    std::array<std::int32_t, 3> offset{};
    std::array<std::int32_t, 3> size{};
    /// parity: `Mirror`, none, left-right and front-back.
    std::int32_t mirror = 0;
    /// parity: `Rotation`, in quarter turns clockwise.
    std::int32_t rotation = 0;
    std::string data;
    bool ignoreEntities = true;
    bool strict = false;
    bool showAir = false;
    bool showBoundingBox = true;
    float integrity = 1.0F;
    std::int64_t seed = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetStructureBlockPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetStructureBlockPacket&, const SetStructureBlockPacket&) = default;
};

} // namespace basalt::wire::protocol::play
