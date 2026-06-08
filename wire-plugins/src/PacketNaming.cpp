#include <basalt/wire/plugins/PacketNaming.hpp>
#include <basalt/wire/protocol/PacketNames.hpp>

namespace basalt::wire::plugins::packetNaming {
namespace {
namespace shared = protocol::packetNames;

[[nodiscard]] shared::Address toShared(const PacketAddress& address) {
    return {static_cast<shared::Phase>(address.phase), address.serverbound, address.id};
}

[[nodiscard]] PacketAddress fromShared(const shared::Address& address) {
    return {static_cast<PacketPhase>(address.phase), address.serverbound, address.id};
}
} // namespace

std::string of(const PacketAddress& address) { return shared::of(toShared(address)); }

std::optional<PacketAddress> resolve(std::string_view name) {
    const auto found = shared::resolve(name);
    return found.has_value() ? std::optional{fromShared(*found)} : std::nullopt;
}

std::optional<PacketScope> scopeOf(std::string_view name) {
    if (name == "*") {
        return PacketScope{};
    }
    if (const std::optional<PacketAddress> exact = resolve(name); exact.has_value()) {
        return PacketScope{static_cast<std::int32_t>(exact->phase), exact->serverbound ? 1 : 0, exact->id};
    }
    if (!name.ends_with(".*")) {
        return std::nullopt;
    }
    name.remove_suffix(2);
    if (name == "serverbound" || name == "clientbound") {
        return PacketScope{-1, name == "serverbound" ? 1 : 0, -1};
    }
    const bool serverbound = name.starts_with("serverbound.");
    if (!serverbound && !name.starts_with("clientbound.")) {
        return std::nullopt;
    }
    const auto phase = shared::phaseOf(name.substr(12));
    if (!phase.has_value()) {
        return std::nullopt;
    }
    return PacketScope{static_cast<std::int32_t>(*phase), serverbound ? 1 : 0, -1};
}

std::vector<std::string> all() { return shared::all(); }
} // namespace basalt::wire::plugins::packetNaming
