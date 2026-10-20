#include <basalt/wire/bedrock/BedrockBanners.hpp>
#include <basalt/wire/bedrock/BedrockJavaBlockEntities.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::bedrock {
namespace {

constexpr std::int32_t kBedFirst = 2030;
constexpr std::int32_t kBannerFirst = 14774;
constexpr std::int32_t kWallBannerFirst = 15030;
constexpr std::int32_t kDyes = 16;
constexpr std::int32_t kBedStates = 16;
constexpr std::int32_t kBannerStates = 16;
constexpr std::int32_t kWallBannerStates = 4;
constexpr std::int32_t kLastDye = 15;

[[nodiscard]] std::optional<std::uint8_t> dyeOf(const std::int32_t state, const std::int32_t first,
                                                const std::int32_t perDye) {
    const std::int32_t at = state - first;
    if (at < 0 || at >= kDyes * perDye) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(at / perDye);
}

[[nodiscard]] bool isOminous(const nbt::NbtList& layers) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 8> kOminous{{
        {"rhombus", "cyan"},
        {"stripe_bottom", "light_gray"},
        {"stripe_center", "gray"},
        {"border", "light_gray"},
        {"stripe_middle", "black"},
        {"half_horizontal", "light_gray"},
        {"circle", "light_gray"},
        {"border", "black"},
    }};
    if (layers.size() != kOminous.size()) {
        return false;
    }
    for (std::size_t at = 0; at < kOminous.size(); ++at) {
        const nbt::NbtCompound* layer = layers[at].get<nbt::NbtCompound>();
        if (layer == nullptr) {
            return false;
        }
        std::string_view pattern = layer->getString("pattern");
        pattern.remove_prefix(pattern.starts_with("minecraft:") ? std::string_view{"minecraft:"}.size() : 0);
        if (pattern != kOminous[at].first || layer->getString("color") != kOminous[at].second) {
            return false;
        }
    }
    return true;
}

void writeBanner(io::ByteBuffer& tags, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                 const std::uint8_t colour, const nbt::NbtCompound& data) {
    std::vector<BedrockBannerLayer> layers;
    const nbt::NbtList* saved = data.getList("patterns");
    const bool ominous = saved != nullptr && isOminous(*saved);
    if (saved != nullptr && !ominous) {
        for (const nbt::NbtTag& entry : *saved) {
            const nbt::NbtCompound* layer = entry.get<nbt::NbtCompound>();
            if (layer == nullptr) {
                continue;
            }
            layers.push_back({.pattern = bedrockBannerPattern(layer->getString("pattern")),
                              .colour = kLastDye - dyeIndex(layer->getString("color"))});
        }
    }
    writeBannerTag(tags, x, y, z, kLastDye - static_cast<std::int32_t>(colour), layers, ominous,
                   data.getString("CustomName"));
}

[[nodiscard]] std::string plainLine(const nbt::NbtTag& line) {
    const std::string* raw = line.get<std::string>();
    if (raw != nullptr && !raw->empty() && (raw->front() == '{' || raw->front() == '[' || raw->front() == '"')) {
        const std::expected<common::JsonValue, common::JsonError> parsed = common::parseJson(*raw);
        if (parsed.has_value()) {
            return text::toPlainText(text::componentJson::deserialize(*parsed));
        }
    }
    return text::toPlainText(text::componentNbt::deserialize(line));
}

[[nodiscard]] std::string plainSide(const nbt::NbtCompound* side) {
    const nbt::NbtList* messages = side == nullptr ? nullptr : side->getList("messages");
    if (messages == nullptr) {
        return {};
    }
    std::string text;
    bool first = true;
    for (const nbt::NbtTag& line : *messages) {
        if (!first) {
            text += '\n';
        }
        first = false;
        text += plainLine(line);
    }
    return text;
}

} // namespace

std::optional<JavaSignText> readJavaSign(const nbt::NbtCompound& data) {
    return JavaSignText{.front = plainSide(data.getCompound("front_text")),
                        .back = plainSide(data.getCompound("back_text")),
                        .waxed = data.getByte("is_waxed") != 0};
}

std::optional<std::uint8_t> javaBedColour(const std::int32_t state) { return dyeOf(state, kBedFirst, kBedStates); }

std::optional<std::uint8_t> javaBannerColour(const std::int32_t state) {
    if (const auto standing = dyeOf(state, kBannerFirst, kBannerStates); standing) {
        return standing;
    }
    return dyeOf(state, kWallBannerFirst, kWallBannerStates);
}

bool writeJavaBlockEntity(io::ByteBuffer& tags, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                          const std::int32_t javaType, const nbt::NbtCompound& data, const std::int32_t state) {
    if (javaType == kJavaBannerType) {
        const std::optional<std::uint8_t> colour = javaBannerColour(state);
        if (colour) {
            writeBanner(tags, x, y, z, *colour, data);
        }
        return colour.has_value();
    }
    if (javaType != kJavaSignType && javaType != kJavaHangingSignType) {
        return false;
    }
    const std::optional<JavaSignText> sign = readJavaSign(data);
    writeSignTag(tags, x, y, z, sign->front, sign->back, sign->waxed);
    return true;
}

} // namespace basalt::wire::bedrock
