#include "Text.hpp"

#include <array>
#include <charconv>
#include <cstring>

namespace basalt::wire::wasm::trace {
namespace {
using common::toml::TomlTable;
using common::toml::TomlValue;

[[nodiscard]] const TomlValue* descend(const TomlTable& table, std::string_view key) {
    if (const TomlValue* direct = table.find(key)) {
        return direct;
    }
    const std::size_t dot = key.find('.');
    if (dot == std::string_view::npos) {
        return nullptr;
    }
    const TomlValue* head = table.find(key.substr(0, dot));
    const TomlTable* nested = head != nullptr ? head->get<TomlTable>() : nullptr;
    return nested != nullptr ? descend(*nested, key.substr(dot + 1)) : nullptr;
}

template <typename T>
[[nodiscard]] std::string shortest(T value) {
    std::array<char, 40> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string{buffer.data(), result.ptr};
}
} // namespace

std::string quote(std::string_view text) {
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string out = "\"";
    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\') {
            out += '\\';
            out += character;
        } else if (character == '\n') {
            out += "\\n";
        } else if (character == '\t') {
            out += "\\t";
        } else if (character == '\r') {
            out += "\\r";
        } else if (byte < 0x20 || byte == 0x7f) {
            out += "\\x";
            out += kHex[byte >> 4];
            out += kHex[byte & 0xF];
        } else {
            out += character;
        }
    }
    out += '"';
    return out;
}

std::string number(double value) { return shortest(value); }

std::string number(float value) { return shortest(value); }

std::optional<ArgList> readArgs(const GuestMemory& memory, std::int32_t ptr, std::int32_t len) {
    if (len == 0) {
        return ArgList{};
    }
    const std::optional<std::string_view> bytes = memory.string(ptr, len);
    if (!bytes) {
        return std::nullopt;
    }
    std::size_t at = 0;
    auto readU32 = [&](std::uint32_t& out) {
        if (bytes->size() - at < 4) {
            return false;
        }
        std::memcpy(&out, bytes->data() + at, 4);
        at += 4;
        return true;
    };
    auto readText = [&](std::string& out) {
        std::uint32_t length = 0;
        if (!readU32(length) || bytes->size() - at < length) {
            return false;
        }
        out.assign(bytes->data() + at, length);
        at += length;
        return true;
    };
    std::uint32_t count = 0;
    if (!readU32(count)) {
        return std::nullopt;
    }
    ArgList args;
    for (std::uint32_t index = 0; index < count; ++index) {
        std::string key;
        std::string value;
        if (!readText(key) || !readText(value)) {
            return std::nullopt;
        }
        args.emplace_back(std::move(key), std::move(value));
    }
    if (at != bytes->size()) {
        return std::nullopt;
    }
    return args;
}

std::string describe(const std::optional<ArgList>& args) {
    if (!args) {
        return "<malformed>";
    }
    std::string out = "{";
    for (std::size_t index = 0; index < args->size(); ++index) {
        out += (index == 0 ? "" : ", ") + (*args)[index].first + "=" + quote((*args)[index].second);
    }
    return out + "}";
}

std::optional<std::string> langTemplate(const World& world, std::string_view key) {
    const TomlValue* value = descend(world.lang, key);
    const std::string* text = value != nullptr ? value->get<std::string>() : nullptr;
    return text != nullptr ? std::optional<std::string>{*text} : std::nullopt;
}

std::string substitute(std::string_view text, const ArgList& args) {
    std::string out;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t open = text.find('{', at);
        if (open == std::string_view::npos) {
            out.append(text.substr(at));
            break;
        }
        out.append(text.substr(at, open - at));
        const std::size_t close = text.find('}', open);
        bool replaced = false;
        if (close != std::string_view::npos) {
            const std::string_view key = text.substr(open + 1, close - open - 1);
            for (const auto& [name, value] : args) {
                if (name == key) {
                    out += value;
                    replaced = true;
                    break;
                }
            }
            if (replaced) {
                at = close + 1;
                continue;
            }
        }
        out += '{';
        at = open + 1;
    }
    return out;
}

std::optional<std::string> render(const World& world, std::string_view text, const ArgList& args) {
    if (text.starts_with('@')) {
        const std::optional<std::string> found = langTemplate(world, text.substr(1));
        if (!found) {
            return std::nullopt;
        }
        return substitute(*found, args);
    }
    return substitute(text, args);
}

const common::toml::TomlValue* configValue(const World& world, std::string_view key) {
    return descend(world.config, key);
}

} // namespace basalt::wire::wasm::trace
