#include "PacketNameTable.hpp"

#include <basalt/wire/protocol/PacketNames.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <unordered_map>

namespace basalt::wire::protocol::packetNames {
namespace {
using PacketAddress = Address;

constexpr std::array<std::string_view, 5> kPhases{"handshake", "status", "login", "configuration", "play"};

struct Names {
    std::map<std::string, PacketAddress, std::less<>> byName;
    std::unordered_map<std::int32_t, std::string> canonical;
};

[[nodiscard]] std::int32_t keyOf(const PacketAddress& address) {
    return (static_cast<std::int32_t>(address.phase) << 17) | (address.serverbound ? 1 << 16 : 0) | address.id;
}

[[nodiscard]] std::string_view side(bool serverbound) { return serverbound ? "serverbound" : "clientbound"; }

[[nodiscard]] std::string spelled(bool serverbound, std::string_view middle, std::string_view name) {
    std::string out{side(serverbound)};
    out += '.';
    if (!middle.empty()) {
        out += middle;
        out += '.';
    }
    out += name;
    return out;
}

[[nodiscard]] Names build() {
    Names names;
    std::map<std::string, int> uses;
    for (const Row& row : rows()) {
        uses[spelled(row.serverbound, "", row.name)]++;
    }
    for (const Row& row : rows()) {
        const PacketAddress address{row.phase, row.serverbound, row.id};
        const std::string_view phase = kPhases[static_cast<std::size_t>(row.phase)];
        const std::string shortName = spelled(row.serverbound, "", row.name);
        const bool unique = uses[shortName] == 1;
        const std::string longName = spelled(row.serverbound, phase, row.name);
        if (unique) {
            names.byName.emplace(shortName, address);
        }
        names.byName.emplace(longName, address);
        names.canonical.emplace(keyOf(address), unique ? shortName : longName);
    }
    for (const Row& row : aliases()) {
        const PacketAddress address{row.phase, row.serverbound, row.id};
        const std::string_view phase = kPhases[static_cast<std::size_t>(row.phase)];
        names.byName.emplace(spelled(row.serverbound, phase, row.name), address);
        if (uses[spelled(row.serverbound, "", row.name)] == 0) {
            names.byName.emplace(spelled(row.serverbound, "", row.name), address);
        }
    }
    return names;
}

[[nodiscard]] const Names& names() {
    static const Names kNames = build();
    return kNames;
}

[[nodiscard]] std::optional<std::int32_t> phaseIndex(std::string_view word) {
    for (std::size_t index = 0; index < kPhases.size(); index++) {
        if (kPhases[index] == word) {
            return static_cast<std::int32_t>(index);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::int32_t> hexId(std::string_view word) {
    if (!word.starts_with("0x") || word.size() < 3) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const auto [end, error] = std::from_chars(word.data() + 2, word.data() + word.size(), value, 16);
    if (error != std::errc{} || end != word.data() + word.size()) {
        return std::nullopt;
    }
    return value;
}
} // namespace

std::string of(const PacketAddress& address) {
    const auto found = names().canonical.find(keyOf(address));
    if (found != names().canonical.end()) {
        return found->second;
    }
    std::string hex;
    constexpr std::string_view kDigits = "0123456789abcdef";
    if (address.id > 0xFF) {
        hex += kDigits[static_cast<std::size_t>((address.id >> 12) & 0xF)];
        hex += kDigits[static_cast<std::size_t>((address.id >> 8) & 0xF)];
    }
    hex += kDigits[static_cast<std::size_t>((address.id >> 4) & 0xF)];
    hex += kDigits[static_cast<std::size_t>(address.id & 0xF)];
    return spelled(address.serverbound, kPhases[static_cast<std::size_t>(address.phase)], "0x" + hex);
}

std::optional<PacketAddress> resolve(std::string_view name) {
    const auto found = names().byName.find(name);
    if (found != names().byName.end()) {
        return found->second;
    }
    // side.phase.0xNN
    const bool serverbound = name.starts_with("serverbound.");
    if (!serverbound && !name.starts_with("clientbound.")) {
        return std::nullopt;
    }
    std::string_view rest = name.substr(12);
    const std::size_t dot = rest.find('.');
    if (dot == std::string_view::npos) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> phase = phaseIndex(rest.substr(0, dot));
    const std::optional<std::int32_t> id = hexId(rest.substr(dot + 1));
    if (!phase.has_value() || !id.has_value()) {
        return std::nullopt;
    }
    return PacketAddress{static_cast<Phase>(*phase), serverbound, *id};
}

std::optional<Phase> phaseOf(std::string_view word) {
    const auto index = phaseIndex(word);
    return index.has_value() ? std::optional{static_cast<Phase>(*index)} : std::nullopt;
}

std::vector<std::string> all() {
    std::vector<std::string> out;
    out.reserve(names().canonical.size());
    for (const auto& [key, name] : names().canonical) {
        out.push_back(name);
    }
    std::ranges::sort(out);
    return out;
}
} // namespace basalt::wire::protocol::packetNames
