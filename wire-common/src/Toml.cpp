#include <basalt/wire/common/toml/Toml.hpp>

#include <charconv>
#include <cstdint>

namespace basalt::wire::common::toml {
namespace {

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

/// Walks a line, tracking quotes, and hands each character to the visitor along with whether it is
/// inside a string. Every scan in this parser has to ignore what is quoted, and doing it once here
/// is what keeps `#`, `=`, `,` and brackets from being found inside a value.
template <typename Visitor>
void scanOutsideQuotes(std::string_view text, Visitor&& visit) {
    bool inQuotes = false;
    char quote = ' ';
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inQuotes) {
            if (c == quote) {
                inQuotes = false;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            inQuotes = true;
            quote = c;
            continue;
        }
        if (!visit(c, i)) {
            return;
        }
    }
}

std::string_view stripComment(std::string_view line) noexcept {
    std::size_t at = std::string_view::npos;
    scanOutsideQuotes(line, [&at](char c, std::size_t i) {
        if (c == '#') {
            at = i;
            return false;
        }
        return true;
    });
    return at == std::string_view::npos ? line : line.substr(0, at);
}

std::optional<std::size_t> findSeparator(std::string_view line) noexcept {
    std::size_t at = std::string_view::npos;
    scanOutsideQuotes(line, [&at](char c, std::size_t i) {
        if (c == '=') {
            at = i;
            return false;
        }
        return true;
    });
    return at == std::string_view::npos ? std::nullopt : std::optional{at};
}

/// How many brackets are still open, ignoring any inside strings.
int bracketDepth(std::string_view text) noexcept {
    int depth = 0;
    scanOutsideQuotes(text, [&depth](char c, std::size_t) {
        if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
        }
        return true;
    });
    return depth;
}

std::string_view unquoteKey(std::string_view key) noexcept {
    if (key.size() >= 2 && (key.front() == '"' || key.front() == '\'') && key.back() == key.front()) {
        return key.substr(1, key.size() - 2);
    }
    return key;
}

std::expected<std::string, TomlError> parseQuotedString(std::string_view text) {
    const char quote = text.front();
    if (text.size() < 2 || text.back() != quote) {
        return std::unexpected{TomlError::UnterminatedString};
    }
    const std::string_view body = text.substr(1, text.size() - 2);
    if (quote == '\'') {
        // A literal string takes no escapes at all, which is the whole point of it.
        return std::string{body};
    }

    std::string out;
    out.reserve(body.size());
    for (std::size_t i = 0; i < body.size();) {
        if (body[i] == '\\' && i + 1 < body.size()) {
            switch (body[i + 1]) {
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                out += '\r';
                break;
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            default:
                return std::unexpected{TomlError::UnknownEscape};
            }
            i += 2;
        } else {
            out += body[i];
            ++i;
        }
    }
    return out;
}

std::expected<TomlValue, TomlError> parseValue(std::string_view text);

std::vector<std::string_view> splitArrayElements(std::string_view inner) {
    std::vector<std::string_view> elements;
    int depth = 0;
    std::size_t start = 0;
    scanOutsideQuotes(inner, [&](char c, std::size_t i) {
        if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
        } else if (c == ',' && depth == 0) {
            elements.push_back(inner.substr(start, i - start));
            start = i + 1;
        }
        return true;
    });
    elements.push_back(inner.substr(start));
    return elements;
}

std::expected<TomlValue, TomlError> parseArray(std::string_view text) {
    if (!text.ends_with(']')) {
        return std::unexpected{TomlError::UnterminatedArray};
    }
    const std::string_view inner = trim(text.substr(1, text.size() - 2));
    if (inner.empty()) {
        return TomlValue::ofArray({});
    }

    std::vector<TomlValue> values;
    for (const std::string_view raw : splitArrayElements(inner)) {
        const std::string_view element = trim(raw);
        // A blank element is dropped rather than refused: that is TOML's trailing comma, which is
        // how a multi-line list is written so that adding an entry touches only one line.
        if (element.empty()) {
            continue;
        }
        auto value = parseValue(element);
        if (!value) {
            return value;
        }
        values.push_back(std::move(*value));
    }
    return TomlValue::ofArray(std::move(values));
}

std::expected<TomlValue, TomlError> parseNumber(std::string_view text) {
    std::int64_t integer = 0;
    const auto asInteger = std::from_chars(text.data(), text.data() + text.size(), integer);
    if (asInteger.ec == std::errc{} && asInteger.ptr == text.data() + text.size()) {
        return TomlValue::of(integer);
    }

    double real = 0.0;
    const auto asReal = std::from_chars(text.data(), text.data() + text.size(), real);
    if (asReal.ec == std::errc{} && asReal.ptr == text.data() + text.size()) {
        return TomlValue::of(real);
    }
    return std::unexpected{TomlError::NotAValue};
}

std::expected<TomlValue, TomlError> parseValue(std::string_view text) {
    if (text.empty()) {
        return std::unexpected{TomlError::NotAValue};
    }
    if (text.front() == '"' || text.front() == '\'') {
        return parseQuotedString(text).transform([](std::string v) { return TomlValue::of(std::move(v)); });
    }
    if (text.front() == '[') {
        return parseArray(text);
    }
    if (text == "true") {
        return TomlValue::of(true);
    }
    if (text == "false") {
        return TomlValue::of(false);
    }
    return parseNumber(text);
}

std::expected<TomlTable*, TomlError> navigateToTable(TomlTable& root, std::string_view dottedPath) {
    TomlTable* table = &root;
    std::size_t cursor = 0;
    while (cursor <= dottedPath.size()) {
        const std::size_t dot = dottedPath.find('.', cursor);
        const std::size_t end = dot == std::string_view::npos ? dottedPath.size() : dot;
        const std::string_view key = unquoteKey(trim(dottedPath.substr(cursor, end - cursor)));
        if (key.empty())
            return std::unexpected(TomlError::MalformedTableHeader);

        TomlValue* existing = table->find(key);
        if (existing != nullptr && existing->get<TomlTable>() == nullptr)
            return std::unexpected(TomlError::ConflictingTable);
        if (existing == nullptr) {
            table->set(key, TomlValue::ofTable(TomlTable{}));
            existing = table->find(key);
        }
        table = existing->get<TomlTable>();

        if (dot == std::string_view::npos) {
            break;
        }
        cursor = dot + 1;
    }
    return table;
}

std::vector<std::string_view> splitLines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

} // namespace

std::string_view tomlErrorMessage(TomlError error) noexcept {
    switch (error) {
    case TomlError::ArrayOfTablesUnsupported:
        return "arrays of tables are not supported here: write a table per entry instead";
    case TomlError::MalformedTableHeader:
        return "malformed table header";
    case TomlError::MissingSeparator:
        return "expected '=' in a key and value line";
    case TomlError::UnterminatedString:
        return "unterminated string";
    case TomlError::UnterminatedArray:
        return "unterminated array";
    case TomlError::UnknownEscape:
        return "unknown escape in a string";
    case TomlError::NotAValue:
        return "not a valid value";
    case TomlError::DuplicateKey:
        return "duplicate key";
    case TomlError::ConflictingTable:
        return "table conflicts with an existing value";
    }
    return "malformed TOML";
}

TomlTable::TomlTable() = default;
TomlTable::TomlTable(const TomlTable&) = default;
TomlTable::TomlTable(TomlTable&&) noexcept = default;
TomlTable& TomlTable::operator=(const TomlTable&) = default;
TomlTable& TomlTable::operator=(TomlTable&&) noexcept = default;
TomlTable::~TomlTable() = default;

std::size_t TomlTable::size() const noexcept { return keys_.size(); }

std::size_t TomlTable::indexOf(std::string_view key) const noexcept {
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i] == key) {
            return i;
        }
    }
    return keys_.size();
}

const TomlValue* TomlTable::find(std::string_view key) const noexcept {
    const std::size_t at = indexOf(key);
    return at == keys_.size() ? nullptr : &values_[at];
}

TomlValue* TomlTable::find(std::string_view key) noexcept {
    const std::size_t at = indexOf(key);
    return at == keys_.size() ? nullptr : &values_[at];
}

void TomlTable::set(std::string_view key, TomlValue value) {
    const std::size_t at = indexOf(key);
    if (at == keys_.size()) {
        keys_.emplace_back(key);
        values_.push_back(std::move(value));
    } else {
        values_[at] = std::move(value);
    }
}

const TomlTable* TomlTable::table(std::string_view key) const noexcept {
    const TomlValue* value = find(key);
    return value != nullptr ? value->get<TomlTable>() : nullptr;
}

std::optional<std::string_view> TomlTable::string(std::string_view key) const noexcept {
    const TomlValue* value = find(key);
    const auto* text = value != nullptr ? value->get<std::string>() : nullptr;
    return text != nullptr ? std::optional<std::string_view>{*text} : std::nullopt;
}

std::optional<std::int64_t> TomlTable::integer(std::string_view key) const noexcept {
    const TomlValue* value = find(key);
    const auto* number = value != nullptr ? value->get<std::int64_t>() : nullptr;
    return number != nullptr ? std::optional<std::int64_t>{*number} : std::nullopt;
}

std::optional<double> TomlTable::real(std::string_view key) const noexcept {
    const TomlValue* value = find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* number = value->get<double>()) {
        return *number;
    }
    // An integer where a float was expected is what a hand-edited file gives when someone writes
    // `1` for a rate, and refusing it would be pedantry.
    if (const auto* integer = value->get<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    return std::nullopt;
}

std::optional<bool> TomlTable::boolean(std::string_view key) const noexcept {
    const TomlValue* value = find(key);
    const auto* flag = value != nullptr ? value->get<bool>() : nullptr;
    return flag != nullptr ? std::optional<bool>{*flag} : std::nullopt;
}

std::vector<std::string> TomlTable::stringList(std::string_view key) const {
    std::vector<std::string> out;
    const TomlValue* value = find(key);
    const auto* array = value != nullptr ? value->get<std::vector<TomlValue>>() : nullptr;
    if (array == nullptr) {
        return out;
    }
    for (const TomlValue& element : *array) {
        if (const auto* text = element.get<std::string>()) {
            out.push_back(*text);
        }
    }
    return out;
}

std::vector<std::int64_t> TomlTable::integerList(std::string_view key) const {
    std::vector<std::int64_t> out;
    const TomlValue* value = find(key);
    const auto* array = value != nullptr ? value->get<std::vector<TomlValue>>() : nullptr;
    if (array == nullptr) {
        return out;
    }
    for (const TomlValue& element : *array) {
        if (const auto* number = element.get<std::int64_t>()) {
            out.push_back(*number);
        }
    }
    return out;
}

bool operator==(const TomlTable& lhs, const TomlTable& rhs) {
    return lhs.keys_ == rhs.keys_ && lhs.values_ == rhs.values_;
}

TomlValue::TomlValue() : storage_(std::string{}) {}

TomlValue::TomlValue(Storage storage) : storage_(std::move(storage)) {}

TomlValue::TomlValue(const TomlValue&) = default;
TomlValue::TomlValue(TomlValue&&) noexcept = default;
TomlValue& TomlValue::operator=(const TomlValue&) = default;
TomlValue& TomlValue::operator=(TomlValue&&) noexcept = default;
TomlValue::~TomlValue() = default;

TomlValue TomlValue::of(std::string value) { return TomlValue{Storage{std::move(value)}}; }

TomlValue TomlValue::of(std::int64_t value) { return TomlValue{Storage{value}}; }

TomlValue TomlValue::of(double value) { return TomlValue{Storage{value}}; }

TomlValue TomlValue::of(bool value) { return TomlValue{Storage{value}}; }

TomlValue TomlValue::ofArray(std::vector<TomlValue> value) { return TomlValue{Storage{std::move(value)}}; }

TomlValue TomlValue::ofTable(TomlTable value) { return TomlValue{Storage{std::move(value)}}; }

bool operator==(const TomlValue& lhs, const TomlValue& rhs) { return lhs.storage_ == rhs.storage_; }

std::expected<TomlTable, TomlError> parse(std::string_view text) {
    TomlTable root;
    TomlTable* current = &root;

    const std::vector<std::string_view> lines = splitLines(text);
    std::size_t i = 0;
    while (i < lines.size()) {
        const std::string_view line = trim(stripComment(lines[i]));
        ++i;
        if (line.empty()) {
            continue;
        }

        if (line.starts_with('[')) {
            // An array of tables is real TOML this parser does not do. Said out loud, because taken
            // as an ordinary header it becomes a table named "[guides]" and the file quietly means
            // something nobody wrote.
            if (line.starts_with("[[")) {
                return std::unexpected{TomlError::ArrayOfTablesUnsupported};
            }
            if (!line.ends_with(']')) {
                return std::unexpected{TomlError::MalformedTableHeader};
            }
            auto table = navigateToTable(root, trim(line.substr(1, line.size() - 2)));
            if (!table)
                return std::unexpected(table.error());
            current = *table;
            continue;
        }

        const std::optional<std::size_t> separator = findSeparator(line);
        if (!separator) {
            return std::unexpected{TomlError::MissingSeparator};
        }
        const std::string_view key = unquoteKey(trim(line.substr(0, *separator)));
        std::string valueText{trim(line.substr(*separator + 1))};

        // An array may span several lines, and in a hand-edited file it usually does. Keep taking
        // lines until the brackets close.
        if (valueText.starts_with('[')) {
            while (bracketDepth(valueText) > 0) {
                if (i >= lines.size()) {
                    return std::unexpected{TomlError::UnterminatedArray};
                }
                valueText += ' ';
                valueText += trim(stripComment(lines[i]));
                ++i;
            }
        }

        auto value = parseValue(valueText);
        if (!value) {
            return std::unexpected{value.error()};
        }
        if (current->find(key) != nullptr)
            return std::unexpected(TomlError::DuplicateKey);
        current->set(key, std::move(*value));
    }
    return root;
}

} // namespace basalt::wire::common::toml
