#include <basalt/wire/io/FileIO.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace basalt::wire::io {
namespace {

std::FILE* asFile(void* handle) noexcept { return static_cast<std::FILE*>(handle); }

std::filesystem::path toPath(const std::string& path) { return std::filesystem::path{path}; }

bool flushToDisk(std::FILE* handle) noexcept {
#ifdef _WIN32
    return _commit(_fileno(handle)) == 0;
#else
    return ::fsync(::fileno(handle)) == 0;
#endif
}

} // namespace

FileWriter::FileWriter(FileWriter&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

FileWriter& FileWriter::operator=(FileWriter&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

FileWriter::~FileWriter() { close(); }

std::optional<FileWriter> FileWriter::open(const std::string& path) {
    std::FILE* handle = std::fopen(path.c_str(), "wb");
    if (handle == nullptr) {
        return std::nullopt;
    }
    return FileWriter{handle};
}

void FileWriter::write(std::span<const std::uint8_t> bytes) {
    if (handle_ == nullptr || bytes.empty()) {
        return;
    }
    std::fwrite(bytes.data(), 1, bytes.size(), asFile(handle_));
}

void FileWriter::seek(std::int64_t offset) {
    if (handle_ != nullptr) {
        std::fseek(asFile(handle_), static_cast<long>(offset), SEEK_SET);
    }
}

void FileWriter::close() noexcept {
    if (handle_ != nullptr) {
        std::fclose(asFile(handle_));
        handle_ = nullptr;
    }
}

namespace fileIO {

bool exists(const std::string& path) {
    std::error_code error;
    return std::filesystem::exists(toPath(path), error) && !error;
}

bool isDirectory(const std::string& path) {
    std::error_code error;
    return std::filesystem::is_directory(toPath(path), error) && !error;
}

bool remove(const std::string& path) {
    std::error_code error;
    return std::filesystem::remove(toPath(path), error) && !error;
}

std::vector<std::string> list(const std::string& path) {
    std::vector<std::string> names;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator{toPath(path), error}) {
        names.push_back(entry.path().filename().string());
    }
    if (error) {
        names.clear();
    }
    return names;
}

std::optional<std::uint64_t> size(const std::string& path) {
    std::error_code error;
    const auto bytes = std::filesystem::file_size(toPath(path), error);
    if (error) {
        return std::nullopt;
    }
    return bytes;
}

std::optional<std::vector<std::uint8_t>> readBytes(const std::string& path) {
    std::FILE* handle = std::fopen(path.c_str(), "rb");
    if (handle == nullptr) {
        return std::nullopt;
    }
    std::fseek(handle, 0, SEEK_END);
    const long length = std::ftell(handle);
    std::fseek(handle, 0, SEEK_SET);
    if (length <= 0) {
        std::fclose(handle);
        return std::vector<std::uint8_t>{};
    }

    std::vector<std::uint8_t> out(static_cast<std::size_t>(length));
    const std::size_t read = std::fread(out.data(), 1, out.size(), handle);
    std::fclose(handle);
    out.resize(read);
    return out;
}

std::optional<std::string> readText(const std::string& path) {
    const auto bytes = readBytes(path);
    if (!bytes) {
        return std::nullopt;
    }
    return std::string{reinterpret_cast<const char*>(bytes->data()), bytes->size()};
}

bool writeBytes(const std::string& path, std::span<const std::uint8_t> bytes) {
    // Written beside the target and moved over it, so a crash leaves the old file or the new one, never half.
    const std::string temporary = path + ".tmp";
    std::FILE* handle = std::fopen(temporary.c_str(), "wb");
    if (handle == nullptr) {
        return false;
    }
    bool written = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), handle) == bytes.size();
    written = written && std::fflush(handle) == 0 && flushToDisk(handle);
    written = std::fclose(handle) == 0 && written;
    std::error_code error;
    if (written) {
        std::filesystem::rename(toPath(temporary), toPath(path), error);
    }
    if (!written || error) {
        std::filesystem::remove(toPath(temporary), error);
        return false;
    }
    return true;
}

bool writeText(const std::string& path, std::string_view content) {
    return writeBytes(path, std::span{reinterpret_cast<const std::uint8_t*>(content.data()), content.size()});
}

bool appendText(const std::string& path, std::string_view content) {
    std::FILE* handle = std::fopen(path.c_str(), "ab");
    if (handle == nullptr) {
        return false;
    }
    const bool written = content.empty() || std::fwrite(content.data(), 1, content.size(), handle) == content.size();
    std::fclose(handle);
    return written;
}

bool createDirectories(const std::string& path) {
    std::error_code error;
    std::filesystem::create_directories(toPath(path), error);
    return !error;
}

} // namespace fileIO

} // namespace basalt::wire::io
