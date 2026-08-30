#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::common {

class JsonValue;

/// Why a document did not parse, or did not hold what the caller asked for.
enum class JsonError : std::uint8_t {
    SyntaxError,
    DepthLimitExceeded,
    WrongType,
};

/// An object's members, in document order. Order matters for anything written back out, and a
/// datapack object has a handful of keys, so a scan over contiguous names beats a hash map.
class JsonObject {
public:
    JsonObject();
    JsonObject(const JsonObject&);
    JsonObject(JsonObject&&) noexcept;
    JsonObject& operator=(const JsonObject&);
    JsonObject& operator=(JsonObject&&) noexcept;
    ~JsonObject();

    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] const std::vector<std::string>& names() const noexcept { return names_; }

    [[nodiscard]] const std::vector<JsonValue>& values() const noexcept { return values_; }

    [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;

    void set(std::string_view key, JsonValue value);

    friend bool operator==(const JsonObject&, const JsonObject&);

private:
    std::vector<std::string> names_;
    std::vector<JsonValue> values_;
};

/// A parsed JSON value. Deliberately a small owned tree rather than a view over the input: the
/// documents Basalt reads (datapacks, registries, config, chat components) outlive their buffers.
class JsonValue {
public:
    struct Null {
        friend bool operator==(const Null&, const Null&) = default;
    };

    /// Numbers keep the shape they were written in: NBT and the datapack codecs care whether a
    /// value was an integer, and re-deriving that from a double loses it.
    using Storage = std::variant<Null, bool, std::int64_t, double, std::string, std::vector<JsonValue>, JsonObject>;

    JsonValue();
    explicit JsonValue(Storage storage);
    JsonValue(const JsonValue&);
    JsonValue(JsonValue&&) noexcept;
    JsonValue& operator=(const JsonValue&);
    JsonValue& operator=(JsonValue&&) noexcept;
    ~JsonValue();

    [[nodiscard]] static JsonValue of(bool value);
    [[nodiscard]] static JsonValue of(std::int64_t value);
    [[nodiscard]] static JsonValue of(double value);
    [[nodiscard]] static JsonValue of(std::string value);
    [[nodiscard]] static JsonValue ofArray(std::vector<JsonValue> value);
    [[nodiscard]] static JsonValue ofObject(JsonObject value);

    [[nodiscard]] bool isNull() const noexcept;

    template <typename T>
    [[nodiscard]] const T* get() const noexcept {
        return std::get_if<T>(&storage_);
    }

    [[nodiscard]] const Storage& storage() const noexcept { return storage_; }

    friend bool operator==(const JsonValue&, const JsonValue&);

private:
    Storage storage_;
};

/// Guards against a crafted document nesting deep enough to run the stack out.
inline constexpr int kJsonMaxDepth = 256;

[[nodiscard]] std::expected<JsonValue, JsonError> parseJson(std::string_view text);

/// Compact output, no whitespace. Object members keep the order they were inserted in.
[[nodiscard]] std::string writeJson(const JsonValue& value);

} // namespace basalt::wire::common
