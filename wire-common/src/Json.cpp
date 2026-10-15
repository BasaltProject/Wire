#include <basalt/wire/common/json/Json.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <simdjson.h>

namespace basalt::wire::common {
namespace {

std::expected<JsonValue, JsonError> convert(simdjson::dom::element element, int depth) {
    if (depth > kJsonMaxDepth) {
        return std::unexpected{JsonError::DepthLimitExceeded};
    }

    switch (element.type()) {
    case simdjson::dom::element_type::NULL_VALUE:
        return JsonValue{};
    case simdjson::dom::element_type::BOOL:
        return JsonValue::of(bool{element});
    case simdjson::dom::element_type::INT64:
        return JsonValue::of(std::int64_t{element});
    case simdjson::dom::element_type::UINT64:
        return JsonValue::of(static_cast<double>(std::uint64_t{element}));
    case simdjson::dom::element_type::DOUBLE:
        return JsonValue::of(double{element});
    case simdjson::dom::element_type::STRING:
        return JsonValue::of(std::string{std::string_view{element}});
    case simdjson::dom::element_type::ARRAY: {
        std::vector<JsonValue> out;
        for (simdjson::dom::element child : simdjson::dom::array{element}) {
            auto converted = convert(child, depth + 1);
            if (!converted) {
                return converted;
            }
            out.push_back(std::move(*converted));
        }
        return JsonValue::ofArray(std::move(out));
    }
    case simdjson::dom::element_type::OBJECT: {
        JsonObject out;
        for (auto [key, child] : simdjson::dom::object{element}) {
            auto converted = convert(child, depth + 1);
            if (!converted) {
                return converted;
            }
            out.set(key, std::move(*converted));
        }
        return JsonValue::ofObject(std::move(out));
    }
    }
    return std::unexpected{JsonError::SyntaxError};
}

void appendEscaped(std::string& out, std::string_view value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    out += '"';
    for (const char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += "\\u00";
                out += kDigits[(static_cast<unsigned char>(c) >> 4) & 0xF];
                out += kDigits[static_cast<unsigned char>(c) & 0xF];
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

void appendValue(std::string& out, const JsonValue& value);

void appendArray(std::string& out, const std::vector<JsonValue>& values) {
    out += '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        appendValue(out, values[i]);
    }
    out += ']';
}

void appendObject(std::string& out, const JsonObject& object) {
    out += '{';
    for (std::size_t i = 0; i < object.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        appendEscaped(out, object.names()[i]);
        out += ':';
        appendValue(out, object.values()[i]);
    }
    out += '}';
}

void appendValue(std::string& out, const JsonValue& value) {
    if (value.isNull()) {
        out += "null";
    } else if (const auto* boolean = value.get<bool>()) {
        out += *boolean ? "true" : "false";
    } else if (const auto* integer = value.get<std::int64_t>()) {
        out += std::to_string(*integer);
    } else if (const auto* number = value.get<double>()) {
        std::array<char, 40> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), *number);
        out.append(buffer.data(), result.ptr);
    } else if (const auto* string = value.get<std::string>()) {
        appendEscaped(out, *string);
    } else if (const auto* array = value.get<std::vector<JsonValue>>()) {
        appendArray(out, *array);
    } else {
        appendObject(out, std::get<JsonObject>(value.storage()));
    }
}

} // namespace

JsonObject::JsonObject() = default;

JsonObject::JsonObject(const JsonObject&) = default;

JsonObject::JsonObject(JsonObject&&) noexcept = default;

JsonObject& JsonObject::operator=(const JsonObject&) = default;

JsonObject& JsonObject::operator=(JsonObject&&) noexcept = default;

JsonObject::~JsonObject() = default;

std::size_t JsonObject::size() const noexcept { return names_.size(); }

bool JsonObject::empty() const noexcept { return names_.empty(); }

const JsonValue* JsonObject::find(std::string_view key) const noexcept {
    for (std::size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == key) {
            return &values_[i];
        }
    }
    return nullptr;
}

void JsonObject::set(std::string_view key, JsonValue value) {
    for (std::size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == key) {
            values_[i] = std::move(value);
            return;
        }
    }
    names_.emplace_back(key);
    values_.push_back(std::move(value));
}

bool operator==(const JsonObject& lhs, const JsonObject& rhs) {
    return lhs.names_ == rhs.names_ && lhs.values_ == rhs.values_;
}

JsonValue::JsonValue() : storage_(Null{}) {}

JsonValue::JsonValue(Storage storage) : storage_(std::move(storage)) {}

JsonValue::JsonValue(const JsonValue&) = default;

JsonValue::JsonValue(JsonValue&&) noexcept = default;

JsonValue& JsonValue::operator=(const JsonValue&) = default;

JsonValue& JsonValue::operator=(JsonValue&&) noexcept = default;

JsonValue::~JsonValue() = default;

JsonValue JsonValue::of(bool value) { return JsonValue{Storage{value}}; }

JsonValue JsonValue::of(std::int64_t value) { return JsonValue{Storage{value}}; }

JsonValue JsonValue::of(double value) { return JsonValue{Storage{value}}; }

JsonValue JsonValue::of(std::string value) { return JsonValue{Storage{std::move(value)}}; }

JsonValue JsonValue::ofArray(std::vector<JsonValue> value) { return JsonValue{Storage{std::move(value)}}; }

JsonValue JsonValue::ofObject(JsonObject value) { return JsonValue{Storage{std::move(value)}}; }

bool JsonValue::isNull() const noexcept { return std::holds_alternative<Null>(storage_); }

bool operator==(const JsonValue& lhs, const JsonValue& rhs) { return lhs.storage_ == rhs.storage_; }

std::expected<JsonValue, JsonError> parseJson(std::string_view text) {
    simdjson::dom::parser parser;
    simdjson::dom::element root;
    const simdjson::error_code error = parser.parse(text.data(), text.size()).get(root);
    if (error != simdjson::SUCCESS) {
        return std::unexpected{JsonError::SyntaxError};
    }
    return convert(root, 0);
}

std::string writeJson(const JsonValue& value) {
    std::string out;
    appendValue(out, value);
    return out;
}

} // namespace basalt::wire::common
