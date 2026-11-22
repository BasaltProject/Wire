#pragma once

#include <basalt/wire/plugins/PluginPackage.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::plugins::signature {
inline constexpr std::string_view kFile = "plugin.sig";

using PublicKey = std::array<std::uint8_t, 32>;
using Seed = std::array<std::uint8_t, 32>;
using Signature = std::array<std::uint8_t, 64>;

/// The manifest and every `.wasm`, each as path, length and bytes, in path order.
[[nodiscard]] std::vector<std::uint8_t> payload(const PluginPackage& package);

[[nodiscard]] PublicKey publicKeyOf(const Seed& seed);

[[nodiscard]] Signature sign(const Seed& seed, std::span<const std::uint8_t> message);

[[nodiscard]] std::string toHex(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> fromHex(std::string_view text, std::size_t size);

/// The text a package carries as `plugin.sig`.
[[nodiscard]] std::string render(const PublicKey& key, const Signature& signature);

/// The key that signed this package, if it is one of `trusted` and the signature holds for the modules in it.
[[nodiscard]] std::expected<PublicKey, std::string> verify(const PluginPackage& package,
                                                           std::span<const PublicKey> trusted);

/// Whether the package ships a `plugin.sig`, which a WebAssembly plugin must then satisfy.
[[nodiscard]] bool carriesSignature(const PluginPackage& package);
} // namespace basalt::wire::plugins::signature
