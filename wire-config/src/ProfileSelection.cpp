#include <basalt/wire/config/ProfileSelection.hpp>

#include <cstdint>
#include <utility>

namespace basalt::wire::config {

using wire::common::placeholder::ConditionOutcome;
using wire::common::placeholder::ConditionResult;
using wire::common::placeholder::PlaceholderCondition;
using wire::common::toml::TomlTable;

ProfileHeader ProfileHeader::from(std::string name, const TomlTable& table) {
    std::vector<PlaceholderCondition> conditions;
    for (std::string& written : table.stringList("when")) {
        conditions.emplace_back(std::move(written));
    }
    return ProfileHeader{.name = std::move(name),
                         .priority = static_cast<std::int32_t>(table.integer("priority").value_or(0)),
                         .isDefault = table.boolean("default").value_or(false),
                         .isActive = table.boolean("active").value_or(false),
                         .conditions = std::move(conditions)};
}

void OnceReporter::report(const wire::common::Logger& logger, const std::string& message) {
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        if (!said_.insert(message).second) {
            return;
        }
    }
    logger.warn([&message] { return message; });
}

bool profileApplies(const ProfileHeader& header, const wire::common::placeholder::PlaceholderRegistry& registry,
                    const wire::common::placeholder::PlaceholderContext& context, const wire::common::Logger& logger,
                    OnceReporter& reporter, std::string_view what) {
    if (header.conditions.empty()) {
        return false;
    }
    for (const PlaceholderCondition& condition : header.conditions) {
        const ConditionResult result = condition.evaluate(registry, context);
        if (result.outcome == ConditionOutcome::True) {
            continue;
        }
        if (result.outcome == ConditionOutcome::Unresolvable) {
            reporter.report(logger, "Condition '" + condition.raw() + "' of " + std::string{what} + " profile '" +
                                        header.name + "' was skipped: " + result.reason + ".");
        }
        return false;
    }
    return true;
}

std::optional<std::vector<std::string>> linesOf(const TomlTable& table, std::string_view key) {
    std::vector<std::string> lines = table.stringList(key);
    if (!lines.empty()) {
        return lines;
    }
    const std::optional<std::string_view> single = table.string(key);
    if (!single.has_value()) {
        return std::nullopt;
    }
    std::string_view text = *single;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

} // namespace basalt::wire::config
