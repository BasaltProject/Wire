#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::common::placeholder {

/// Whether a condition held, or why it could not be decided.
///
/// `Unresolvable` is kept apart from `False` because they mean opposite things to whoever wrote the
/// file: a false condition worked and said no, an unresolvable one never ran, and the operator
/// needs telling.
enum class ConditionOutcome : std::uint8_t { True, False, Unresolvable };

struct ConditionResult {
    ConditionOutcome outcome = ConditionOutcome::False;
    /// Why it could not be decided: an unknown placeholder, one this situation cannot answer, or a
    /// comparison of things that are not numbers.
    std::string reason;

    [[nodiscard]] static ConditionResult of(bool held);
    [[nodiscard]] static ConditionResult unresolvable(std::string reason);

    [[nodiscard]] bool isTrue() const noexcept { return outcome == ConditionOutcome::True; }

    [[nodiscard]] bool isUnresolvable() const noexcept { return outcome == ConditionOutcome::Unresolvable; }

    friend bool operator==(const ConditionResult&, const ConditionResult&) = default;
};

/// What a condition compares its two sides with, or `None` when it has only one side.
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

/// One line of an operator-written condition, e.g. `%basalt_player_protocol% < 735`. Nothing here
/// knows what a protocol version or a player is: it compares whatever the placeholders resolve to,
/// so a new thing to condition on is a registered placeholder, not a parser edit.
///
/// Comparisons are numeric when both sides are numbers and case-insensitive text otherwise, and a
/// condition with no operator is true when it resolves to `true`.
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
