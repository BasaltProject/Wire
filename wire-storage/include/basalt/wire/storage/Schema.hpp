#pragma once

#include <basalt/wire/sql/Sqlite.hpp>

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace basalt::wire::storage {

namespace schema {

/// The lean `player` table of a proxy: who connected, when, from where and in which group.
[[nodiscard]] std::string_view identityDdl();

/// The `ban` and `whitelist` tables.
[[nodiscard]] std::string_view accessDdl();

/// The `plugin_data` and `holder_data` tables.
[[nodiscard]] std::string_view pluginDdl();

} // namespace schema

/// Opens `path`, creating its parent directory and the tables of `ddl` when the file is new.
/// A file missing a column that `ddl` declares is refused with a message saying so.
[[nodiscard]] std::expected<std::unique_ptr<wire::sql::Database>, std::string>
openPlayerDatabase(const std::string& path, std::string_view ddl);

} // namespace basalt::wire::storage
