#pragma once

#include "HostApi.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::wasm::trace::packets {

inline constexpr std::size_t kMaxBody = 1U << 20U;

struct Packet {
    std::string_view name;
    std::string_view phase;
    bool serverbound;
    std::int32_t id;
};

/// A small stand-in for the server's packet registry, enough to exercise names, wildcards and both directions.
inline constexpr Packet kPackets[] = {
    {"clientbound.set_title_text", "play", false, 0x6A},
    {"clientbound.system_chat", "play", false, 0x77},
    {"clientbound.configuration.0x07", "configuration", false, 0x07},
    {"serverbound.chat_message", "play", true, 0x08},
    {"serverbound.chat_command", "play", true, 0x06},
    {"serverbound.play.custom_payload", "play", true, 0x15},
};

struct Listener {
    std::string pattern;
    std::vector<const Packet*> matches;
    std::int32_t priority = 2;
    bool observer = false;
    std::int32_t token = 0;
    bool alive = true;
};

struct Traffic {
    const Packet* packet = nullptr;
    std::string channel;
    std::string playerName;
    std::vector<std::uint8_t> bytes;
    std::optional<std::vector<std::uint8_t>> replacement;
    bool cancelled = false;
    bool observer = false;
};

struct State {
    std::vector<Listener> listeners;
    std::vector<std::pair<std::string, std::int32_t>> channels;
    Traffic* active = nullptr;
};

[[nodiscard]] inline State& stateOf(World& world) { return world.areaState<State>("packets"); }

[[nodiscard]] Value blob(const std::vector<std::uint8_t>& bytes);

/// The packets a name or wildcard covers; nothing when the name is unknown.
[[nodiscard]] std::optional<std::vector<const Packet*>> resolve(std::string_view pattern);

/// Runs the listeners for one packet the way the server's registry does; false when one cancelled it.
bool fire(World& world, Traffic& traffic);

[[nodiscard]] std::int32_t activeOf(World& world, std::int32_t ctx, Traffic*& out);

} // namespace basalt::wire::wasm::trace::packets
