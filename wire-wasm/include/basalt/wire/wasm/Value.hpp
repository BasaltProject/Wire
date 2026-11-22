#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::wasm {

inline constexpr std::size_t kValueMaxBytes = 1U << 20U;
inline constexpr std::size_t kValueMaxDepth = 16;
inline constexpr std::size_t kValueMaxNodes = 65536;
inline constexpr std::size_t kValueMaxKeyBytes = 256;

struct Blob {
    std::vector<std::uint8_t> data;
    bool operator==(const Blob&) const = default;
};

struct MapEntry;

/// A structured value of the ABI (abi/basalt_values.h): null, bool, i64, f64, string, bytes, list or map.
class Value {
public:
    using List = std::vector<Value>;
    using Map = std::vector<MapEntry>;
    using Storage = std::variant<std::monostate, bool, std::int64_t, double, std::string, Blob, List, Map>;

    Value();
    Value(bool value);
    Value(std::int64_t value);
    Value(int value);
    Value(double value);
    Value(std::string value);
    Value(const char* value);
    Value(Blob value);
    Value(List value);
    Value(Map value);
    Value(const Value& other);
    Value(Value&& other) noexcept;
    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;
    ~Value();

    [[nodiscard]] bool isNull() const noexcept { return std::holds_alternative<std::monostate>(data); }

    [[nodiscard]] const bool* asBool() const noexcept { return std::get_if<bool>(&data); }

    [[nodiscard]] const std::int64_t* asInt() const noexcept { return std::get_if<std::int64_t>(&data); }

    [[nodiscard]] const double* asFloat() const noexcept { return std::get_if<double>(&data); }

    [[nodiscard]] const std::string* asString() const noexcept { return std::get_if<std::string>(&data); }

    [[nodiscard]] const Blob* asBytes() const noexcept { return std::get_if<Blob>(&data); }

    [[nodiscard]] const List* asList() const noexcept { return std::get_if<List>(&data); }

    [[nodiscard]] const Map* asMap() const noexcept { return std::get_if<Map>(&data); }

    /// The value under `key` when this is a map that has it.
    [[nodiscard]] const Value* find(std::string_view key) const noexcept;

    bool operator==(const Value& other) const;

    Storage data;
};

struct MapEntry {
    std::string key;
    Value value;
    bool operator==(const MapEntry&) const = default;
};

inline Value::Value() = default;

inline Value::Value(bool value) : data(value) {}

inline Value::Value(std::int64_t value) : data(value) {}

inline Value::Value(int value) : data(std::int64_t{value}) {}

inline Value::Value(double value) : data(value) {}

inline Value::Value(std::string value) : data(std::move(value)) {}

inline Value::Value(const char* value) : data(std::string{value}) {}

inline Value::Value(Blob value) : data(std::move(value)) {}

inline Value::Value(List value) : data(std::move(value)) {}

inline Value::Value(Map value) : data(std::move(value)) {}

inline Value::Value(const Value& other) = default;
inline Value::Value(Value&& other) noexcept = default;
inline Value& Value::operator=(const Value& other) = default;
inline Value& Value::operator=(Value&& other) noexcept = default;
inline Value::~Value() = default;

inline bool Value::operator==(const Value& other) const { return data == other.data; }

/// Appends the encoding of `value`; false, with `out` untouched, when it breaks a limit of the value table.
[[nodiscard]] bool encodeValue(const Value& value, std::string& out);

/// Reads exactly one value; the error is a short reason (truncated, over a limit, duplicate key, trailing bytes).
[[nodiscard]] std::expected<Value, std::string> decodeValue(std::span<const std::uint8_t> bytes);

} // namespace basalt::wire::wasm
