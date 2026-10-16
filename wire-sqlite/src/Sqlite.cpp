#include <basalt/wire/sql/Sqlite.hpp>

#include <cstdint>
#include <sqlite3.h>
#include <utility>

namespace basalt::wire::sql {
namespace {
void initialiseOnce() {
    static const int status = sqlite3_initialize();
    static_cast<void>(status);
}
} // namespace

Statement::Statement(Statement&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), owner_(std::exchange(other.owner_, nullptr)) {}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        sqlite3_finalize(handle_);
        handle_ = std::exchange(other.handle_, nullptr);
        owner_ = std::exchange(other.owner_, nullptr);
    }
    return *this;
}

Statement::~Statement() { sqlite3_finalize(handle_); }

void Statement::bind(std::int32_t parameter, std::int64_t value) { sqlite3_bind_int64(handle_, parameter, value); }

void Statement::bind(std::int32_t parameter, double value) { sqlite3_bind_double(handle_, parameter, value); }

void Statement::bind(std::int32_t parameter, std::string_view value) {
    sqlite3_bind_text(handle_, parameter, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Statement::bind(std::int32_t parameter, std::span<const std::uint8_t> value) {
    sqlite3_bind_blob(handle_, parameter, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Statement::bind(std::int32_t parameter, const std::optional<std::string>& value) {
    if (value.has_value()) {
        bind(parameter, std::string_view{*value});
    } else {
        sqlite3_bind_null(handle_, parameter);
    }
}

void Statement::bind(std::int32_t parameter, const std::optional<std::int64_t>& value) {
    if (value.has_value()) {
        bind(parameter, *value);
    } else {
        sqlite3_bind_null(handle_, parameter);
    }
}

void Statement::bindBlobOrNull(std::int32_t parameter, std::span<const std::uint8_t> value) {
    if (value.empty()) {
        sqlite3_bind_null(handle_, parameter);
    } else {
        bind(parameter, value);
    }
}

bool Statement::step() {
    if (handle_ == nullptr) {
        return false;
    }
    const int status = sqlite3_step(handle_);
    if (status == SQLITE_ROW) {
        return true;
    }
    if (status != SQLITE_DONE && owner_ != nullptr) {
        owner_->raise(sqlite3_errmsg(sqlite3_db_handle(handle_)));
    }
    return false;
}

bool Statement::execute() {
    if (handle_ == nullptr) {
        return false;
    }
    const int status = sqlite3_step(handle_);
    if (status == SQLITE_DONE || status == SQLITE_ROW) {
        return true;
    }
    if (owner_ != nullptr) {
        owner_->raise(sqlite3_errmsg(sqlite3_db_handle(handle_)));
    }
    return false;
}

bool Statement::isNull(std::int32_t column) const { return sqlite3_column_type(handle_, column) == SQLITE_NULL; }

std::int64_t Statement::integer(std::int32_t column) const { return sqlite3_column_int64(handle_, column); }

double Statement::real(std::int32_t column) const { return sqlite3_column_double(handle_, column); }

std::string Statement::text(std::int32_t column) const {
    const unsigned char* value = sqlite3_column_text(handle_, column);
    if (value == nullptr) {
        return {};
    }
    return std::string{reinterpret_cast<const char*>(value),
                       static_cast<std::size_t>(sqlite3_column_bytes(handle_, column))};
}

std::optional<std::string> Statement::textOrNull(std::int32_t column) const {
    if (isNull(column)) {
        return std::nullopt;
    }
    return text(column);
}

std::optional<std::int64_t> Statement::integerOrNull(std::int32_t column) const {
    if (isNull(column)) {
        return std::nullopt;
    }
    return integer(column);
}

std::vector<std::uint8_t> Statement::blob(std::int32_t column) const {
    const void* value = sqlite3_column_blob(handle_, column);
    const int size = sqlite3_column_bytes(handle_, column);
    if (value == nullptr || size <= 0) {
        return {};
    }
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    return std::vector<std::uint8_t>{bytes, bytes + static_cast<std::size_t>(size)};
}

Database::Database(Database&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), error_(std::move(other.error_)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = std::exchange(other.handle_, nullptr);
        error_ = std::move(other.error_);
    }
    return *this;
}

Database::~Database() { close(); }

std::optional<Database> Database::open(const std::string& path) {
    initialiseOnce();
    sqlite3* handle = nullptr;
    if (sqlite3_open_v2(path.c_str(), &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        sqlite3_close(handle);
        return std::nullopt;
    }
    Database database;
    database.handle_ = handle;
    database.exec("PRAGMA journal_mode = WAL");
    database.exec("PRAGMA synchronous = NORMAL");
    database.exec("PRAGMA foreign_keys = ON");
    database.clearError();
    return database;
}

Statement Database::prepare(std::string_view sql) {
    sqlite3_stmt* handle = nullptr;
    if (sqlite3_prepare_v2(handle_, sql.data(), static_cast<int>(sql.size()), &handle, nullptr) != SQLITE_OK) {
        raise(sqlite3_errmsg(handle_));
        return Statement{};
    }
    return Statement{handle, this};
}

bool Database::exec(std::string_view sql) {
    char* message = nullptr;
    const std::string statement{sql};
    if (sqlite3_exec(handle_, statement.c_str(), nullptr, nullptr, &message) != SQLITE_OK) {
        raise(message == nullptr ? "unknown error" : message);
        sqlite3_free(message);
        return false;
    }
    sqlite3_free(message);
    return true;
}

std::vector<std::string> Database::tables() {
    std::vector<std::string> names;
    Statement statement = prepare("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name");
    while (statement.step()) {
        std::string name = statement.text(0);
        if (!name.starts_with("sqlite_")) {
            names.push_back(std::move(name));
        }
    }
    return names;
}

std::vector<std::string> Database::columns(std::string_view table) {
    std::vector<std::string> names;
    Statement statement = prepare("PRAGMA table_info(\"" + std::string{table} + "\")");
    while (statement.step()) {
        names.push_back(statement.text(1));
    }
    return names;
}

void Database::close() {
    if (handle_ == nullptr) {
        return;
    }
    sqlite3_close(handle_);
    handle_ = nullptr;
}

void Database::raise(std::string_view what) {
    if (error_.empty()) {
        error_ = std::string{what};
    }
}
} // namespace basalt::wire::sql
