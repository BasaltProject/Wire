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

/// Applies, in order and one transaction each, every migration above the stored schema version.
/// Versions must rise strictly; a database newer than the newest migration is refused, never downgraded.
[[nodiscard]] std::expected<std::int32_t, std::string> migrate(Database& database,
                                                               std::span<const Migration> migrations);

} // namespace basalt::wire::sql
