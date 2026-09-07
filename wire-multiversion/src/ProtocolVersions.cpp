#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <array>
#include <cstdint>

namespace basalt::wire::multiversion {
namespace {

constexpr std::array<ProtocolVersion, 49> kAll = {{
    ProtocolVersion{47, "1.8-1.8.9"},      ProtocolVersion{107, "1.9"},
    ProtocolVersion{108, "1.9.1"},         ProtocolVersion{109, "1.9.2"},
    ProtocolVersion{110, "1.9.3-1.9.4"},   ProtocolVersion{210, "1.10-1.10.2"},
    ProtocolVersion{315, "1.11"},          ProtocolVersion{316, "1.11.1-1.11.2"},
    ProtocolVersion{335, "1.12"},          ProtocolVersion{338, "1.12.1"},
    ProtocolVersion{340, "1.12.2"},        ProtocolVersion{393, "1.13"},
    ProtocolVersion{401, "1.13.1"},        ProtocolVersion{404, "1.13.2"},
    ProtocolVersion{477, "1.14"},          ProtocolVersion{480, "1.14.1"},
    ProtocolVersion{485, "1.14.2"},        ProtocolVersion{490, "1.14.3"},
    ProtocolVersion{498, "1.14.4"},        ProtocolVersion{573, "1.15"},
    ProtocolVersion{575, "1.15.1"},        ProtocolVersion{578, "1.15.2"},
    ProtocolVersion{735, "1.16"},          ProtocolVersion{736, "1.16.1"},
    ProtocolVersion{751, "1.16.2"},        ProtocolVersion{753, "1.16.3"},
    ProtocolVersion{754, "1.16.4-1.16.5"}, ProtocolVersion{755, "1.17"},
    ProtocolVersion{756, "1.17.1"},        ProtocolVersion{757, "1.18-1.18.1"},
    ProtocolVersion{758, "1.18.2"},        ProtocolVersion{759, "1.19"},
    ProtocolVersion{760, "1.19.1-1.19.2"}, ProtocolVersion{761, "1.19.3"},
    ProtocolVersion{762, "1.19.4"},        ProtocolVersion{763, "1.20-1.20.1"},
    ProtocolVersion{764, "1.20.2"},        ProtocolVersion{765, "1.20.3-1.20.4"},
    ProtocolVersion{766, "1.20.5-1.20.6"}, ProtocolVersion{767, "1.21-1.21.1"},
    ProtocolVersion{768, "1.21.2-1.21.3"}, ProtocolVersion{769, "1.21.4"},
    ProtocolVersion{770, "1.21.5"},        ProtocolVersion{771, "1.21.6"},
    ProtocolVersion{772, "1.21.7-1.21.8"}, ProtocolVersion{773, "1.21.9-1.21.10"},
    ProtocolVersion{774, "1.21.11"},       ProtocolVersion{775, "26.1-26.1.2"},
    ProtocolVersion{776, "26.2"},
}};

} // namespace

std::string ProtocolVersion::toString() const { return std::string{label} + " (" + std::to_string(number) + ")"; }

namespace protocolVersions {

std::span<const ProtocolVersion> known() noexcept { return kAll; }

std::int32_t oldest() noexcept { return kAll.front().number; }

std::optional<ProtocolVersion> of(std::int32_t number) noexcept {
    // The table is sorted, so this is a binary search rather than a map: it is read on every
    // handshake and never written.
    std::size_t low = 0;
    std::size_t high = kAll.size();
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        if (kAll[mid].number < number) {
            low = mid + 1;
        } else if (kAll[mid].number > number) {
            high = mid;
        } else {
            return kAll[mid];
        }
    }
    return std::nullopt;
}

std::string describe(std::int32_t number) {
    if (isSnapshot(number)) {
        return "snapshot " + std::to_string(number & ~kSnapshotBit) + " (" + std::to_string(number) + ")";
    }
    if (const std::optional<ProtocolVersion> version = of(number)) {
        return version->toString();
    }
    return "unknown (" + std::to_string(number) + ")";
}

bool isBelowFloor(std::int32_t number) noexcept { return !isSnapshot(number) && number < oldest(); }

} // namespace protocolVersions

} // namespace basalt::wire::multiversion
