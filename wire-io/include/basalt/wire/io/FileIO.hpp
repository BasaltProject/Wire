#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::io {

/// An open file being written sequentially, with the ability to go back and patch what was already
/// written. A format whose header declares the length of what follows (Linear's compressed size,
/// for one) can then be streamed straight to disk: write the header with a hole, stream the body,
/// seek back and fill the hole in. The alternative is building the whole body in memory first, and
/// for a region that is tens of megabytes.
class FileWriter {
public:
    FileWriter() = default;
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    FileWriter(FileWriter&& other) noexcept;
    FileWriter& operator=(FileWriter&& other) noexcept;
    ~FileWriter();

    /// Opens the path for writing, truncating it.
    [[nodiscard]] static std::optional<FileWriter> open(const std::string& path);

    [[nodiscard]] bool isOpen() const noexcept { return handle_ != nullptr; }

    void write(std::span<const std::uint8_t> bytes);

    void seek(std::int64_t offset);

    void close() noexcept;

private:
    explicit FileWriter(void* handle) : handle_(handle) {}

    void* handle_ = nullptr;
};

/// Plain file access for the files that live outside the world and plugin data models: config, the
/// EULA, the asset cache, log files.
namespace fileIO {

[[nodiscard]] bool exists(const std::string& path);

[[nodiscard]] bool isDirectory(const std::string& path);

/// Removes the path, reporting whether it was there to remove.
bool remove(const std::string& path);

/// Entry names directly inside the path, without recursion and without `.` or `..`. Empty when the
/// path is not a readable directory.
[[nodiscard]] std::vector<std::string> list(const std::string& path);

/// Byte length without reading the contents.
[[nodiscard]] std::optional<std::uint64_t> size(const std::string& path);

[[nodiscard]] std::optional<std::string> readText(const std::string& path);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> readBytes(const std::string& path);

bool writeText(const std::string& path, std::string_view content);

bool writeBytes(const std::string& path, std::span<const std::uint8_t> bytes);

/// Appends, creating the file if needed: what the log file writes through.
bool appendText(const std::string& path, std::string_view content);

/// `mkdir -p`: creates the path and any missing parents.
bool createDirectories(const std::string& path);

} // namespace fileIO

} // namespace basalt::wire::io
