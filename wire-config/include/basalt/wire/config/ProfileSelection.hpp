#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/PlaceholderCondition.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::config {

struct ProfileHeader {
    std::string name;
    std::int32_t priority = 0;
    bool isDefault = false;
    bool isActive = false;
    std::vector<wire::common::placeholder::PlaceholderCondition> conditions;

    [[nodiscard]] static ProfileHeader from(std::string name, const wire::common::toml::TomlTable& table);
};

template <typename T>
concept ConditionalProfile = requires(const T& profile) {
    { profile.header } -> std::convertible_to<const ProfileHeader&>;
};

class OnceReporter {
public:
    void report(const wire::common::Logger& logger, const std::string& message);

private:
    mutable std::mutex mutex_;
    std::set<std::string, std::less<>> said_;
};

[[nodiscard]] bool profileApplies(const ProfileHeader& header,
                                  const wire::common::placeholder::PlaceholderRegistry& registry,
                                  const wire::common::placeholder::PlaceholderContext& context,
                                  const wire::common::Logger& logger, OnceReporter& reporter, std::string_view what);

template <ConditionalProfile T>
[[nodiscard]] const T& selectProfile(const std::vector<T>& profiles, const T& fallback,
                                     const wire::common::placeholder::PlaceholderRegistry& registry,
                                     const wire::common::placeholder::PlaceholderContext& context,
                                     const wire::common::Logger& logger, OnceReporter& reporter,
                                     std::string_view what) {
    const T* best = nullptr;
    for (const T& profile : profiles) {
        if (!profile.header.isActive && !profileApplies(profile.header, registry, context, logger, reporter, what)) {
            continue;
        }
        if (best == nullptr || profile.header.priority > best->header.priority) {
            best = &profile;
        }
    }
    return best == nullptr ? fallback : *best;
}

template <typename Build>
[[nodiscard]] auto parseProfiles(const wire::common::toml::TomlTable& root, const std::string& path, Build&& build)
    -> std::expected<std::pair<std::vector<decltype(build(ProfileHeader{}, root))>, std::size_t>, std::string> {
    using Profile = decltype(build(ProfileHeader{}, root));

    const wire::common::toml::TomlTable* declared = root.table("profiles");
    if (declared == nullptr) {
        return std::unexpected(path + " declares no [profiles] table.");
    }
    std::vector<Profile> profiles;
    for (const std::string& name : declared->keys()) {
        const wire::common::toml::TomlTable* table = declared->table(name);
        if (table == nullptr) {
            continue;
        }
        profiles.push_back(build(ProfileHeader::from(name, *table), *table));
    }
    if (profiles.empty()) {
        return std::unexpected(path + " declares no profiles.");
    }

    std::size_t defaults = 0;
    std::size_t defaultAt = 0;
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        if (profiles[index].header.isDefault) {
            ++defaults;
            defaultAt = index;
        }
    }
    if (defaults != 1) {
        return std::unexpected(path + " must mark exactly one profile as `default`, found " + std::to_string(defaults) +
                               ".");
    }
    return std::pair<std::vector<Profile>, std::size_t>{std::move(profiles), defaultAt};
}

[[nodiscard]] std::optional<std::vector<std::string>> linesOf(const wire::common::toml::TomlTable& table,
                                                              std::string_view key);

} // namespace basalt::wire::config
