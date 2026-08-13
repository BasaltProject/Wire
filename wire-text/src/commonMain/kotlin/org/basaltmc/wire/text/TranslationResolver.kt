package org.basaltmc.wire.text

/** Thrown when a translation's format string can't be resolved: an unsupported specifier or an out-of-range argument index. */
public class TranslationFormatException(
    message: String,
) : Exception(message)

// Mirrors `TranslatableContents.FORMAT_PATTERN`: `%s`/`%N$s` (1-based positional) and `%%`, nothing else.
private val FORMAT_PATTERN = Regex("""%(?:(\d+)\$)?([A-Za-z%]|$)""")

/**
 * Flattens any component to plain text, resolving every translation in it (arguments and children
 * included) through [table]. What [PlainTextSerializer] does, except that a translatable renders as
 * the sentence a player would read rather than as its key.
 *
 * This is how the server console prints the same messages it sends to clients: the client resolves
 * keys itself against its own language, so the console has to resolve them here or print keys.
 */
public fun Component.resolvePlainText(table: LangTable): String {
    val sb = StringBuilder()
    when (val component = this) {
        is Component.Text -> sb.append(component.content)
        is Component.Translatable -> sb.append(component.resolveTemplate(table))
        is Component.Keybind -> sb.append(component.key)
        is Component.Score -> sb.append(component.name)
        is Component.Selector -> sb.append(component.pattern)
    }
    for (child in children) sb.append(child.resolvePlainText(table))
    return sb.toString()
}

/**
 * Resolves a [Component.Translatable]'s own template (not its children) into plain text using
 * [table], mirroring vanilla's `TranslatableContents.decompose`.
 *
 * parity: a template that cannot be formatted renders as the raw format string, which is what
 * vanilla does when `decomposeTemplate` throws. A message with the wrong number of arguments is a
 * bug in the caller, but it is a cosmetic one: it must never take down whatever was printing it.
 * Use [resolveTemplateStrict] to see the failure instead.
 */
public fun Component.Translatable.resolveTemplate(table: LangTable): String {
    val format = table.getOrDefault(key, fallback ?: key)
    return try {
        resolveTemplateStrict(table)
    } catch (_: TranslationFormatException) {
        format
    }
}

/**
 * [resolveTemplate] without the fallback: throws [TranslationFormatException] on an unsupported
 * specifier or a missing argument. Vanilla's format string is a small, strict subset of
 * `String.format`: `%s`, `%N$s`, and `%%` for a literal percent, and nothing else.
 */
public fun Component.Translatable.resolveTemplateStrict(table: LangTable): String {
    val format = table.getOrDefault(key, fallback ?: key)
    val result = StringBuilder()
    var replacementIndex = 0
    var current = 0

    for (match in FORMAT_PATTERN.findAll(format)) {
        val start = match.range.first
        if (start > current) result.append(format, current, start)

        val formatType = match.groupValues[2]
        if (formatType == "%" && match.value == "%%") {
            result.append('%')
        } else {
            if (formatType != "s") throw TranslationFormatException("Unsupported format: '${match.value}' in '$key'")
            val positionalIndex = match.groupValues[1]
            val index = if (positionalIndex.isNotEmpty()) positionalIndex.toInt() - 1 else replacementIndex++
            val arg =
                args.getOrNull(index) ?: throw TranslationFormatException("Invalid index $index requested for '$key'")
            result.append(arg.resolvePlainText(table))
        }
        current = match.range.last + 1
    }
    if (current < format.length) result.append(format, current, format.length)
    return result.toString()
}
