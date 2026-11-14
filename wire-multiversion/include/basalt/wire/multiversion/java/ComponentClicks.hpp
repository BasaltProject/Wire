#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::multiversion::java {

/// What a 1.20.5 to 1.21.4 server last showed of the inventory, the open container and the cursor, so the hashed stacks
/// of a newer client's click go back with the server's own components and the server sees nothing to correct.
class ComponentClicks {
public:
    /// Notes a packet in 1.21.4's numbering and shape, inside the stack source of the server.
    void clientbound(std::int32_t id, std::span<const std::uint8_t> body);
    /// Rewrites the changed slots and cursor of a 1.21.5 click, read after its mode, into 1.21.4's.
    [[nodiscard]] bool click(std::int32_t windowId, io::ByteBuffer& in, io::ByteBuffer& out);

    /// The same for a server before 1.20.5, which compares item and count only.
    [[nodiscard]] static bool namedClick(io::ByteBuffer& in, io::ByteBuffer& out);

private:
    using Stack = std::vector<std::uint8_t>;

    struct Window {
        std::int32_t id = -1;
        std::vector<Stack> slots;
    };

    [[nodiscard]] Window* window(std::int32_t id);
    [[nodiscard]] const Stack& at(const Window& window, std::int32_t slot) const;
    void set(Window& window, std::int32_t slot, Stack stack);
    [[nodiscard]] Stack resolve(std::int32_t item, std::int32_t count, std::span<const Stack> before) const;

    Window inventory_{0, {}};
    Window open_;
    Stack cursor_{0};
};

} // namespace basalt::wire::multiversion::java
