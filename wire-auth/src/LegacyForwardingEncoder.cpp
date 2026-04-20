#include <basalt/wire/auth/forwarding/LegacyForwarding.hpp>

namespace basalt::wire::auth::forwarding::legacy {

std::optional<std::string> encodeBounded(std::string_view host, std::string_view address, const common::Uuid& profileId,
                                         const std::vector<SessionProfileProperty>& properties, std::size_t maxBytes,
                                         std::string_view tokenProperty, std::string_view token) {
    if (host.empty() || host.size() > 255 || address.empty() || address.size() > 64 ||
        host.find('\0') != std::string_view::npos || address.find('\0') != std::string_view::npos ||
        properties.size() > 64 || tokenProperty.empty() != token.empty() || tokenProperty.size() > 256 ||
        token.size() > 4096 || maxBytes > 32767)
        return std::nullopt;
    std::string out;
    const auto append = [&](std::string_view text) {
        if (text.size() > maxBytes - out.size())
            return false;
        out.append(text);
        return true;
    };
    const auto quoted = [&](std::string_view text) {
        if (!append("\""))
            return false;
        constexpr char kHex[] = "0123456789abcdef";
        for (const char raw : text) {
            const auto c = static_cast<unsigned char>(raw);
            if (c == '"' || c == '\\') {
                const char escape[]{'\\', static_cast<char>(c)};
                if (!append(std::string_view{escape, 2}))
                    return false;
            } else if (c < 32) {
                const char escape[]{'\\', 'u', '0', '0', kHex[c >> 4], kHex[c & 15]};
                if (!append(std::string_view{escape, 6}))
                    return false;
            } else {
                const char value = static_cast<char>(c);
                if (!append(std::string_view{&value, 1}))
                    return false;
            }
        }
        return append("\"");
    };
    bool first = true;
    const auto property = [&](std::string_view name, std::string_view value, std::string_view signature) {
        if (!first && !append(","))
            return false;
        first = false;
        return append("{\"name\":") && quoted(name) && append(",\"value\":") && quoted(value) &&
               append(",\"signature\":") && quoted(signature) && append("}");
    };
    if (!append(host) || !append(std::string_view{"\0", 1}) || !append(address) || !append(std::string_view{"\0", 1}) ||
        !append(profileId.toUndashedString()) || !append(std::string_view{"\0", 1}) || !append("["))
        return std::nullopt;
    for (const auto& entry : properties) {
        if (entry.name.size() > 256 || entry.value.size() > 32768 ||
            (entry.signature && entry.signature->size() > 8192) ||
            (!tokenProperty.empty() && entry.name == tokenProperty) ||
            !property(entry.name, entry.value,
                      entry.signature ? std::string_view{*entry.signature} : std::string_view{}))
            return std::nullopt;
    }
    if ((!tokenProperty.empty() && !property(tokenProperty, token, {})) || !append("]"))
        return std::nullopt;
    return out;
}

} // namespace basalt::wire::auth::forwarding::legacy
