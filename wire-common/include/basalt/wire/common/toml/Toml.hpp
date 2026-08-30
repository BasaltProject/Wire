#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::common::toml {

/// Why a TOML document did not parse.
enum class TomlError : std::uint8_t {
    ArrayOfTablesUnsupported,
    MalformedTableHeader,
    MissingSeparator,
    UnterminatedString,
    UnterminatedArray,
    UnknownEscape,
    NotAValue,
};

[[nodiscard]] std::string_view tomlErrorMessage(TomlError error) noexcept;

class TomlValue;

/// A table, in the order its keys appear on the page, which is the order they are written back in.
class TomlTable {
public:
    TomlTable();
    TomlTable(const TomlTable&);
    TomlTable(TomlTable&&) noexcept;
    TomlTable& operator=(const TomlTable&);
    TomlTable& operator=(TomlTable&&) noexcept;
    ~TomlTable();

    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] const std::vector<std::string>& keys() const noexcept { return keys_; }

    [[nodiscard]] const TomlValue* find(std::string_view key) const noexcept;

    [[nodiscard]] TomlValue* find(std::string_view key) noexcept;

    void set(std::string_view key, TomlValue value);

    [[nodiscard]] const TomlTable* table(std::string_view key) const noexcept;

    [[nodiscard]] std::optional<std::string_view> string(std::string_view key) const noexcept;

    [[nodiscard]] std::optional<std::int64_t> integer(std::string_view key) const noexcept;

    [[nodiscard]] std::optional<double> real(std::string_view key) const noexcept;

    [[nodiscard]] std::optional<bool> boolean(std::string_view key) const noexcept;

    /// An array of strings, skipping any entry that is not one, so one bad element is not one bad
    /// file.
    [[nodiscard]] std::vector<std::string> stringList(std::string_view key) const;

    /// Same tolerance for one bad element as stringList.
    [[nodiscard]] std::vector<std::int64_t> integerList(std::string_view key) const;

    friend bool operator==(const TomlTable&, const TomlTable&);

private:
    [[nodiscard]] std::size_t indexOf(std::string_view key) const noexcept;

    std::vector<std::string> keys_;
    std::vector<TomlValue> values_;
};

class TomlValue {
public:
    using Storage = std::variant<std::string, std::int64_t, double, bool, std::vector<TomlValue>, TomlTable>;

    TomlValue();
    explicit TomlValue(Storage storage);
    TomlValue(const TomlValue&);
    TomlValue(TomlValue&&) noexcept;
    TomlValue& operator=(const TomlValue&);
    TomlValue& operator=(TomlValue&&) noexcept;
    ~TomlValue();

    [[nodiscard]] static TomlValue of(std::string value);
    [[nodiscard]] static TomlValue of(std::int64_t value);
    [[nodiscard]] static TomlValue of(double value);
    [[nodiscard]] static TomlValue of(bool value);
    [[nodiscard]] static TomlValue ofArray(std::vector<TomlValue> value);
    [[nodiscard]] static TomlValue ofTable(TomlTable value);

    template <typename T>
    [[nodiscard]] const T* get() const noexcept {
        return std::get_if<T>(&storage_);
    }

    template <typename T>
    [[nodiscard]] T* get() noexcept {
        return std::get_if<T>(&storage_);
    }

    friend bool operator==(const TomlValue&, const TomlValue&);

private:
    Storage storage_;
};

/// A hand-written parser for the subset of TOML the files under `config/` actually use: comments,
/// `[table]` and `[table.sub]` headers, bare and quoted keys, and string, integer, float, bool and
/// array values.
///
/// No multi-line strings, inline tables, arrays of tables or dates: none of those appear in any of
/// them, so they are refused rather than guessed at. Arrays *do* span lines, because a hand-edited
/// list of permissions or worlds is written one entry per line.
[[nodiscard]] std::expected<TomlTable, TomlError> parse(std::string_view text);

} // namespace basalt::wire::common::toml
