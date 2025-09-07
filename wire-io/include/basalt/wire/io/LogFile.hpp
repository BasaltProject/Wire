#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::io {

/// Mirrors the console into `<root>/latest.log`, archiving the previous run's file on the way in.
/// `latest.log` stays alone at the top of the root with the gzipped archives one level down, and `N`
/// counts runs on the same date by probing, which needs no directory read.
///
/// Nothing is buffered: after a crash every line logged up to it is worth more than the syscalls.
namespace logFile {

inline constexpr std::string_view kArchiveDirectory = "archive";
inline constexpr std::string_view kCurrentFile = "latest.log";

/// Rotates the previous run's log, applies the retention window, and starts mirroring into a fresh
/// `latest.log`, opening it with the header. A retention of 0 or less keeps archives forever.
bool install(const std::string& root, int keepDays, std::string_view header = {});

/// Stops mirroring. The sink cannot be removed on its own, so this drops every sink, which is what
/// shutdown wants anyway.
void uninstall();

/// Deletes archives whose own date is more than `keepDays` before today, read straight from the file
/// name. The names are ISO-8601, so a string comparison orders them correctly with no calendar
/// arithmetic beyond the cutoff itself.
void pruneArchives(const std::string& archiveRoot, int keepDays);

/// The date `days` before today, `yyyy-MM-dd`, in the same local zone the archive names use.
[[nodiscard]] std::string daysBeforeToday(int days);

} // namespace logFile

} // namespace basalt::wire::io
