#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

namespace basalt::wire::multiversion::java::stack765 {

/// Reads a 1.20.4 stack (item id, count, NBT) and writes a 1.20.5 one with the few components it can name.
[[nodiscard]] bool toComponents(io::ByteBuffer& in, io::ByteBuffer& out);

} // namespace basalt::wire::multiversion::java::stack765
