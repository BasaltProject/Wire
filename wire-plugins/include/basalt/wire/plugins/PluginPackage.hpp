#pragma once

#include <basalt/wire/io/ZipReader.hpp>
#include <basalt/wire/plugins/PluginManifest.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::plugins {
struct PluginLoadError {
    std::string message;
};

namespace packageLayout {
inline constexpr std::string_view kManifest = PluginManifest::kFileName;
inline constexpr std::string_view kSource = "src/";
inline constexpr std::string_view kResources = "resources/";
inline constexpr std::string_view kConfigFile = "resources/config.toml";
inline constexpr std::string_view kConfigDirectory = "resources/config/";
inline constexpr std::string_view kLang = "resources/lang/";
inline constexpr std::string_view kAssets = "resources/assets/";

[[nodiscard]] std::optional<std::string> moduleFile(std::string_view module);
} // namespace packageLayout

class PluginPackage {
public:
    PluginPackage(const PluginPackage&) = delete;
    PluginPackage& operator=(const PluginPackage&) = delete;
    virtual ~PluginPackage() = default;

    [[nodiscard]] const std::string& origin() const noexcept { return origin_; }

    [[nodiscard]] virtual bool editable() const noexcept = 0;

    [[nodiscard]] virtual std::optional<std::string> workingFolder() const { return std::nullopt; }

    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> read(std::string_view path) const = 0;

    [[nodiscard]] virtual std::vector<std::string> list(std::string_view prefix) const = 0;

    [[nodiscard]] std::optional<std::string> readText(std::string_view path) const;

    [[nodiscard]] bool exists(std::string_view path) const { return read(path).has_value(); }

protected:
    explicit PluginPackage(std::string origin) : origin_(std::move(origin)) {}

private:
    std::string origin_;
};

class FolderPackage final : public PluginPackage {
public:
    explicit FolderPackage(std::string root);

    [[nodiscard]] bool editable() const noexcept override { return true; }

    [[nodiscard]] std::optional<std::string> workingFolder() const override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path) const override;

    [[nodiscard]] std::vector<std::string> list(std::string_view prefix) const override;

private:
    void walk(const std::string& relative, std::vector<std::string>& into) const;

    std::string root_;
};

class ArchivePackage final : public PluginPackage {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<ArchivePackage>, PluginLoadError>
    open(std::string path, std::vector<std::uint8_t> bytes);

    [[nodiscard]] bool editable() const noexcept override { return false; }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path) const override;

    [[nodiscard]] std::vector<std::string> list(std::string_view prefix) const override;

private:
    ArchivePackage(std::string path, wire::io::ZipReader zip);

    wire::io::ZipReader zip_;
};

[[nodiscard]] std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError>
discoverPackages(const std::string& directory, std::string_view extension);
} // namespace basalt::wire::plugins
