#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/config/Permissions.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace basalt::wire::config {
namespace {

using common::placeholder::lowercaseAscii;
using common::toml::TomlTable;

[[nodiscard]] std::string_view trim(std::string_view value) {
    while (!value.empty() && static_cast<unsigned char>(value.front()) <= ' ') {
        value.remove_prefix(1);
    }
    while (!value.empty() && static_cast<unsigned char>(value.back()) <= ' ') {
        value.remove_suffix(1);
    }
    return value;
}

} // namespace

PermissionRule::PermissionRule(std::string pattern, bool granted) : pattern_(std::move(pattern)), granted_(granted) {
    prefix_ = pattern_;
    isWildcard_ = prefix_.ends_with('*');
    if (isWildcard_) {
        prefix_.pop_back();
    }
    if (prefix_.ends_with('.')) {
        prefix_.pop_back();
    }
    specificity_ = prefix_.empty() ? 0 : static_cast<std::int32_t>(std::ranges::count(prefix_, '.')) + 1;
}

PermissionRule PermissionRule::parse(std::string_view raw) {
    const bool denied = raw.starts_with('-');
    if (denied) {
        raw.remove_prefix(1);
    }
    return PermissionRule{std::string{trim(raw)}, !denied};
}

bool PermissionRule::matches(std::string_view node) const {
    if (!isWildcard_) {
        return node == pattern_;
    }
    // A bare `*` grants everything, and `a.*` grants `a` itself as well as what is under it.
    return prefix_.empty() || node == prefix_ || node.starts_with(prefix_ + ".");
}

std::vector<std::string> GroupsConfig::names() const {
    std::vector<std::string> named;
    named.reserve(groups_.size());
    for (const PermissionGroup& group : groups_) {
        named.push_back(group.name);
    }
    return named;
}

const PermissionGroup& GroupsConfig::at(const std::size_t index) const noexcept {
    static const PermissionGroup kNothing{};
    return index < groups_.size() ? groups_[index] : kNothing;
}

const PermissionGroup* GroupsConfig::find(std::string_view name) const {
    for (const PermissionGroup& group : groups_) {
        if (group.name == name) {
            return &group;
        }
    }
    return nullptr;
}

const PermissionGroup& GroupsConfig::get(std::optional<std::string_view> name) const {
    if (!name.has_value()) {
        return defaultGroup();
    }
    const PermissionGroup* found = find(lowercaseAscii(*name));
    return found == nullptr ? defaultGroup() : *found;
}

bool GroupsConfig::resolve(std::optional<std::string_view> groupName, std::string_view node) const {
    const PermissionRule* best = nullptr;
    std::int32_t bestDepth = std::numeric_limits<std::int32_t>::max();
    std::int32_t depth = 0;
    const PermissionGroup* current = &get(groupName);
    std::vector<std::string_view> seen;

    // `load` refuses a cycle; this guard is for configurations built in code.
    while (current != nullptr && std::ranges::find(seen, std::string_view{current->name}) == seen.end()) {
        seen.emplace_back(current->name);
        for (const PermissionRule& rule : current->rules) {
            if (!rule.matches(node)) {
                continue;
            }
            // A group is read before what it inherits, so at equal specificity the one found first is nearer.
            const bool better = best == nullptr || rule.specificity() > best->specificity() ||
                                (rule.specificity() == best->specificity() && depth < bestDepth);
            if (better) {
                best = &rule;
                bestDepth = depth;
            }
        }
        current = current->inherits.has_value() ? find(*current->inherits) : nullptr;
        ++depth;
    }
    return best != nullptr && best->granted();
}

std::expected<GroupsConfig, std::string> GroupsConfig::parse(const TomlTable& root, std::string_view pathName,
                                                             const common::Logger& logger) {
    const std::string path{pathName};
    const TomlTable* declared = root.table("groups");
    if (declared == nullptr) {
        return std::unexpected(path + " declares no [groups] table.");
    }

    std::vector<PermissionGroup> groups;
    for (const std::string& name : declared->keys()) {
        const TomlTable* table = declared->table(name);
        if (table == nullptr) {
            continue;
        }
        std::vector<PermissionRule> rules;
        for (const std::string& written : table->stringList("permissions")) {
            rules.push_back(PermissionRule::parse(written));
        }
        std::optional<std::string> inherits;
        if (const std::optional<std::string_view> parent = table->string("inherits"); parent.has_value()) {
            inherits = lowercaseAscii(*parent);
        }
        groups.push_back(PermissionGroup{.name = lowercaseAscii(name),
                                         .rules = std::move(rules),
                                         .inherits = std::move(inherits),
                                         .isDefault = table->boolean("default").value_or(false),
                                         .isOperator = table->boolean("operator").value_or(false)});
    }
    if (groups.empty()) {
        return std::unexpected(path + " declares no groups.");
    }

    const auto byName = [&groups](std::string_view name) -> const PermissionGroup* {
        for (const PermissionGroup& group : groups) {
            if (group.name == name) {
                return &group;
            }
        }
        return nullptr;
    };
    for (const PermissionGroup& group : groups) {
        if (!group.inherits.has_value()) {
            continue;
        }
        if (byName(*group.inherits) == nullptr) {
            return std::unexpected("Group '" + group.name + "' inherits from '" + *group.inherits +
                                   "', which does not exist.");
        }
        std::vector<std::string_view> seen{group.name};
        const PermissionGroup* current = byName(*group.inherits);
        while (current != nullptr) {
            if (std::ranges::find(seen, std::string_view{current->name}) != seen.end()) {
                return std::unexpected("Group inheritance in " + path + " forms a cycle through '" + current->name +
                                       "'.");
            }
            seen.emplace_back(current->name);
            current = current->inherits.has_value() ? byName(*current->inherits) : nullptr;
        }
    }

    // Exactly one of each: a file with two default or op groups is one whose author meant something unguessable.
    const auto countOf = [&groups](bool PermissionGroup::* flag) {
        std::size_t found = 0;
        std::size_t at = 0;
        for (std::size_t index = 0; index < groups.size(); ++index) {
            if (groups[index].*flag) {
                ++found;
                at = index;
            }
        }
        return std::pair<std::size_t, std::size_t>{found, at};
    };
    const auto [defaults, defaultAt] = countOf(&PermissionGroup::isDefault);
    if (defaults != 1) {
        return std::unexpected(path + " must mark exactly one group as `default`, found " + std::to_string(defaults) +
                               ".");
    }
    const auto [operators, operatorAt] = countOf(&PermissionGroup::isOperator);
    if (operators != 1) {
        return std::unexpected(path + " must mark exactly one group as `operator`, found " + std::to_string(operators) +
                               ".");
    }

    logger.debug([&groups, &path] {
        return "Loaded " + std::to_string(groups.size()) + " permission group(s) from " + path + ".";
    });
    return GroupsConfig{std::move(groups), defaultAt, operatorAt};
}

} // namespace basalt::wire::config
