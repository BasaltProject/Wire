#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play::componentSkip {

/// Advances past one native data component payload; false, with the buffer failed, if it cannot.
[[nodiscard]] bool skipPayload(io::ByteBuffer& buffer, std::int32_t id);

} // namespace basalt::wire::protocol::play::componentSkip
