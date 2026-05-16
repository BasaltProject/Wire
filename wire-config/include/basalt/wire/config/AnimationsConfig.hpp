#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/config/ConfigDirectory.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::config {

struct Animation {
    std::string name;
    std::vector<std::string> frames;
    std::int64_t changeIntervalMillis = 200;

    [[nodiscard]] const std::string& frameAt(std::int64_t epochMillis) const;
};

class AnimationsConfig {
public:
    static constexpr std::string_view kFileName = "animations.toml";

    static constexpr std::int64_t kMinimumIntervalMillis = 1;

    explicit AnimationsConfig(std::vector<Animation> animations = {}) : animations_(std::move(animations)) {}

    [[nodiscard]] static std::expected<AnimationsConfig, std::string>
    load(const ConfigDirectory& directory, std::string_view defaultContents,
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] std::vector<std::string> names() const;

    [[nodiscard]] const Animation* get(std::string_view name) const;

private:
    std::vector<Animation> animations_;
};

} // namespace basalt::wire::config
