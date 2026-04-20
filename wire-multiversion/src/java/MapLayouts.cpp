#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/MapLayouts.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <limits>
#include <optional>
#include <vector>

namespace basalt::wire::multiversion::java::mapLayouts {
namespace {

namespace play = protocol::play;
namespace versions = protocolVersions;

/// The base colours as RGB, by map colour id: `MapColor`'s table, which only ever grew at the end.
constexpr std::array<std::uint32_t, 62> kBaseColors{
    0,        8368696, 16247203, 13092807, 16711680, 10526975, 10987431, 31744,    16777215, 10791096, 9923917,
    7368816,  4210943, 9402184,  16776437, 14188339, 11685080, 6724056,  15066419, 8375321,  15892389, 5000268,
    10066329, 5013401, 8339378,  3361970,  6704179,  6717235,  10040115, 1644825,  16445005, 6085589,  4882687,
    55610,    8476209, 7340544,  13742497, 10441252, 9787244,  7367818,  12223780, 6780213,  10505550, 3746083,
    8874850,  5725276, 8014168,  4996700,  4993571,  5001770,  9321518,  2430480,  12398641, 9715553,  6035741,
    1474182,  3837580, 5647422,  1356933,  6579300,  14200723, 8365974,
};

constexpr std::array<std::uint32_t, 4> kModifiers{180, 220, 255, 135};

constexpr std::int32_t kFirstNew = 35;
constexpr std::int32_t kRedX = 26;
constexpr std::int32_t kTrialChambers = 34;
constexpr std::int32_t kJungleTemple = 32;
constexpr std::int32_t kFirstVillage = 27;
constexpr std::int32_t kRedMarker = 2;
constexpr std::int32_t kLastBefore113 = 9;

[[nodiscard]] std::size_t paletteOf(std::int32_t farEnd) {
    if (farEnd < versions::kV1_12) {
        return 36;
    }
    if (farEnd < versions::kV1_16) {
        return 52;
    }
    if (farEnd < versions::kV1_17) {
        return 59;
    }
    return kBaseColors.size();
}

[[nodiscard]] std::array<std::uint32_t, 3> shaded(std::uint8_t packed) {
    const std::uint32_t rgb = kBaseColors[packed >> 2U];
    const std::uint32_t modifier = kModifiers[packed & 3U];
    return {((rgb >> 16U) & 0xFFU) * modifier / 255U, ((rgb >> 8U) & 0xFFU) * modifier / 255U,
            (rgb & 0xFFU) * modifier / 255U};
}

[[nodiscard]] std::array<std::uint8_t, 256> nearestTable(std::size_t palette) {
    std::array<std::uint8_t, 256> table{};
    for (std::size_t packed = 0; packed < table.size(); ++packed) {
        table[packed] = static_cast<std::uint8_t>(packed);
        if ((packed >> 2U) < palette || (packed >> 2U) >= kBaseColors.size()) {
            continue;
        }
        const std::array<std::uint32_t, 3> wanted = shaded(static_cast<std::uint8_t>(packed));
        std::int64_t best = std::numeric_limits<std::int64_t>::max();
        for (std::size_t candidate = 4; candidate < palette * 4; ++candidate) {
            const std::array<std::uint32_t, 3> have = shaded(static_cast<std::uint8_t>(candidate));
            std::int64_t distance = 0;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const std::int64_t delta =
                    static_cast<std::int64_t>(wanted[channel]) - static_cast<std::int64_t>(have[channel]);
                distance += delta * delta;
            }
            if (distance < best) {
                best = distance;
                table[packed] = static_cast<std::uint8_t>(candidate);
            }
        }
    }
    return table;
}

[[nodiscard]] std::optional<std::int32_t> typeFor(std::int32_t type, std::int32_t farEnd) {
    if (farEnd >= versions::kNative) {
        return type;
    }
    if (type >= kFirstNew) {
        type = kRedX;
    }
    if (farEnd < versions::kV1_20_5 && type == kTrialChambers) {
        type = kJungleTemple;
    }
    if (farEnd < versions::kV1_20_2 && type >= kFirstVillage) {
        type = kRedMarker;
    }
    if (farEnd < versions::kV1_13 && type > kLastBefore113) {
        return std::nullopt;
    }
    return type;
}

void writeName(io::ByteBuffer& output, const std::optional<text::Component>& name, std::int32_t farEnd) {
    protocol::writeBoolean(output, name.has_value());
    if (!name.has_value()) {
        return;
    }
    if (farEnd >= versions::kV1_20_3) {
        protocol::writeAnonymousNbt(output, text::componentNbt::serialize(*name));
    } else {
        protocol::writeString(output, common::writeJson(text::componentJson::serialize(*name)));
    }
}

void writeDecorations(io::ByteBuffer& output, const std::vector<play::MapDecorationEntry>& decorations,
                      std::int32_t farEnd) {
    std::vector<std::pair<std::int32_t, const play::MapDecorationEntry*>> kept;
    for (const play::MapDecorationEntry& decoration : decorations) {
        if (const std::optional<std::int32_t> type = typeFor(decoration.type, farEnd); type.has_value()) {
            kept.emplace_back(*type, &decoration);
        }
    }
    protocol::varInt::write(output, static_cast<std::int32_t>(kept.size()));
    for (const auto& [type, decoration] : kept) {
        if (farEnd < versions::kV1_13) {
            // Before 1.13 the type and the rotation share one byte.
            output.writeByte(static_cast<std::uint8_t>((type << 4) | (decoration->rotation & 15)));
            output.writeByte(static_cast<std::uint8_t>(decoration->x));
            output.writeByte(static_cast<std::uint8_t>(decoration->y));
            continue;
        }
        protocol::varInt::write(output, type);
        output.writeByte(static_cast<std::uint8_t>(decoration->x));
        output.writeByte(static_cast<std::uint8_t>(decoration->y));
        output.writeByte(static_cast<std::uint8_t>(decoration->rotation));
        writeName(output, decoration->name, farEnd);
    }
}

} // namespace

std::uint8_t colorFor(std::uint8_t packed, std::int32_t farEnd) {
    static const std::array<std::uint8_t, 256> kBefore112 = nearestTable(36);
    static const std::array<std::uint8_t, 256> kBefore116 = nearestTable(52);
    static const std::array<std::uint8_t, 256> kBefore117 = nearestTable(59);
    switch (paletteOf(farEnd)) {
    case 36:
        return kBefore112[packed];
    case 52:
        return kBefore116[packed];
    case 59:
        return kBefore117[packed];
    default:
        return packed;
    }
}

void write(io::ByteBuffer& output, const play::MapItemDataPacket& packet, std::int32_t farEnd) {
    protocol::varInt::write(output, packet.mapId);
    output.writeByte(static_cast<std::uint8_t>(packet.scale));
    if (farEnd >= versions::kV1_9 && farEnd < versions::kV1_17) {
        protocol::writeBoolean(output, true);
    }
    if (farEnd >= versions::kV1_14) {
        protocol::writeBoolean(output, packet.locked);
    }
    if (farEnd >= versions::kV1_17) {
        protocol::writeBoolean(output, packet.decorations.has_value());
        if (packet.decorations.has_value()) {
            writeDecorations(output, *packet.decorations, farEnd);
        }
    } else {
        writeDecorations(output, packet.decorations.value_or(std::vector<play::MapDecorationEntry>{}), farEnd);
    }
    if (!packet.patch.has_value() || packet.patch->width == 0) {
        output.writeByte(0);
        return;
    }
    output.writeByte(packet.patch->width);
    output.writeByte(packet.patch->height);
    output.writeByte(packet.patch->startX);
    output.writeByte(packet.patch->startY);
    std::vector<std::uint8_t> colors = packet.patch->colors;
    if (paletteOf(farEnd) < kBaseColors.size()) {
        for (std::uint8_t& color : colors) {
            color = colorFor(color, farEnd);
        }
    }
    protocol::writeByteArray(output, colors);
}

} // namespace basalt::wire::multiversion::java::mapLayouts
