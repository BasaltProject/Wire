#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

namespace basalt::wire::multiversion::java::stack769 {

/// Reads a 1.21.4 stack and writes it with this build's component ids; components this build cannot carry are dropped.
[[nodiscard]] bool convert(io::ByteBuffer& in, io::ByteBuffer& out);

/// While alive, stacks are read the way 1.21.2 sent them: custom_model_data is a bare varint.
class IntModelDataScope {
public:
    explicit IntModelDataScope(bool enabled);
    ~IntModelDataScope();
    IntModelDataScope(const IntModelDataScope&) = delete;
    IntModelDataScope& operator=(const IntModelDataScope&) = delete;

private:
    bool previous_;
};

/// The way back for a stack a client sends: only the components both sides share survive.
[[nodiscard]] bool convertToServer(io::ByteBuffer& in, io::ByteBuffer& out);

} // namespace basalt::wire::multiversion::java::stack769
