#pragma once

#include <basalt/wire/sql/Sqlite.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace basalt::wire::sql {

struct Migration {
    std::int32_t version;
    std::string_view statements;
};

/// Applies each migration above the stored version in order, one transaction each; a newer database is refused.
[[nodiscard]] std::expected<std::int32_t, std::string> migrate(Database& database,
                                                               std::span<const Migration> migrations);

} // namespace basalt::wire::sql
