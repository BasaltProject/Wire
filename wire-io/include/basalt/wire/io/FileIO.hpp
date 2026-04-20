#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::io {

class FileWriter {
public:
    FileWriter() = default;
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    FileWriter(FileWriter&& other) noexcept;
    FileWriter& operator=(FileWriter&& other) noexcept;
    ~FileWriter();

    [[nodiscard]] static std::optional<FileWriter> open(const std::string& path);

    [[nodiscard]] bool isOpen() const noexcept { return handle_ != nullptr; }

    void write(std::span<const std::uint8_t> bytes);

    void seek(std::int64_t offset);

    void close() noexcept;

private:
    explicit FileWriter(void* handle) : handle_(handle) {}

    void* handle_ = nullptr;
};

namespace fileIO {

[[nodiscard]] bool exists(const std::string& path);

[[nodiscard]] bool isDirectory(const std::string& path);

bool remove(const std::string& path);

[[nodiscard]] std::vector<std::string> list(const std::string& path);

[[nodiscard]] std::optional<std::uint64_t> size(const std::string& path);

[[nodiscard]] std::optional<std::string> readText(const std::string& path);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> readBytes(const std::string& path);

bool writeText(const std::string& path, std::string_view content);

bool writeBytes(const std::string& path, std::span<const std::uint8_t> bytes);

bool appendText(const std::string& path, std::string_view content);

bool createDirectories(const std::string& path);

} // namespace fileIO

} // namespace basalt::wire::io
