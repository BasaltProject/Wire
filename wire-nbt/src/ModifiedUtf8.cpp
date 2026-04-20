#include "ModifiedUtf8.hpp"

#include <cstdint>
#include <vector>

namespace basalt::wire::nbt::modifiedUtf8 {
namespace {

template <typename Sink>
void forEachUtf16Unit(std::string_view value, Sink&& sink) {
    for (std::size_t i = 0; i < value.size();) {
        const auto lead = static_cast<std::uint8_t>(value[i]);
        std::uint32_t code = lead;
        std::size_t extra = 0;
        if (lead >= 0xF0) {
            code = lead & 0x07U;
            extra = 3;
        } else if (lead >= 0xE0) {
            code = lead & 0x0FU;
            extra = 2;
        } else if (lead >= 0xC0) {
            code = lead & 0x1FU;
            extra = 1;
        }
        if (i + extra >= value.size()) {
            extra = 0;
            code = lead;
        }
        for (std::size_t k = 1; k <= extra; ++k) {
            code = (code << 6) | (static_cast<std::uint8_t>(value[i + k]) & 0x3FU);
        }
        i += extra + 1;

        if (code > 0xFFFF) {
            const std::uint32_t adjusted = code - 0x10000U;
            sink(static_cast<std::uint16_t>(0xD800U + (adjusted >> 10)));
            sink(static_cast<std::uint16_t>(0xDC00U + (adjusted & 0x3FFU)));
        } else {
            sink(static_cast<std::uint16_t>(code));
        }
    }
}

void appendUtf8(std::string& out, std::uint32_t code) {
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0U | (code >> 6)));
        out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xE0U | (code >> 12)));
        out.push_back(static_cast<char>(0x80U | ((code >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (code >> 18)));
        out.push_back(static_cast<char>(0x80U | ((code >> 12) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((code >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    }
}

} // namespace

std::size_t encodedLength(std::string_view value) noexcept {
    std::size_t length = 0;
    forEachUtf16Unit(value, [&length](std::uint16_t unit) {
        if (unit >= 0x0001 && unit <= 0x007F) {
            length += 1;
        } else if (unit <= 0x07FF) {
            length += 2;
        } else {
            length += 3;
        }
    });
    return length;
}

void encode(std::string_view value, ByteWriter& sink) {
    forEachUtf16Unit(value, [&sink](std::uint16_t unit) {
        if (unit >= 0x0001 && unit <= 0x007F) {
            sink.writeByte(static_cast<std::uint8_t>(unit));
        } else if (unit <= 0x07FF) {
            sink.writeByte(static_cast<std::uint8_t>(0xC0U | (unit >> 6)));
            sink.writeByte(static_cast<std::uint8_t>(0x80U | (unit & 0x3FU)));
        } else {
            sink.writeByte(static_cast<std::uint8_t>(0xE0U | (unit >> 12)));
            sink.writeByte(static_cast<std::uint8_t>(0x80U | ((unit >> 6) & 0x3FU)));
            sink.writeByte(static_cast<std::uint8_t>(0x80U | (unit & 0x3FU)));
        }
    });
}

std::expected<std::string, NbtError> decode(ByteReader& source, std::size_t byteLength) {
    std::vector<std::uint16_t> units;
    units.reserve(byteLength);

    std::size_t remaining = byteLength;
    while (remaining > 0) {
        const auto lead = source.readByte();
        if (!lead) {
            return std::unexpected{lead.error()};
        }
        const std::uint32_t a = *lead;
        remaining -= 1;

        if ((a & 0x80U) == 0) {
            units.push_back(static_cast<std::uint16_t>(a));
        } else if ((a & 0xE0U) == 0xC0U) {
            if (remaining < 1) {
                return std::unexpected{NbtError::TruncatedUtf8};
            }
            const auto b = source.readByte();
            if (!b) {
                return std::unexpected{b.error()};
            }
            remaining -= 1;
            units.push_back(static_cast<std::uint16_t>(((a & 0x1FU) << 6) | (*b & 0x3FU)));
        } else if ((a & 0xF0U) == 0xE0U) {
            if (remaining < 2) {
                return std::unexpected{NbtError::TruncatedUtf8};
            }
            const auto b = source.readByte();
            const auto c = source.readByte();
            if (!b || !c) {
                return std::unexpected{NbtError::UnexpectedEnd};
            }
            remaining -= 2;
            units.push_back(static_cast<std::uint16_t>(((a & 0x0FU) << 12) | ((*b & 0x3FU) << 6) | (*c & 0x3FU)));
        } else {
            return std::unexpected{NbtError::InvalidUtf8};
        }
    }

    std::string out;
    out.reserve(byteLength);
    for (std::size_t i = 0; i < units.size(); ++i) {
        const std::uint16_t unit = units[i];
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < units.size() && units[i + 1] >= 0xDC00 &&
            units[i + 1] <= 0xDFFF) {
            const std::uint32_t code =
                0x10000U + ((static_cast<std::uint32_t>(unit) - 0xD800U) << 10) + (units[i + 1] - 0xDC00U);
            appendUtf8(out, code);
            ++i;
        } else {
            appendUtf8(out, unit);
        }
    }
    return out;
}

} // namespace basalt::wire::nbt::modifiedUtf8
