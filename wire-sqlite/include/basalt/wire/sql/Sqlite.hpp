#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace basalt::wire::sql {
class Database;

class Statement {
public:
    Statement() = default;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&&) noexcept;
    Statement& operator=(Statement&&) noexcept;
    ~Statement();

    [[nodiscard]] bool isValid() const noexcept { return handle_ != nullptr; }

    void bind(std::int32_t parameter, std::int64_t value);
    void bind(std::int32_t parameter, double value);
    void bind(std::int32_t parameter, std::string_view value);

    void bind(std::int32_t parameter, const std::string& value) { bind(parameter, std::string_view{value}); }

    void bind(std::int32_t parameter, std::span<const std::uint8_t> value);
    void bind(std::int32_t parameter, const std::optional<std::string>& value);
    void bind(std::int32_t parameter, const std::optional<std::int64_t>& value);
    void bindBlobOrNull(std::int32_t parameter, std::span<const std::uint8_t> value);

    [[nodiscard]] bool step();

    bool execute();

    [[nodiscard]] bool isNull(std::int32_t column) const;
    [[nodiscard]] std::int64_t integer(std::int32_t column) const;
    [[nodiscard]] double real(std::int32_t column) const;
    [[nodiscard]] std::string text(std::int32_t column) const;
    [[nodiscard]] std::optional<std::string> textOrNull(std::int32_t column) const;
    [[nodiscard]] std::optional<std::int64_t> integerOrNull(std::int32_t column) const;
    [[nodiscard]] std::vector<std::uint8_t> blob(std::int32_t column) const;

private:
    friend class Database;

    Statement(sqlite3_stmt* handle, Database* owner) : handle_(handle), owner_(owner) {}

    sqlite3_stmt* handle_ = nullptr;
    Database* owner_ = nullptr;
};

class Database {
public:
    Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    ~Database();

    [[nodiscard]] static std::optional<Database> open(const std::string& path);

    [[nodiscard]] bool isOpen() const noexcept { return handle_ != nullptr; }

    [[nodiscard]] Statement prepare(std::string_view sql);

    bool exec(std::string_view sql);

    template <typename Action>
    bool transaction(Action&& action) {
        if (!exec("BEGIN")) {
            return false;
        }
        if (!action() || hasError()) {
            exec("ROLLBACK");
            return false;
        }
        return exec("COMMIT");
    }

    [[nodiscard]] std::vector<std::string> tables();

    [[nodiscard]] std::vector<std::string> columns(std::string_view table);

    [[nodiscard]] bool hasError() const noexcept { return !error_.empty(); }

    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    void clearError() { error_.clear(); }

    void close();

private:
    friend class Statement;

    void raise(std::string_view what);

    sqlite3* handle_ = nullptr;
    std::string error_;
};
} // namespace basalt::wire::sql
