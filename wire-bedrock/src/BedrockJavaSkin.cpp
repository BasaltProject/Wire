#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/bedrock/BedrockJavaSkin.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/io/Compression.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace basalt::wire::bedrock {
namespace {

constexpr std::string_view kTexturesHost = "https://textures.minecraft.net/";
constexpr std::size_t kMaxSkinBytes = 1U << 20U;

struct Rgba {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;
};

std::uint32_t be32(const std::uint8_t* at) {
    return (static_cast<std::uint32_t>(at[0]) << 24U) | (static_cast<std::uint32_t>(at[1]) << 16U) |
           (static_cast<std::uint32_t>(at[2]) << 8U) | at[3];
}

int paeth(const int left, const int up, const int upLeft) {
    const int estimate = left + up - upLeft;
    const int distLeft = std::abs(estimate - left);
    const int distUp = std::abs(estimate - up);
    const int distUpLeft = std::abs(estimate - upLeft);
    if (distLeft <= distUp && distLeft <= distUpLeft) {
        return left;
    }
    return distUp <= distUpLeft ? up : upLeft;
}

std::optional<Rgba> decodePng(const std::span<const std::uint8_t> file) {
    constexpr std::array<std::uint8_t, 8> kSignature{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (file.size() < kSignature.size() || !std::equal(kSignature.begin(), kSignature.end(), file.begin())) {
        return std::nullopt;
    }
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint8_t depth = 0;
    std::uint8_t colour = 0;
    std::uint8_t interlace = 0;
    std::vector<std::uint8_t> palette;
    std::vector<std::uint8_t> alpha;
    std::vector<std::uint8_t> compressed;
    std::size_t at = kSignature.size();
    while (at + 12 <= file.size()) {
        const std::uint32_t length = be32(&file[at]);
        const std::string_view type{reinterpret_cast<const char*>(&file[at + 4]), 4};
        if (length > file.size() || at + 12 + length > file.size()) {
            return std::nullopt;
        }
        const std::uint8_t* data = &file[at + 8];
        if (type == "IHDR" && length >= 13) {
            width = be32(data);
            height = be32(data + 4);
            depth = data[8];
            colour = data[9];
            interlace = data[12];
        } else if (type == "PLTE") {
            palette.assign(data, data + length);
        } else if (type == "tRNS") {
            alpha.assign(data, data + length);
        } else if (type == "IDAT") {
            compressed.insert(compressed.end(), data, data + length);
        } else if (type == "IEND") {
            break;
        }
        at += 12 + length;
    }
    if (width == 0 || height == 0 || width > 256 || height > 256 || depth != 8 || interlace != 0) {
        return std::nullopt;
    }
    const std::size_t channels = colour == 6 ? 4 : colour == 2 ? 3 : colour == 0 ? 1 : colour == 4 ? 2 : 1;
    if (colour != 6 && colour != 2 && colour != 0 && colour != 4 && colour != 3) {
        return std::nullopt;
    }
    const auto raw = io::zlib::decompressUnknownSize(compressed);
    const std::size_t stride = static_cast<std::size_t>(width) * channels;
    if (!raw.has_value() || raw->size() < (stride + 1) * height) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> rows(stride * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint8_t filter = (*raw)[y * (stride + 1)];
        const std::uint8_t* in = &(*raw)[y * (stride + 1) + 1];
        std::uint8_t* out = &rows[y * stride];
        const std::uint8_t* above = y == 0 ? nullptr : &rows[(y - 1) * stride];
        for (std::size_t x = 0; x < stride; ++x) {
            const int left = x >= channels ? out[x - channels] : 0;
            const int up = above != nullptr ? above[x] : 0;
            const int upLeft = (above != nullptr && x >= channels) ? above[x - channels] : 0;
            int predicted = 0;
            switch (filter) {
            case 1:
                predicted = left;
                break;
            case 2:
                predicted = up;
                break;
            case 3:
                predicted = (left + up) / 2;
                break;
            case 4:
                predicted = paeth(left, up, upLeft);
                break;
            default:
                break;
            }
            out[x] = static_cast<std::uint8_t>(in[x] + predicted);
        }
    }

    Rgba image{.width = width, .height = height, .pixels = std::vector<std::uint8_t>(std::size_t{width} * height * 4)};
    for (std::size_t pixel = 0; pixel < std::size_t{width} * height; ++pixel) {
        const std::uint8_t* from = &rows[pixel * channels];
        std::uint8_t* to = &image.pixels[pixel * 4];
        switch (colour) {
        case 6:
            std::copy_n(from, 4, to);
            break;
        case 2:
            std::copy_n(from, 3, to);
            to[3] = 0xFF;
            break;
        case 0:
        case 4:
            to[0] = to[1] = to[2] = from[0];
            to[3] = colour == 4 ? from[1] : 0xFF;
            break;
        default: {
            const std::size_t index = from[0];
            if (index * 3 + 2 >= palette.size()) {
                return std::nullopt;
            }
            std::copy_n(&palette[index * 3], 3, to);
            to[3] = index < alpha.size() ? alpha[index] : 0xFF;
            break;
        }
        }
    }
    return image;
}

std::string_view valueAfter(const std::string_view text, const std::string_view key) {
    const std::size_t found = text.find(key);
    if (found == std::string_view::npos) {
        return {};
    }
    std::size_t start = text.find('"', found + key.size());
    if (start == std::string_view::npos) {
        return {};
    }
    ++start;
    const std::size_t end = text.find('"', start);
    return end == std::string_view::npos ? std::string_view{} : text.substr(start, end - start);
}

std::string humanoidGeometry(const bool slim) {
    const std::string armWidth = slim ? "3" : "4";
    const std::string armY = slim ? "11.5" : "12";
    const std::string armPivotY = slim ? "21.5" : "22";
    const std::string name = slim ? "geometry.humanoid.customSlim" : "geometry.humanoid.custom";
    const auto cube = [](const std::string& origin, const std::string& size, const std::string& uv,
                         const std::string& inflate = {}) {
        return R"("cubes":[{)" + (inflate.empty() ? std::string{} : R"("inflate":)" + inflate + ",") + R"("origin":)" +
               origin + R"(,"size":)" + size + R"(,"uv":)" + uv + "}]";
    };
    const auto bone = [](const std::string& id, const std::string& parent, const std::string& pivot,
                         const std::string& cubes = {}) {
        return R"({"name":")" + id + R"(",)" + (parent.empty() ? std::string{} : R"("parent":")" + parent + R"(",)") +
               (cubes.empty() ? std::string{} : cubes + ",") + R"("pivot":)" + pivot + "}";
    };
    const std::string leftArmOrigin = "[4," + armY + ",-2]";
    const std::string rightArmOrigin = slim ? "[-7,11.5,-2]" : "[-8,12,-2]";
    const std::string armSize = "[" + armWidth + ",12,4]";
    const std::string leftPivot = "[5," + armPivotY + ",0]";
    const std::string rightPivot = "[-5," + armPivotY + ",0]";
    std::string json = R"({"format_version":"1.12.0","minecraft:geometry":[{"description":{"identifier":")" + name +
                       R"(","texture_height":64,"texture_width":64,"visible_bounds_height":2,)"
                       R"("visible_bounds_offset":[0,1,0],"visible_bounds_width":1},"bones":[)";
    const std::array<std::string, 17> bones = {
        bone("root", "", "[0,0,0]"),
        bone("waist", "root", "[0,12,0]"),
        bone("body", "waist", "[0,24,0]", cube("[-4,12,-2]", "[8,12,4]", "[16,16]")),
        bone("jacket", "body", "[0,24,0]", cube("[-4,12,-2]", "[8,12,4]", "[16,32]", "0.25")),
        bone("head", "body", "[0,24,0]", cube("[-4,24,-4]", "[8,8,8]", "[0,0]")),
        bone("hat", "head", "[0,24,0]", cube("[-4,24,-4]", "[8,8,8]", "[32,0]", "0.5")),
        bone("rightLeg", "root", "[-1.9,12,0]", cube("[-3.9,0,-2]", "[4,12,4]", "[0,16]")),
        bone("rightPants", "rightLeg", "[-1.9,12,0]", cube("[-3.9,0,-2]", "[4,12,4]", "[0,32]", "0.25")),
        bone("leftLeg", "root", "[1.9,12,0]", cube("[-0.1,0,-2]", "[4,12,4]", "[16,48]")),
        bone("leftPants", "leftLeg", "[1.9,12,0]", cube("[-0.1,0,-2]", "[4,12,4]", "[0,48]", "0.25")),
        bone("leftArm", "body", leftPivot, cube(leftArmOrigin, armSize, "[32,48]")),
        bone("leftSleeve", "leftArm", leftPivot, cube(leftArmOrigin, armSize, "[48,48]", "0.25")),
        bone("leftItem", "leftArm", "[6,14.5,1]"),
        bone("rightArm", "body", rightPivot, cube(rightArmOrigin, armSize, "[40,16]")),
        bone("rightSleeve", "rightArm", rightPivot, cube(rightArmOrigin, armSize, "[40,32]", "0.25")),
        bone("rightItem", "rightArm", "[-6,14.5,1]"),
        bone("cape", "body", "[0,24,-3]"),
    };
    for (std::size_t i = 0; i < bones.size(); ++i) {
        json += (i == 0 ? "" : ",") + bones[i];
    }
    return json + "]}]}";
}

} // namespace

std::optional<SkinInfo> javaSkinOf(const std::span<const protocol::login::GameProfileProperty> properties) {
    const auto textures = std::ranges::find(properties, "textures", &protocol::login::GameProfileProperty::name);
    if (textures == properties.end()) {
        return std::nullopt;
    }
    const auto decoded = common::base64::decode(textures->value);
    if (!decoded.has_value()) {
        return std::nullopt;
    }
    const std::string_view json{reinterpret_cast<const char*>(decoded->data()), decoded->size()};
    const std::size_t skinAt = json.find("\"SKIN\"");
    if (skinAt == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view skinJson = json.substr(skinAt);
    std::string url{valueAfter(skinJson, "\"url\"")};
    // The profile names the texture over plain http, and the same address answers over https.
    if (url.starts_with("http://textures.minecraft.net/")) {
        url.insert(4, "s");
    }
    if (!url.starts_with(kTexturesHost)) {
        return std::nullopt;
    }
    const bool slim = skinJson.find("\"slim\"") != std::string_view::npos;

    const auth::BoundedHttpsOptions options{.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5)};
    const auto response = auth::httpsGetBounded(url, kMaxSkinBytes, options);
    if (!response.has_value() || !response->ok()) {
        return std::nullopt;
    }
    const std::optional<Rgba> image = decodePng(response->body);
    if (!image.has_value() || image->width != 64 || (image->height != 64 && image->height != 32)) {
        return std::nullopt;
    }

    SkinInfo skin;
    skin.skinId = "basalt-java-" + std::string{url.substr(kTexturesHost.size())};
    skin.overridesAppearance = true;
    skin.geometry = humanoidGeometry(slim);
    skin.width = 64;
    skin.height = 64;
    skin.image.assign(std::size_t{64} * 64 * 4, 0);
    std::ranges::copy(image->pixels, skin.image.begin());
    skin.resourcePatch = slim ? R"({"geometry":{"default":"geometry.humanoid.customSlim"}})"
                              : R"({"geometry":{"default":"geometry.humanoid.custom"}})";
    return skin;
}

void SkinCache::remember(const common::Uuid& who, SkinInfo skin) {
    const std::lock_guard<std::mutex> guard{lock_};
    skins_.insert_or_assign(who, std::move(skin));
}

void SkinCache::forget(const common::Uuid& who) {
    const std::lock_guard<std::mutex> guard{lock_};
    skins_.erase(who);
}

bool SkinCache::fetchJava(const std::shared_ptr<SkinCache>& self, const common::Uuid& who,
                          std::vector<protocol::login::GameProfileProperty> properties, const std::size_t maxInFlight) {
    {
        const std::lock_guard<std::mutex> guard{self->lock_};
        if (self->skins_.contains(who)) {
            return false;
        }
        if (const auto known = self->fetching_.find(who); known != self->fetching_.end()) {
            return known->second;
        }
        if (self->inFlight_ >= maxInFlight) {
            return false;
        }
        self->fetching_.emplace(who, true);
        ++self->inFlight_;
    }
    std::thread{[self, who, properties = std::move(properties)] {
        std::optional<SkinInfo> skin = javaSkinOf(properties);
        const std::lock_guard<std::mutex> guard{self->lock_};
        if (skin.has_value()) {
            self->skins_.insert_or_assign(who, std::move(*skin));
        }
        self->fetching_.insert_or_assign(who, false);
        --self->inFlight_;
    }}.detach();
    return true;
}

std::optional<SkinInfo> SkinCache::of(const common::Uuid& who) const {
    const std::lock_guard<std::mutex> guard{lock_};
    const auto found = skins_.find(who);
    return found == skins_.end() ? std::nullopt : std::optional{found->second};
}

} // namespace basalt::wire::bedrock
