#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java::stack769 {

/// Reads a 1.21.4 stack and writes it with this build's component ids; components this build cannot carry are dropped.
[[nodiscard]] bool convert(io::ByteBuffer& in, io::ByteBuffer& out);

/// The version that wrote a stack: 1.21.2 sent custom_model_data as a varint, and 1.21 had older food and potions too.
enum class Source : std::uint8_t { k1_21_4, k1_21_2, k1_21, k1_20_5 };

[[nodiscard]] Source sourceFor(std::int32_t backendVersion);

/// While alive, stacks are read the way `source` wrote them.
class SourceScope {
public:
    explicit SourceScope(Source source);
    ~SourceScope();
    SourceScope(const SourceScope&) = delete;
    SourceScope& operator=(const SourceScope&) = delete;

private:
    Source previous_;
};

/// The way back for a stack a client sends: only the components both sides share survive.
[[nodiscard]] bool convertToServer(io::ByteBuffer& in, io::ByteBuffer& out);

} // namespace basalt::wire::multiversion::java::stack769
