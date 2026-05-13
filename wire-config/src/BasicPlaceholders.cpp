#include <basalt/wire/config/BasicPlaceholders.hpp>
#include <basalt/wire/config/ServerListConfig.hpp>

#include <utility>

namespace basalt::wire::config {
namespace {

namespace ph = common::placeholder;

constexpr std::string_view kNoPing = "this text is not about any player";

ph::PlaceholderValue ofPing(std::string_view params, const ph::PlaceholderContext& context) {
    const PingRequest* ping = context.get(pingSlot());
    if (ping == nullptr) {
        return ph::PlaceholderValue::unavailable(std::string{kNoPing});
    }
    if (params == "protocol") {
        return ph::PlaceholderValue::of(static_cast<std::int64_t>(ping->protocolVersion()));
    }
    if (params == "host") {
        return ph::PlaceholderValue::of(ping->host());
    }
    if (params == "ip") {
        return ping->address().has_value() ? ph::PlaceholderValue::of(*ping->address())
                                           : ph::PlaceholderValue::unavailable("the address is not known");
    }
    if (params == "known") {
        return ph::PlaceholderValue::of(ping->knownPlayer().has_value());
    }
    if (params == "name") {
        return ping->knownPlayer().has_value() ? ph::PlaceholderValue::of(*ping->knownPlayer())
                                               : ph::PlaceholderValue::unavailable("nobody is known at this address");
    }
    return ph::PlaceholderValue::unknown();
}

} // namespace

void registerBasicPlaceholders(ph::PlaceholderRegistry& registry, BasicFacts facts) {
    registry.registerProvider(
        "basalt_server",
        [facts = std::move(facts)](std::string_view params, const ph::PlaceholderContext&) {
            if (params == "version") {
                return ph::PlaceholderValue::of(facts.versionName);
            }
            if (params == "protocol") {
                return ph::PlaceholderValue::of(static_cast<std::int64_t>(facts.protocolVersion));
            }
            if (params == "online") {
                return ph::PlaceholderValue::of(static_cast<std::int64_t>(facts.online ? facts.online() : 0));
            }
            if (params == "max_players") {
                return ph::PlaceholderValue::of(static_cast<std::int64_t>(facts.maxPlayers ? facts.maxPlayers() : 0));
            }
            return ph::PlaceholderValue::unknown();
        },
        [] { return std::vector<std::string>{"max_players", "online", "protocol", "version"}; });
    registry.registerProvider("basalt_player", ofPing,
                              [] { return std::vector<std::string>{"host", "ip", "known", "name", "protocol"}; });
}

} // namespace basalt::wire::config
