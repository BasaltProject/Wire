#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::io {

namespace logFile {

inline constexpr std::string_view kArchiveDirectory = "archive";
inline constexpr std::string_view kCurrentFile = "latest.log";

bool install(const std::string& root, int keepDays, std::string_view header = {});

void uninstall();

void pruneArchives(const std::string& archiveRoot, int keepDays);

[[nodiscard]] std::string daysBeforeToday(int days);

} // namespace logFile

} // namespace basalt::wire::io
