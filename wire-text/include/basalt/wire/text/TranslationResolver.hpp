#pragma once

#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace basalt::wire::text {

/// Why a translation's format string could not be resolved.
enum class TranslationError : std::uint8_t {
    UnsupportedSpecifier,
    ArgumentOutOfRange,
};

/// Flattens any component to plain text, resolving every translation in it (arguments and children
/// included) through the table. What `toPlainText` does, except that a translatable renders as the
/// sentence a player would read rather than as its key.
///
/// This is how the server console prints the same messages it sends to clients: a client resolves
/// keys itself against its own language, so the console resolves them here or prints keys.
[[nodiscard]] std::string resolvePlainText(const Component& component, const LangTable& table);

/// Resolves a translatable's own template, not its children.
///
/// parity: a template that cannot be formatted renders as the raw format string, which is what
/// vanilla does when `decomposeTemplate` throws. Wrong argument counts are a caller bug, but a
/// cosmetic one: it must never take down whatever was printing the message.
[[nodiscard]] std::string resolveTemplate(const Component::Translatable& translatable, const LangTable& table);

/// resolveTemplate without the fallback. Vanilla's format string is a small, strict subset of
/// `String.format`: `%s`, `%N$s` and `%%`, and nothing else.
[[nodiscard]] std::expected<std::string, TranslationError>
resolveTemplateStrict(const Component::Translatable& translatable, const LangTable& table);

} // namespace basalt::wire::text
