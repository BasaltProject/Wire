#pragma once

#include <basalt/wire/sql/Sqlite.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::storage {

struct PlayerSummary {
    std::string uuid;
    std::string name;
    std::int64_t value = 0;
};

/// The columns every `player` table has, whether it is the full one of a server or the lean one of a proxy.
class PlayerIdentityRepository {
public:
    explicit PlayerIdentityRepository(wire::sql::Database& database) : database_(&database) {}

    /// Upserts name and last_seen; only valid on the lean table of `schema::identityDdl()`.
    void recordLogin(std::string_view uuid, std::string_view name, std::int64_t nowEpochSeconds);

    [[nodiscard]] bool exists(std::string_view uuid);

    [[nodiscard]] std::optional<std::string> groupOf(std::string_view uuid);

    bool setGroup(std::string_view uuid, const std::optional<std::string>& group);

    void setLastIp(std::string_view uuid, const std::optional<std::string>& address);

    [[nodiscard]] std::optional<std::string> findNameByAddress(std::string_view address);

    [[nodiscard]] std::optional<std::string> findUuidByName(std::string_view name);

    [[nodiscard]] std::vector<PlayerSummary> findInGroup(std::string_view group);

    [[nodiscard]] std::vector<PlayerSummary> findLastSeenBefore(std::int64_t epochSeconds);

    void remove(std::string_view uuid);

    [[nodiscard]] std::int64_t count();

protected:
    wire::sql::Database* database_;
};

} // namespace basalt::wire::storage
