#include <basalt/wire/storage/AntibotVerifiedRepository.hpp>

namespace basalt::wire::storage {

void AntibotVerifiedRepository::upsert(std::string_view name, std::string_view origin,
                                       std::int64_t expiresAtEpochSeconds) {
    wire::sql::Statement statement =
        database_->prepare("INSERT INTO antibot_verified(name, origin, expires_at) VALUES (?, ?, ?) "
                           "ON CONFLICT(name, origin) DO UPDATE SET expires_at = excluded.expires_at");
    statement.bind(1, name);
    statement.bind(2, origin);
    statement.bind(3, expiresAtEpochSeconds);
    statement.execute();
}

void AntibotVerifiedRepository::remove(std::string_view name) {
    wire::sql::Statement statement = database_->prepare("DELETE FROM antibot_verified WHERE name = ?");
    statement.bind(1, name);
    statement.execute();
}

std::vector<VerifiedRow> AntibotVerifiedRepository::live(std::int64_t nowEpochSeconds, std::int32_t limit) {
    std::vector<VerifiedRow> rows;
    wire::sql::Statement statement = database_->prepare(
        "SELECT name, origin, expires_at FROM antibot_verified WHERE expires_at > ? ORDER BY expires_at DESC LIMIT ?");
    statement.bind(1, nowEpochSeconds);
    statement.bind(2, static_cast<std::int64_t>(limit));
    while (statement.step())
        rows.push_back({statement.text(0), statement.text(1), statement.integer(2)});
    return rows;
}

std::int32_t AntibotVerifiedRepository::purgeExpired(std::int64_t nowEpochSeconds, std::int32_t limit) {
    wire::sql::Statement statement =
        database_->prepare("DELETE FROM antibot_verified WHERE rowid IN "
                           "(SELECT rowid FROM antibot_verified WHERE expires_at <= ? LIMIT ?) RETURNING 1");
    statement.bind(1, nowEpochSeconds);
    statement.bind(2, static_cast<std::int64_t>(limit));
    std::int32_t removed = 0;
    while (statement.step())
        ++removed;
    return removed;
}

std::int32_t AntibotVerifiedRepository::trimTo(std::int32_t keep) {
    wire::sql::Statement statement = database_->prepare(
        "DELETE FROM antibot_verified WHERE rowid IN "
        "(SELECT rowid FROM antibot_verified ORDER BY expires_at DESC LIMIT -1 OFFSET ?) RETURNING 1");
    statement.bind(1, static_cast<std::int64_t>(keep));
    std::int32_t removed = 0;
    while (statement.step())
        ++removed;
    return removed;
}

std::int64_t AntibotVerifiedRepository::count() {
    wire::sql::Statement statement = database_->prepare("SELECT COUNT(*) FROM antibot_verified");
    return statement.step() ? statement.integer(0) : 0;
}

} // namespace basalt::wire::storage
