#include <basalt/wire/plugins/PluginSignature.hpp>

#include <algorithm>
#include <monocypher-ed25519.h>

namespace basalt::wire::plugins::signature {
namespace {
void appendLength(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

[[nodiscard]] std::vector<std::string> signedPaths(const PluginPackage& package) {
    std::vector<std::string> paths{std::string{packageLayout::kManifest}};
    for (const std::string& path : package.list(packageLayout::kSource)) {
        if (path.ends_with(".luac")) {
            paths.push_back(path);
        }
    }
    std::ranges::sort(paths);
    return paths;
}

[[nodiscard]] std::optional<std::string> valueOf(std::string_view text, std::string_view key) {
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        end = end == std::string_view::npos ? text.size() : end;
        std::string_view line = text.substr(at, end - at);
        at = end + 1;
        if (!line.starts_with(key)) {
            continue;
        }
        line.remove_prefix(key.size());
        const std::size_t open = line.find('"');
        const std::size_t close = line.rfind('"');
        if (line.find('=') == std::string_view::npos || open == std::string_view::npos || close <= open) {
            return std::nullopt;
        }
        return std::string{line.substr(open + 1, close - open - 1)};
    }
    return std::nullopt;
}
} // namespace

std::vector<std::uint8_t> payload(const PluginPackage& package) {
    std::vector<std::uint8_t> out;
    for (const std::string& path : signedPaths(package)) {
        const std::vector<std::uint8_t> bytes = package.read(path).value_or(std::vector<std::uint8_t>{});
        out.insert(out.end(), path.begin(), path.end());
        out.push_back(0);
        appendLength(out, bytes.size());
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    return out;
}

PublicKey publicKeyOf(const Seed& seed) {
    std::array<std::uint8_t, 64> secret{};
    PublicKey key{};
    Seed scratch = seed;
    crypto_ed25519_key_pair(secret.data(), key.data(), scratch.data());
    crypto_wipe(secret.data(), secret.size());
    return key;
}

Signature sign(const Seed& seed, const std::span<const std::uint8_t> message) {
    std::array<std::uint8_t, 64> secret{};
    PublicKey key{};
    Seed scratch = seed;
    crypto_ed25519_key_pair(secret.data(), key.data(), scratch.data());
    Signature out{};
    crypto_ed25519_sign(out.data(), secret.data(), message.data(), message.size());
    crypto_wipe(secret.data(), secret.size());
    return out;
}

std::string toHex(const std::span<const std::uint8_t> bytes) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        out.push_back(kDigits[byte >> 4]);
        out.push_back(kDigits[byte & 0xF]);
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> fromHex(const std::string_view text, const std::size_t size) {
    if (text.size() != size * 2) {
        return std::nullopt;
    }
    const auto digit = [](const char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve(size);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int high = digit(text[i]);
        const int low = digit(text[i + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        out.push_back(static_cast<std::uint8_t>(high << 4 | low));
    }
    return out;
}

std::string render(const PublicKey& key, const Signature& value) {
    return "key = \"" + toHex(key) + "\"\nsignature = \"" + toHex(value) + "\"\n";
}

bool hasBytecode(const PluginPackage& package) {
    const std::vector<std::string> files = package.list(packageLayout::kSource);
    return std::ranges::any_of(files, [](const std::string& path) { return path.ends_with(".luac"); });
}

std::expected<PublicKey, std::string> verify(const PluginPackage& package, const std::span<const PublicKey> trusted) {
    const std::optional<std::string> text = package.readText(kFile);
    if (!text.has_value()) {
        return std::unexpected(std::string{"the package has bytecode and no "} + std::string{kFile});
    }
    const auto key = fromHex(valueOf(*text, "key").value_or(""), 32);
    const auto value = fromHex(valueOf(*text, "signature").value_or(""), 64);
    if (!key.has_value() || !value.has_value()) {
        return std::unexpected(std::string{kFile} + " is malformed");
    }
    PublicKey signer{};
    std::ranges::copy(*key, signer.begin());
    if (std::ranges::find(trusted, signer) == trusted.end()) {
        return std::unexpected("the bytecode is signed with key " + toHex(signer) +
                               ", which the operator does not trust");
    }
    const std::vector<std::uint8_t> message = payload(package);
    if (crypto_ed25519_check(value->data(), signer.data(), message.data(), message.size()) != 0) {
        return std::unexpected(std::string{"the signature does not match the bytecode in the package"});
    }
    return signer;
}
} // namespace basalt::wire::plugins::signature
