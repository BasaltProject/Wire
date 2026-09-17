#include <basalt/wire/common/JavaNumbers.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace basalt::wire::common {
namespace {

constexpr double kPlainLow = 1.0E-3;
constexpr double kPlainHigh = 1.0E7;

template <typename T>
[[nodiscard]] std::string format(T value) {
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value > 0 ? "Infinity" : "-Infinity";
    }
    if (value == 0) {
        return std::signbit(value) ? "-0.0" : "0.0";
    }
    // Shortest round-trip digits as d.ddde±x, which Java's own algorithm also picks.
    std::array<char, 48> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::scientific);
    const std::string_view text{buffer.data(), result.ptr};
    const std::size_t e = text.find('e');
    std::string_view mantissa = text.substr(0, e);
    const int exponent = std::atoi(std::string{text.substr(e + 1)}.c_str());
    std::string out;
    if (mantissa.front() == '-') {
        out += '-';
        mantissa.remove_prefix(1);
    }
    std::string digits;
    for (const char c : mantissa) {
        if (c != '.') {
            digits += c;
        }
    }
    const double magnitude = std::abs(static_cast<double>(value));
    if (magnitude >= kPlainLow && magnitude < kPlainHigh) {
        if (exponent >= 0) {
            const auto whole = static_cast<std::size_t>(exponent) + 1;
            while (digits.size() < whole) {
                digits += '0';
            }
            out += digits.substr(0, whole);
            out += '.';
            out += digits.size() > whole ? digits.substr(whole) : "0";
        } else {
            out += "0.";
            out.append(static_cast<std::size_t>(-exponent - 1), '0');
            out += digits;
        }
        return out;
    }
    out += digits.front();
    out += '.';
    out += digits.size() > 1 ? digits.substr(1) : "0";
    out += 'E';
    out += std::to_string(exponent);
    return out;
}

} // namespace

std::string javaToString(double value) { return format(value); }

std::string javaToString(float value) { return format(value); }

} // namespace basalt::wire::common
