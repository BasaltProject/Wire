#pragma once

#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace basalt::wire::text {

enum class TranslationError : std::uint8_t {
    UnsupportedSpecifier,
    ArgumentOutOfRange,
};

[[nodiscard]] std::string resolvePlainText(const Component& component, const LangTable& table);

[[nodiscard]] Component resolveComponent(const Component& component, const LangTable& table);

/// parity: a template that cannot be formatted renders as the raw format string, as when decomposeTemplate throws.
[[nodiscard]] std::string resolveTemplate(const Component::Translatable& translatable, const LangTable& table);

[[nodiscard]] std::expected<std::string, TranslationError>
resolveTemplateStrict(const Component::Translatable& translatable, const LangTable& table);

} // namespace basalt::wire::text
