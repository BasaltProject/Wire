#pragma once

#include <basalt/wire/sql/Sqlite.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::storage {
class PluginDataRepository {
public:
    explicit PluginDataRepository(wire::sql::Database& database) : database_(&database) {}

    [[nodiscard]] std::optional<std::string> get(std::string_view plugin, std::string_view uuid, std::string_view key);

    [[nodiscard]] std::map<std::string, std::string, std::less<>> all(std::string_view plugin, std::string_view uuid);

    void set(std::string_view plugin, std::string_view uuid, std::string_view key,
             const std::optional<std::string>& value);

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> top(std::string_view plugin, std::string_view key,
                                                                       std::int32_t limit);

    [[nodiscard]] std::map<std::string, std::int64_t, std::less<>> namespaces();

    void purge(std::string_view plugin);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> holder(std::string_view kind, std::string_view id);

    void setHolder(std::string_view kind, std::string_view id, std::span<const std::uint8_t> data);

private:
    wire::sql::Database* database_;
};
} // namespace basalt::wire::storage
