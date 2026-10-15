#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::common::placeholder {

/// Unresolvable is not False: a false condition said no, an unresolvable one never ran.
enum class ConditionOutcome : std::uint8_t { True, False, Unresolvable };

struct ConditionResult {
    ConditionOutcome outcome = ConditionOutcome::False;
    std::string reason;

    [[nodiscard]] static ConditionResult of(bool held);
    [[nodiscard]] static ConditionResult unresolvable(std::string reason);

    [[nodiscard]] bool isTrue() const noexcept { return outcome == ConditionOutcome::True; }

    [[nodiscard]] bool isUnresolvable() const noexcept { return outcome == ConditionOutcome::Unresolvable; }

    friend bool operator==(const ConditionResult&, const ConditionResult&) = default;
};

enum class PlaceholderConditionOperator : std::uint8_t {
    None,
    NotEqual,
    LessOrEqual,
    GreaterOrEqual,
    EqualStrict,
    Equal,
    Less,
    Greater,
    Matches,
    Contains,
};

class PlaceholderCondition {
public:
    explicit PlaceholderCondition(std::string raw);

    [[nodiscard]] const std::string& raw() const noexcept { return raw_; }

    [[nodiscard]] ConditionResult evaluate(const PlaceholderRegistry& registry,
                                           const PlaceholderContext& context = PlaceholderContext{}) const;

private:
    std::string raw_;
    std::string left_;
    PlaceholderConditionOperator operator_ = PlaceholderConditionOperator::None;
    std::string right_;
};

} // namespace basalt::wire::common::placeholder
