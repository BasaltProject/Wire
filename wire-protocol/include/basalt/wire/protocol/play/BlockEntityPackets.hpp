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
struct BlockEntityDataPacket {
    static constexpr std::int32_t kId = 0x06;

    BlockPos pos;
    std::int32_t typeId = 0;
    std::optional<nbt::NbtCompound> tag;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BlockEntityDataPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BlockEntityDataPacket&, const BlockEntityDataPacket&) = default;
};

/// The bytes are named after the packet because each block reads them its own way.
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

struct OpenSignEditorPacket {
    static constexpr std::int32_t kId = 0x3d;

    BlockPos pos;
    bool isFrontText = true;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<OpenSignEditorPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const OpenSignEditorPacket&, const OpenSignEditorPacket&) = default;
};

struct SignUpdatePacket {
    static constexpr std::int32_t kId = 0x3e;
    static constexpr std::size_t kLines = 4;
    /// parity: ServerboundSignUpdatePacket.MAX_STRING_LENGTH.
    static constexpr std::int32_t kMaxLineLength = 384;

    BlockPos pos;
    bool isFrontText = true;
    std::array<std::string, kLines> lines;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SignUpdatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SignUpdatePacket&, const SignUpdatePacket&) = default;
};

/// parity: `ServerboundSetCommandBlockPacket`.
struct SetCommandBlockPacket {
    static constexpr std::int32_t kId = 0x37;

    /// parity: `CommandBlockEntity.Mode`.
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

/// parity: `ServerboundSetStructureBlockPacket`.
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
    /// parity: `Mirror`.
    std::int32_t mirror = 0;
    /// parity: `Rotation`.
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
