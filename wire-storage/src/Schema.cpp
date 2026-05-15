#include <basalt/wire/storage/Schema.hpp>

#include <algorithm>
#include <filesystem>

namespace basalt::wire::storage {
namespace schema {

std::string_view identityDdl() {
    return R"(
-- One row per player that has connected, keyed by UUID. The address is the last one they connected from and is
-- only written when the server list greeting is on.
CREATE TABLE IF NOT EXISTS player (
    uuid TEXT NOT NULL PRIMARY KEY,
    name TEXT NOT NULL,
    first_seen INTEGER NOT NULL,
    last_seen INTEGER NOT NULL,
    group_name TEXT,
    last_ip TEXT
);

CREATE INDEX IF NOT EXISTS player_name ON player(name);
CREATE INDEX IF NOT EXISTS player_last_seen ON player(last_seen);
CREATE INDEX IF NOT EXISTS player_last_ip ON player(last_ip);
)";
}

std::string_view accessDdl() {
    return R"(
-- Who may connect: bans by player, bans by address, and the whitelist. In the same database as
-- players and not in config/, because these are state the server writes, not configuration the
-- operator maintains by hand.
CREATE TABLE IF NOT EXISTS ban (
    -- The player's UUID, or the address for an IP ban. One table, because both answer the same
    -- question at login and a two-table version needs the same union query anyway.
    subject TEXT NOT NULL PRIMARY KEY,
    -- 'player' or 'ip', so a UUID that looks like an address cannot collide with one.
    kind TEXT NOT NULL,
    name TEXT,
    reason TEXT NOT NULL,
    source TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    -- Null means it never expires, which is every ban vanilla's own commands create.
    expires_at INTEGER
);

CREATE INDEX IF NOT EXISTS ban_kind ON ban(kind);

CREATE TABLE IF NOT EXISTS whitelist (
    uuid TEXT NOT NULL PRIMARY KEY,
    name TEXT NOT NULL,
    added_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS whitelist_name ON whitelist(name);

)";
}

std::string_view pluginDdl() {
    return R"(
-- What plugins keep about players, one namespace each. Keyed by the plugin's id: a plugin can only
-- ever see rows with its own id in them, so two of them cannot read or overwrite each other by
-- accident, and neither can touch the columns the server itself depends on. Uninstalling a plugin
-- leaves its rows here on purpose, which is what the orphan warning at startup is about.
CREATE TABLE IF NOT EXISTS plugin_data (
    plugin TEXT NOT NULL,
    uuid TEXT NOT NULL,
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    PRIMARY KEY (plugin, uuid, key)
);

CREATE INDEX IF NOT EXISTS plugin_data_by_plugin ON plugin_data(plugin);

-- What plugins keep on a holder that is not a row of plugin_data (a player, online or not): one NBT
-- compound per holder, read and written whole, in the same form an entity or a chunk saves it.
CREATE TABLE IF NOT EXISTS holder_data (
    kind TEXT NOT NULL,
    id TEXT NOT NULL,
    data BLOB NOT NULL,
    PRIMARY KEY (kind, id)
);
)";
}

} // namespace schema

namespace {

[[nodiscard]] std::string missingColumns(wire::sql::Database& database, std::string_view ddl, const std::string& path) {
    std::optional<wire::sql::Database> reference = wire::sql::Database::open(":memory:");
    if (!reference.has_value()) {
        return {};
    }
    reference->exec(ddl);
    for (const std::string& table : reference->tables()) {
        const std::vector<std::string> actual = database.columns(table);
        std::string names;
        for (const std::string& column : reference->columns(table)) {
            if (std::ranges::find(actual, column) == actual.end()) {
                names += (names.empty() ? "" : ", ") + column;
            }
        }
        if (!names.empty()) {
            return path + " was written by an older build: its '" + table + "' table has no " + names +
                   " column. Basalt has no database migrations yet, so delete " + path +
                   " (and its -wal and -shm files) and start again.";
        }
    }
    return {};
}

} // namespace

std::expected<std::unique_ptr<wire::sql::Database>, std::string> openPlayerDatabase(const std::string& path,
                                                                                    std::string_view ddl) {
    const std::filesystem::path parent = std::filesystem::path{path}.parent_path();
    if (!parent.empty()) {
        std::error_code ignored;
        std::filesystem::create_directories(parent, ignored);
    }
    std::optional<wire::sql::Database> opened = wire::sql::Database::open(path);
    if (!opened.has_value()) {
        return std::unexpected(path + " could not be opened");
    }
    auto database = std::make_unique<wire::sql::Database>(std::move(*opened));
    if (database->tables().empty() && !database->exec(ddl)) {
        return std::unexpected(path + " could not be created: " + database->error());
    }
    if (std::string mismatch = missingColumns(*database, ddl, path); !mismatch.empty()) {
        return std::unexpected(std::move(mismatch));
    }
    return database;
}

} // namespace basalt::wire::storage
