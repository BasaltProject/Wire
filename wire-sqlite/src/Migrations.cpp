#include <basalt/wire/sql/Migrations.hpp>

namespace basalt::wire::sql {
namespace {

std::int32_t storedVersion(Database& database) {
    Statement query = database.prepare("PRAGMA user_version");
    return query.isValid() && query.step() ? static_cast<std::int32_t>(query.integer(0)) : -1;
}

} // namespace

std::expected<std::int32_t, std::string> migrate(Database& database, std::span<const Migration> migrations) {
    for (std::size_t i = 1; i < migrations.size(); ++i) {
        if (migrations[i].version <= migrations[i - 1].version)
            return std::unexpected("migration versions must rise strictly");
    }
    std::int32_t current = storedVersion(database);
    if (current < 0)
        return std::unexpected("could not read the schema version: " + database.error());
    const std::int32_t newest = migrations.empty() ? 0 : migrations.back().version;
    if (current > newest)
        return std::unexpected("the database is at schema version " + std::to_string(current) +
                               " but this build only knows up to " + std::to_string(newest));
    for (const Migration& migration : migrations) {
        if (migration.version <= current)
            continue;
        const bool applied = database.transaction([&] {
            return database.exec(migration.statements) &&
                   database.exec("PRAGMA user_version = " + std::to_string(migration.version));
        });
        if (!applied)
            return std::unexpected("migration " + std::to_string(migration.version) + " failed: " + database.error());
        current = migration.version;
    }
    return current;
}

} // namespace basalt::wire::sql
