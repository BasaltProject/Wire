#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::wire::protocol::play {

/// parity: `DisplayInfo` as it travels. The icon is an `ItemStackTemplate`: item, count, patch.
struct AdvancementDisplayData {
    text::Component title = text::Component::empty();
    text::Component description = text::Component::empty();
    WireItemStack icon;
    /// `AdvancementType` ids: 0 task, 1 challenge, 2 goal.
    std::int32_t frame = 0;
    std::optional<std::string> background;
    bool showToast = true;
    bool hidden = false;

    friend bool operator==(const AdvancementDisplayData&, const AdvancementDisplayData&) = default;
};

/// One advancement as the client builds its screen from. parity: `PositionedAdvancement`.
struct AdvancementEntry {
    std::string id;
    std::optional<std::string> parent;
    std::optional<AdvancementDisplayData> display;
    std::vector<std::vector<std::string>> requirements;
    bool sendsTelemetryEvent = false;
    /// Where the screen draws it. parity: `AdvancementNode.x`/`y`, set by `TreeNodePosition`.
    float x = 0.0F;
    float y = 0.0F;

    friend bool operator==(const AdvancementEntry&, const AdvancementEntry&) = default;
};

/// One advancement's criteria and when each was obtained, in epoch milliseconds, or nothing.
struct AdvancementProgressData {
    std::string id;
    std::vector<std::pair<std::string, std::optional<std::int64_t>>> criteria;

    friend bool operator==(const AdvancementProgressData&, const AdvancementProgressData&) = default;
};

/// Clientbound, id 0x85. parity: `ClientboundUpdateAdvancementsPacket`.
struct UpdateAdvancementsPacket {
    static constexpr std::int32_t kId = 0x85;

    bool reset = false;
    std::vector<AdvancementEntry> added;
    std::vector<std::string> removed;
    std::vector<AdvancementProgressData> progress;
    /// Whether the client shows the toasts this update earns.
    bool showAdvancements = true;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<UpdateAdvancementsPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const UpdateAdvancementsPacket&, const UpdateAdvancementsPacket&) = default;
};

/// Clientbound, id 0x57: the tab the advancement screen opens on, or none.
struct SelectAdvancementsTabPacket {
    static constexpr std::int32_t kId = 0x57;

    std::optional<std::string> tab;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SelectAdvancementsTabPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SelectAdvancementsTabPacket&, const SelectAdvancementsTabPacket&) = default;
};

/// Serverbound, id 0x33: a tab opened, or the screen closed.
struct SeenAdvancementsPacket {
    static constexpr std::int32_t kId = 0x33;

    enum class Action : std::uint8_t { OpenedTab = 0, ClosedScreen = 1 };

    Action action = Action::ClosedScreen;
    std::optional<std::string> tab;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SeenAdvancementsPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SeenAdvancementsPacket&, const SeenAdvancementsPacket&) = default;
};

} // namespace basalt::wire::protocol::play
