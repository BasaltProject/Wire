#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/java/BlockEntityStates.hpp>
#include <basalt/wire/multiversion/java/OlderBlockEntities.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace paletted = protocol::play::palettedContainer;

constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr std::size_t kBiomesPerSection = 4 * 4 * 4;
/// The overworld's 384 blocks from y -64; the other vanilla dimensions start at 0.
constexpr std::size_t kOverworldSections = 24;
constexpr std::int32_t kOverworldMinY = -64;

} // namespace

void BlockEntityStates::remember(std::int64_t packedPos, std::int32_t nativeState) {
    const bool shaped = olderBlockEntities::shapedByState(nativeState);
    for (std::size_t index = 0; index < used_; ++index) {
        if (entries_[index].first == packedPos) {
            entries_[index].second = shaped ? nativeState : kForgotten;
            return;
        }
    }
    if (!shaped) {
        return;
    }
    entries_[next_] = {packedPos, nativeState};
    next_ = (next_ + 1) % kCapacity;
    used_ = used_ < kCapacity ? used_ + 1 : kCapacity;
}

std::optional<std::int32_t> BlockEntityStates::at(std::int64_t packedPos) const {
    for (std::size_t index = 0; index < used_; ++index) {
        if (entries_[index].first == packedPos && entries_[index].second != kForgotten) {
            return entries_[index].second;
        }
    }
    return std::nullopt;
}

void BlockEntityStates::remember(std::int32_t sectionX, std::int32_t sectionY, std::int32_t sectionZ,
                                 std::int16_t local, std::int32_t nativeState) {
    const protocol::play::BlockPos pos{(sectionX << 4) | ((local >> 8) & 0xF), (sectionY << 4) | (local & 0xF),
                                       (sectionZ << 4) | ((local >> 4) & 0xF)};
    remember(pos.pack(), nativeState);
}

void BlockEntityStates::Chunk::read() const {
    read_ = true;
    io::ByteBuffer input = io::ByteBuffer::wrap(std::vector<std::uint8_t>{sections_.begin(), sections_.end()});
    while (input.readableBytes() > 0 && !input.failed()) {
        static_cast<void>(input.readShort());
        if (countsFluids_) {
            static_cast<void>(input.readShort());
        }
        std::vector<std::int32_t> section = paletted::read(input, kBlocksPerSection, paletted::kBlockMaxIndirectBits);
        static_cast<void>(paletted::read(input, kBiomesPerSection, paletted::kBiomeMaxIndirectBits));
        if (input.failed() || section.size() != kBlocksPerSection) {
            break;
        }
        blocks_.push_back(std::move(section));
    }
    minY_ = blocks_.size() == kOverworldSections ? kOverworldMinY : 0;
}

std::optional<std::int32_t> BlockEntityStates::Chunk::at(std::uint8_t packedXZ, std::int32_t y) const {
    if (!read_) {
        read();
    }
    const std::int32_t sectionIndex = (y - minY_) >> 4;
    if (sectionIndex < 0 || static_cast<std::size_t>(sectionIndex) >= blocks_.size()) {
        return std::nullopt;
    }
    const auto x = static_cast<std::size_t>(packedXZ >> 4);
    const auto z = static_cast<std::size_t>(packedXZ & 0xF);
    const auto local = static_cast<std::size_t>(y & 0xF);
    return blocks_[static_cast<std::size_t>(sectionIndex)][(local << 8) | (z << 4) | x];
}

} // namespace basalt::wire::multiversion::java
