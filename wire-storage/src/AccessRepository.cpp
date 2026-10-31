#include <basalt/wire/storage/AccessRepository.hpp>

namespace basalt::wire::storage {
namespace {
constexpr std::string_view kBanColumns = "subject, kind, name, reason, source, created_at, expires_at";
constexpr std::string_view kWhitelistColumns = "uuid, name, added_at";

[[nodiscard]] BanRecord readBan(const wire::sql::Statement& row, BanKind kind) {
    return BanRecord{.subject = row.text(0),
                     .kind = kind,
                     .name = row.textOrNull(2),
                     .reason = row.text(3),
                     .source = row.text(4),
                     .createdAtEpochSeconds = row.integer(5),
                     .expiresAtEpochSeconds = row.integerOrNull(6)};
}

[[nodiscard]] WhitelistEntry readWhitelist(const wire::sql::Statement& row) {
    return WhitelistEntry{.uuid = row.text(0), .name = row.text(1), .addedAtEpochSeconds = row.integer(2)};
}
} // namespace

std::string_view storedNameOf(BanKind kind) { return kind == BanKind::kPlayer ? "player" : "ip"; }

std::optional<BanRecord> AccessRepository::banOf(std::string_view subject, BanKind kind) {
    wire::sql::Statement statement =
        database_->prepare("SELECT " + std::string{kBanColumns} + " FROM ban WHERE subject = ? AND kind = ?");
    statement.bind(1, subject);
    statement.bind(2, storedNameOf(kind));
    if (!statement.step()) {
        return std::nullopt;
    }
    return readBan(statement, kind);
}

std::vector<BanRecord> AccessRepository::bans(BanKind kind) {
    std::vector<BanRecord> found;
    wire::sql::Statement statement =
        database_->prepare("SELECT " + std::string{kBanColumns} + " FROM ban WHERE kind = ? ORDER BY created_at");
    statement.bind(1, storedNameOf(kind));
    while (statement.step()) {
        found.push_back(readBan(statement, kind));
    }
    return found;
}

void AccessRepository::ban(const BanRecord& record) {
    wire::sql::Statement statement =
        database_->prepare("INSERT OR REPLACE INTO ban(subject, kind, name, reason, source, created_at, expires_at) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?)");
    statement.bind(1, record.subject);
    statement.bind(2, storedNameOf(record.kind));
    statement.bind(3, record.name);
    statement.bind(4, record.reason);
    statement.bind(5, record.source);
    statement.bind(6, record.createdAtEpochSeconds);
    statement.bind(7, record.expiresAtEpochSeconds);
    statement.execute();
    if (observer_) {
        observer_(
            AccessChange{AccessChange::Type::kBanned, record.kind, record.subject, record.name.value_or(""), record});
    }
}

bool AccessRepository::pardon(std::string_view subject, BanKind kind) {
    const std::optional<BanRecord> held = banOf(subject, kind);
    if (!held.has_value()) {
        return false;
    }
    wire::sql::Statement statement = database_->prepare("DELETE FROM ban WHERE subject = ? AND kind = ?");
    statement.bind(1, subject);
    statement.bind(2, storedNameOf(kind));
    const bool removed = statement.execute();
    if (removed && observer_) {
        observer_(AccessChange{AccessChange::Type::kPardoned, kind, std::string{subject}, held->name.value_or(""),
                               std::nullopt});
    }
    return removed;
}

std::vector<WhitelistEntry> AccessRepository::whitelist() {
    std::vector<WhitelistEntry> found;
    wire::sql::Statement statement =
        database_->prepare("SELECT " + std::string{kWhitelistColumns} + " FROM whitelist ORDER BY name");
    while (statement.step()) {
        found.push_back(readWhitelist(statement));
    }
    return found;
}

bool AccessRepository::isWhitelisted(std::string_view uuid) {
    wire::sql::Statement statement = database_->prepare("SELECT 1 FROM whitelist WHERE uuid = ?");
    statement.bind(1, uuid);
    return statement.step();
}

std::optional<WhitelistEntry> AccessRepository::whitelistedByName(std::string_view name) {
    wire::sql::Statement statement = database_->prepare("SELECT " + std::string{kWhitelistColumns} +
                                                        " FROM whitelist WHERE name = ? COLLATE NOCASE LIMIT 1");
    statement.bind(1, name);
    if (!statement.step()) {
        return std::nullopt;
    }
    return readWhitelist(statement);
}

void AccessRepository::addToWhitelist(const WhitelistEntry& entry) {
    wire::sql::Statement statement =
        database_->prepare("INSERT OR REPLACE INTO whitelist(uuid, name, added_at) VALUES (?, ?, ?)");
    statement.bind(1, entry.uuid);
    statement.bind(2, entry.name);
    statement.bind(3, entry.addedAtEpochSeconds);
    statement.execute();
    if (observer_) {
        observer_(
            AccessChange{AccessChange::Type::kWhitelistAdded, BanKind::kPlayer, entry.uuid, entry.name, std::nullopt});
    }
}

bool AccessRepository::removeFromWhitelist(std::string_view uuid) {
    std::string name;
    for (const WhitelistEntry& entry : observer_ ? whitelist() : std::vector<WhitelistEntry>{}) {
        if (entry.uuid == uuid) {
            name = entry.name;
        }
    }
    if (!isWhitelisted(uuid)) {
        return false;
    }
    wire::sql::Statement statement = database_->prepare("DELETE FROM whitelist WHERE uuid = ?");
    statement.bind(1, uuid);
    const bool removed = statement.execute();
    if (removed && observer_) {
        observer_(AccessChange{AccessChange::Type::kWhitelistRemoved, BanKind::kPlayer, std::string{uuid}, name,
                               std::nullopt});
    }
    return removed;
}
} // namespace basalt::wire::storage
