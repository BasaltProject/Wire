#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/LogFile.hpp>

#include <chrono>
#include <mutex>

namespace basalt::wire::io::logFile {
namespace {

std::mutex gWriteMutex;
std::string gPath;

std::int64_t nowEpochSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string today() { return common::Logger::formatLocalDate(nowEpochSeconds()); }

std::string nextArchivePath(const std::string& archiveRoot, const std::string& date) {
    for (int index = 1;; ++index) {
        std::string candidate = archiveRoot + '/' + date + '-' + std::to_string(index) + ".log.gz";
        if (!fileIO::exists(candidate)) {
            return candidate;
        }
    }
}

void append(std::string_view line) {
    const std::scoped_lock lock{gWriteMutex};
    if (!gPath.empty()) {
        fileIO::appendText(gPath, std::string{line} + '\n');
    }
}

} // namespace

std::string daysBeforeToday(int days) {
    return common::Logger::formatLocalDate(nowEpochSeconds() - static_cast<std::int64_t>(days) * 86400);
}

void pruneArchives(const std::string& archiveRoot, int keepDays) {
    const std::string cutoff = daysBeforeToday(keepDays);
    for (const std::string& name : fileIO::list(archiveRoot)) {
        if (name.size() < 10 || !name.ends_with(".log.gz")) {
            continue;
        }
        if (name.substr(0, 10) < cutoff) {
            fileIO::remove(archiveRoot + '/' + name);
        }
    }
}

bool install(const std::string& root, int keepDays, std::string_view header) {
    const std::string archiveRoot = root + '/' + std::string{kArchiveDirectory};
    if (!fileIO::createDirectories(archiveRoot)) {
        return false;
    }
    const std::string path = root + '/' + std::string{kCurrentFile};

    if (const auto previous = fileIO::readBytes(path); previous && !previous->empty()) {
        if (const auto archived = gzip::compress(*previous)) {
            fileIO::writeBytes(nextArchivePath(archiveRoot, today()), *archived);
        }
    }
    fileIO::remove(path);

    if (keepDays > 0) {
        pruneArchives(archiveRoot, keepDays);
    }

    {
        const std::scoped_lock lock{gWriteMutex};
        gPath = path;
    }
    // The banner and version line reach the console through plain printing, before any logger
    // exists; without this the log would open mid-startup with no idea what built it.
    if (!header.empty()) {
        append(header);
    }
    common::logOutput::addSink(append);
    return true;
}

void uninstall() {
    common::logOutput::clearSinks();
    const std::scoped_lock lock{gWriteMutex};
    gPath.clear();
}

} // namespace basalt::wire::io::logFile
