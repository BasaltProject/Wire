#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::wire::nbt {

struct SnbtValue {
    NbtTag tag;
    std::size_t position = 0;
};

/// stringify is canonical and round-trips through parse, but is not byte-identical to vanilla's formatter.
namespace snbt {

[[nodiscard]] std::string stringify(const NbtTag& tag);

[[nodiscard]] std::expected<NbtTag, NbtError> parse(std::string_view text);

[[nodiscard]] std::expected<SnbtValue, NbtError> parseFrom(std::string_view text, std::size_t from);

} // namespace snbt

} // namespace basalt::wire::nbt
