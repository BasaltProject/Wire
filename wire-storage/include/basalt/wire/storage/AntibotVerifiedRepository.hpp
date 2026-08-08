#pragma once

#include <basalt/wire/sql/Sqlite.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::storage {

struct VerifiedRow {
    std::string name;
    std::string origin;
    std::int64_t expiresAtEpochSeconds = 0;
};

class AntibotVerifiedRepository {
public:
    explicit AntibotVerifiedRepository(wire::sql::Database& database) : database_(&database) {}

    void upsert(std::string_view name, std::string_view origin, std::int64_t expiresAtEpochSeconds);

    /// Removes every origin of `name`.
    void remove(std::string_view name);

    /// Rows that have not expired at `nowEpochSeconds`, latest expiry first, at most `limit`.
    [[nodiscard]] std::vector<VerifiedRow> live(std::int64_t nowEpochSeconds, std::int32_t limit);

    /// Deletes at most `limit` expired rows and returns how many went.
    std::int32_t purgeExpired(std::int64_t nowEpochSeconds, std::int32_t limit);

    /// Keeps the `keep` rows that expire last and deletes the rest; returns how many went.
    std::int32_t trimTo(std::int32_t keep);

    [[nodiscard]] std::int64_t count();

private:
    wire::sql::Database* database_;
};

} // namespace basalt::wire::storage
