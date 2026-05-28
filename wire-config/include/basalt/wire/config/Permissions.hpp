#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::config {

class PermissionRule {
public:
    PermissionRule(std::string pattern, bool granted);

    [[nodiscard]] static PermissionRule parse(std::string_view raw);

    [[nodiscard]] const std::string& pattern() const noexcept { return pattern_; }

    [[nodiscard]] bool granted() const noexcept { return granted_; }

    [[nodiscard]] std::int32_t specificity() const noexcept { return specificity_; }

    [[nodiscard]] bool matches(std::string_view node) const;

    friend bool operator==(const PermissionRule&, const PermissionRule&) = default;

private:
    std::string pattern_;
    bool granted_ = true;
    std::string prefix_;
    bool isWildcard_ = false;
    std::int32_t specificity_ = 0;
};

struct PermissionGroup {
    std::string name;
    std::vector<PermissionRule> rules;
    std::optional<std::string> inherits;
    bool isDefault = false;
    bool isOperator = false;
};

/// The groups of a groups.toml: dotted permissions, `*` wildcards, `-` denials and inheritance.
class GroupsConfig {
public:
    static constexpr std::string_view kFileName = "groups.toml";

    GroupsConfig() = default;

    GroupsConfig(std::vector<PermissionGroup> groups, std::size_t defaultIndex, std::size_t operatorIndex)
        : groups_(std::move(groups)), defaultIndex_(defaultIndex), operatorIndex_(operatorIndex) {}

    [[nodiscard]] static std::expected<GroupsConfig, std::string>
    parse(const common::toml::TomlTable& root, std::string_view path,
          const common::Logger& logger = common::Logger{"Config"});

    [[nodiscard]] std::vector<std::string> names() const;

    [[nodiscard]] bool contains(std::string_view name) const { return find(name) != nullptr; }

    [[nodiscard]] const PermissionGroup& defaultGroup() const noexcept { return at(defaultIndex_); }

    [[nodiscard]] const PermissionGroup& operatorGroup() const noexcept { return at(operatorIndex_); }

    [[nodiscard]] const PermissionGroup& get(std::optional<std::string_view> name) const;

    [[nodiscard]] bool resolve(std::optional<std::string_view> groupName, std::string_view node) const;

private:
    [[nodiscard]] const PermissionGroup& at(std::size_t index) const noexcept;

    [[nodiscard]] const PermissionGroup* find(std::string_view name) const;

    std::vector<PermissionGroup> groups_;
    std::size_t defaultIndex_ = 0;
    std::size_t operatorIndex_ = 0;
};

} // namespace basalt::wire::config
