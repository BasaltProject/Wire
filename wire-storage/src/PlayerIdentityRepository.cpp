#include <basalt/wire/storage/PlayerIdentityRepository.hpp>

namespace basalt::wire::storage {
namespace {

[[nodiscard]] std::vector<PlayerSummary> summaries(wire::sql::Statement& statement) {
    std::vector<PlayerSummary> found;
    while (statement.step()) {
        found.push_back(PlayerSummary{.uuid = statement.text(0),
                                      .name = statement.text(1),
                                      .value = statement.isNull(2) ? 0 : statement.integer(2)});
    }
    return found;
}

} // namespace

void PlayerIdentityRepository::recordLogin(std::string_view uuid, std::string_view name, std::int64_t nowEpochSeconds) {
    wire::sql::Statement statement =
        database_->prepare("INSERT INTO player(uuid, name, first_seen, last_seen) VALUES (?, ?, ?, ?) "
                           "ON CONFLICT(uuid) DO UPDATE SET name = excluded.name, last_seen = excluded.last_seen");
    statement.bind(1, uuid);
    statement.bind(2, name);
    statement.bind(3, nowEpochSeconds);
    statement.bind(4, nowEpochSeconds);
    statement.execute();
}

bool PlayerIdentityRepository::exists(std::string_view uuid) {
    wire::sql::Statement statement = database_->prepare("SELECT EXISTS(SELECT 1 FROM player WHERE uuid = ?)");
    statement.bind(1, uuid);
    return statement.step() && statement.integer(0) != 0;
}

std::optional<std::string> PlayerIdentityRepository::groupOf(std::string_view uuid) {
    wire::sql::Statement statement = database_->prepare("SELECT group_name FROM player WHERE uuid = ?");
    statement.bind(1, uuid);
    if (!statement.step() || statement.isNull(0)) {
        return std::nullopt;
    }
    return statement.text(0);
}

bool PlayerIdentityRepository::setGroup(std::string_view uuid, const std::optional<std::string>& group) {
    if (!exists(uuid)) {
        return false;
    }
    wire::sql::Statement statement = database_->prepare("UPDATE player SET group_name = ? WHERE uuid = ?");
    statement.bind(1, group);
    statement.bind(2, uuid);
    return statement.execute();
}

void PlayerIdentityRepository::setLastIp(std::string_view uuid, const std::optional<std::string>& address) {
    wire::sql::Statement statement = database_->prepare("UPDATE player SET last_ip = ? WHERE uuid = ?");
    statement.bind(1, address);
    statement.bind(2, uuid);
    statement.execute();
}

std::optional<std::string> PlayerIdentityRepository::findNameByAddress(std::string_view address) {
    wire::sql::Statement statement =
        database_->prepare("SELECT name FROM player WHERE last_ip = ? ORDER BY last_seen DESC LIMIT 1");
    statement.bind(1, address);
    if (!statement.step()) {
        return std::nullopt;
    }
    return statement.text(0);
}

std::optional<std::string> PlayerIdentityRepository::findUuidByName(std::string_view name) {
    wire::sql::Statement statement =
        database_->prepare("SELECT uuid FROM player WHERE name = ? COLLATE NOCASE ORDER BY last_seen DESC LIMIT 1");
    statement.bind(1, name);
    if (!statement.step()) {
        return std::nullopt;
    }
    return statement.text(0);
}

std::vector<PlayerSummary> PlayerIdentityRepository::findInGroup(std::string_view group) {
    wire::sql::Statement statement =
        database_->prepare("SELECT uuid, name, NULL FROM player WHERE group_name = ? ORDER BY name");
    statement.bind(1, group);
    return summaries(statement);
}

std::vector<PlayerSummary> PlayerIdentityRepository::findLastSeenBefore(std::int64_t epochSeconds) {
    wire::sql::Statement statement =
        database_->prepare("SELECT uuid, name, last_seen FROM player WHERE last_seen < ? ORDER BY last_seen");
    statement.bind(1, epochSeconds);
    return summaries(statement);
}

void PlayerIdentityRepository::remove(std::string_view uuid) {
    wire::sql::Statement statement = database_->prepare("DELETE FROM player WHERE uuid = ?");
    statement.bind(1, uuid);
    statement.execute();
}

std::int64_t PlayerIdentityRepository::count() {
    wire::sql::Statement statement = database_->prepare("SELECT count(*) FROM player");
    return statement.step() ? statement.integer(0) : 0;
}

} // namespace basalt::wire::storage
