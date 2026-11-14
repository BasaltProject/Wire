#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::multiversion::java {

/// parity: InventoryMenu's slot for a slot of the player's Inventory, the same from 1.9 to 1.21.4.
[[nodiscard]] std::int32_t inventoryMenuSlot(std::int32_t inventorySlot);

/// What a 1.16.5 or older server last showed of the inventory, the open container and the cursor, so a click from a
/// newer client can carry the exact stack the server compares its own result with, and no resync follows.
class LegacyClicks {
public:
    explicit LegacyClicks(std::int32_t backendVersion) : backendVersion_(backendVersion) {}

    /// Notes a packet in 1.16.5's numbering and shape.
    void clientbound(std::int32_t id, std::span<const std::uint8_t> body);
    /// Rewrites a 1.17 click into 1.16.5's, keeping the view in step with what the server will do.
    [[nodiscard]] bool click(io::ByteBuffer& in, io::ByteBuffer& out);

private:
    using Stack = std::vector<std::uint8_t>;

    struct Window {
        std::int32_t id = -1;
        std::vector<Stack> slots;
    };

    [[nodiscard]] Window* window(std::int32_t id);
    [[nodiscard]] const Stack& at(const Window& window, std::int32_t slot) const;
    void set(Window& window, std::int32_t slot, Stack stack);
    [[nodiscard]] Stack resolve(std::span<const std::uint8_t> claimed, std::span<const Stack> before) const;

    std::int32_t backendVersion_;
    Window inventory_{0, {}};
    Window open_;
    Stack cursor_{0};
};

} // namespace basalt::wire::multiversion::java
