#include <basalt/wire/common/placeholder/PlaceholderCondition.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

namespace basalt::wire::common::placeholder {
namespace {

[[nodiscard]] std::string_view trim(std::string_view value) {
    while (!value.empty() && static_cast<unsigned char>(value.front()) <= ' ') {
        value.remove_prefix(1);
    }
    while (!value.empty() && static_cast<unsigned char>(value.back()) <= ' ') {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] bool isLetterOrDigit(char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z');
}

/// The number a side of a comparison is, or nothing when it is text.
///
/// Only finite numbers count: an infinity or a NaN compares false against everything including
/// itself, which is not something an operator reading the file would expect, so those stay text.
[[nodiscard]] std::optional<double> asNumber(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    std::string_view digits = value;
    if (digits.front() == '+') {
        digits.remove_prefix(1);
    }
    double parsed = 0.0;
    const std::from_chars_result read = std::from_chars(digits.data(), digits.data() + digits.size(), parsed);
    // The end has to be the end: `from_chars` stops at the first character it cannot use and calls
    // that a success, so "12abc" would otherwise be twelve.
    if (read.ec != std::errc{} || read.ptr != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return std::isfinite(parsed) ? std::optional<double>{parsed} : std::nullopt;
}

[[nodiscard]] bool equalsIgnoreCase(std::string_view left, std::string_view right) {
    return lowercaseAscii(left) == lowercaseAscii(right);
}

[[nodiscard]] bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
    return lowercaseAscii(haystack).find(lowercaseAscii(needle)) != std::string::npos;
}

} // namespace

ConditionResult ConditionResult::of(bool held) {
    return ConditionResult{.outcome = held ? ConditionOutcome::True : ConditionOutcome::False, .reason = {}};
}

ConditionResult ConditionResult::unresolvable(std::string reason) {
    return ConditionResult{.outcome = ConditionOutcome::Unresolvable, .reason = std::move(reason)};
}

namespace {

using Operator = PlaceholderConditionOperator;

struct OperatorSpelling {
    Operator op;
    std::string_view token;
    bool wordy;
};

/// Longest token first, so `<=` is not read as `<` followed by a stray `=`.
constexpr std::array<OperatorSpelling, 9> kOperators{{
    {Operator::NotEqual, "!=", false},
    {Operator::LessOrEqual, "<=", false},
    {Operator::GreaterOrEqual, ">=", false},
    {Operator::EqualStrict, "==", false},
    {Operator::Equal, "=", false},
    {Operator::Less, "<", false},
    {Operator::Greater, ">", false},
    {Operator::Matches, "matches", true},
    {Operator::Contains, "contains", true},
}};

[[nodiscard]] std::string_view tokenOf(Operator op) {
    for (const OperatorSpelling& spelling : kOperators) {
        if (spelling.op == op) {
            return spelling.token;
        }
    }
    return {};
}

/// A word operator has to be a word: "matches" inside "rematches" is not one.
[[nodiscard]] bool isWord(std::string_view text, std::size_t start, std::size_t length) {
    const bool freeBefore = start == 0 || !isLetterOrDigit(text[start - 1]);
    const bool freeAfter = start + length >= text.size() || !isLetterOrDigit(text[start + length]);
    return freeBefore && freeAfter;
}

/// The first operator outside a `%...%`.
///
/// Outside, because a placeholder's argument is free text and can contain anything: the separator
/// in `%basalt_server_countdown_dd.MM.yyyy_01.01.2020%` must never be read as an operator.
[[nodiscard]] std::optional<std::pair<Operator, std::size_t>> findOperator(std::string_view text) {
    bool inPlaceholder = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '%') {
            inPlaceholder = !inPlaceholder;
            continue;
        }
        if (inPlaceholder) {
            continue;
        }
        for (const OperatorSpelling& spelling : kOperators) {
            if (!text.substr(index).starts_with(spelling.token)) {
                continue;
            }
            if (spelling.wordy && !isWord(text, index, spelling.token.size())) {
                continue;
            }
            return std::pair<Operator, std::size_t>{spelling.op, index};
        }
    }
    return std::nullopt;
}

[[nodiscard]] ConditionResult apply(Operator op, const std::string& left, const std::string& right) {
    if (op == Operator::Matches) {
        return ConditionResult::of(globMatches(right, left));
    }
    if (op == Operator::Contains) {
        return ConditionResult::of(containsIgnoreCase(left, right));
    }

    const std::optional<double> leftNumber = asNumber(left);
    const std::optional<double> rightNumber = asNumber(right);
    if (op == Operator::Equal || op == Operator::EqualStrict || op == Operator::NotEqual) {
        const bool equal = leftNumber.has_value() && rightNumber.has_value() ? *leftNumber == *rightNumber
                                                                             : equalsIgnoreCase(left, right);
        return ConditionResult::of(op == Operator::NotEqual ? !equal : equal);
    }
    if (!leftNumber.has_value() || !rightNumber.has_value()) {
        const std::string& offender = leftNumber.has_value() ? right : left;
        return ConditionResult::unresolvable("'" + offender + "' is not a number, so '" + std::string{tokenOf(op)} +
                                             "' cannot compare it");
    }
    switch (op) {
    case Operator::Less:
        return ConditionResult::of(*leftNumber < *rightNumber);
    case Operator::LessOrEqual:
        return ConditionResult::of(*leftNumber <= *rightNumber);
    case Operator::Greater:
        return ConditionResult::of(*leftNumber > *rightNumber);
    case Operator::GreaterOrEqual:
    default:
        return ConditionResult::of(*leftNumber >= *rightNumber);
    }
}

} // namespace

PlaceholderCondition::PlaceholderCondition(std::string raw) : raw_(std::move(raw)) {
    const std::string_view trimmed = trim(raw_);
    const std::optional<std::pair<Operator, std::size_t>> found = findOperator(trimmed);
    if (!found.has_value()) {
        left_ = std::string{trimmed};
        operator_ = Operator::None;
        return;
    }
    const auto& [op, at] = *found;
    left_ = std::string{trim(trimmed.substr(0, at))};
    operator_ = op;
    right_ = std::string{trim(trimmed.substr(at + tokenOf(op).size()))};
}

ConditionResult PlaceholderCondition::evaluate(const PlaceholderRegistry& registry,
                                               const PlaceholderContext& context) const {
    std::optional<std::string> failure;
    const UnresolvedHandler report = [&failure](std::string_view name,
                                                const PlaceholderValue& value) -> std::optional<std::string> {
        if (!failure.has_value()) {
            const std::string named = "%" + std::string{name} + "%";
            if (value.kind == PlaceholderKind::Unavailable && value.reason.has_value()) {
                failure = named + " cannot be used here: " + *value.reason;
            } else if (value.kind == PlaceholderKind::Unknown) {
                failure = named + " is not a placeholder this server knows";
            } else {
                failure = named + " cannot be answered here";
            }
        }
        return std::nullopt;
    };

    const std::string leftText = substitute(left_, registry, context, report);
    const std::string rightText = substitute(right_, registry, context, report);
    if (failure.has_value()) {
        return ConditionResult::unresolvable(*failure);
    }

    if (operator_ == Operator::None) {
        return ConditionResult::of(equalsIgnoreCase(leftText, "true"));
    }
    return apply(operator_, leftText, rightText);
}

} // namespace basalt::wire::common::placeholder
