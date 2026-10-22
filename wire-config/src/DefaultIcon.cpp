#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/config/DefaultIcon.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::wire::config::defaultIcon {
namespace {

constexpr std::string_view kBase64 =
    "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"
    "6/P/AmlpWTdOSInDAAAAAElFTkSuQmCC";

} // namespace

std::span<const std::uint8_t> bytes() {
    // Decoded once and kept: the status response asks for it on every ping.
    static const std::vector<std::uint8_t> kDecoded = [] {
        const std::optional<std::vector<std::uint8_t>> raw = common::base64::decode(kBase64);
        return raw.has_value() ? *raw : std::vector<std::uint8_t>{};
    }();
    return kDecoded;
}

} // namespace basalt::wire::config::defaultIcon
