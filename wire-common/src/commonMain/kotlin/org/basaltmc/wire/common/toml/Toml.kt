package org.basaltmc.wire.common.toml

/** Thrown for malformed TOML input. */
public class TomlFormatException(
    message: String,
) : Exception(message)

/** A parsed TOML value. Tables preserve insertion order, matching how they read on the page. */
public sealed interface TomlValue {
    public data class Str(
        val value: String,
    ) : TomlValue

    public data class Int64(
        val value: Long,
    ) : TomlValue

    public data class Real(
        val value: Double,
    ) : TomlValue

    public data class Bool(
        val value: Boolean,
    ) : TomlValue

    public data class Arr(
        val values: List<TomlValue>,
    ) : TomlValue

    public class Table : TomlValue {
        private val entries = LinkedHashMap<String, TomlValue>()

        public operator fun get(key: String): TomlValue? = entries[key]

        public operator fun set(
            key: String,
            value: TomlValue,
        ) {
            entries[key] = value
        }

        public fun tableOrNull(key: String): Table? = entries[key] as? Table

        public fun stringOrNull(key: String): String? = (entries[key] as? Str)?.value

        public fun intOrNull(key: String): Long? = (entries[key] as? Int64)?.value

        public fun boolOrNull(key: String): Boolean? = (entries[key] as? Bool)?.value

        /** An array of strings, skipping any entry that is not one, so one bad element is not one bad file. */
        public fun stringListOrNull(key: String): List<String>? =
            (entries[key] as? Arr)?.values?.mapNotNull { (it as? Str)?.value }

        /** An array of integers, with the same tolerance for one bad element as [stringListOrNull]. */
        public fun intListOrNull(key: String): List<Long>? =
            (entries[key] as? Arr)?.values?.mapNotNull { (it as? Int64)?.value }

        public fun keys(): Set<String> = entries.keys
    }
}

/**
 * A hand-written parser for the subset of TOML the files under `config/` actually use: comments, `[table]`/
 * `[table.sub]` headers, bare/quoted keys, and string/integer/float/bool/array values. No
 * multi-line strings, inline tables, array-of-tables, or dates: none of those appear in
 * any of them, so they are not implemented rather than guessed at. Arrays *do* span lines, because
 * a hand-edited list of permissions or worlds is written one entry per line.
 */
public object Toml {
    public fun parse(text: String): TomlValue.Table {
        val root = TomlValue.Table()
        var current = root
        val lines = text.lines()
        var i = 0
        while (i < lines.size) {
            val line = stripComment(lines[i]).trim()
            i++
            if (line.isEmpty()) continue

            if (line.startsWith("[")) {
                // An array of tables is real TOML this parser does not do. Said out loud, because
                // taken as an ordinary header it becomes a table named "[guides]" and the file
                // quietly means something nobody wrote.
                if (line.startsWith("[[")) {
                    throw TomlFormatException(
                        "Arrays of tables are not supported here: $line. Write a table per entry " +
                            "instead, as in [guides.first] and [guides.second].",
                    )
                }
                if (!line.endsWith("]")) throw TomlFormatException("Malformed table header: $line")
                val path = line.substring(1, line.length - 1).trim()
                current = navigateToTable(root, path)
                continue
            }

            val eq = findKeyValueSeparator(line)
            val key = unquoteKey(line.substring(0, eq).trim())
            var valueText = line.substring(eq + 1).trim()

            // An array may span several lines, and in a hand-edited file it usually does: a list of
            // permissions or worlds written one per line is the readable way to write it, and is
            // what anyone adding an entry will produce. Keep taking lines until the brackets close.
            if (valueText.startsWith("[")) {
                while (bracketDepth(valueText) > 0) {
                    if (i >= lines.size) throw TomlFormatException("Unterminated array for key '$key'")
                    valueText += " " + stripComment(lines[i]).trim()
                    i++
                }
            }
            current[key] = parseValue(valueText)
        }
        return root
    }

    private fun navigateToTable(
        root: TomlValue.Table,
        dottedPath: String,
    ): TomlValue.Table {
        var table = root
        for (part in dottedPath.split('.')) {
            val key = unquoteKey(part.trim())
            table = table.tableOrNull(key) ?: TomlValue.Table().also { table[key] = it }
        }
        return table
    }

    /** How many brackets are still open, ignoring any inside strings. */
    private fun bracketDepth(text: String): Int {
        var depth = 0
        var inQuotes = false
        var quoteChar = ' '
        for (char in text) {
            when {
                inQuotes -> if (char == quoteChar) inQuotes = false
                char == '"' || char == '\'' -> {
                    inQuotes = true
                    quoteChar = char
                }
                char == '[' -> depth++
                char == ']' -> depth--
            }
        }
        return depth
    }

    private fun findKeyValueSeparator(line: String): Int {
        var inQuotes = false
        var quoteChar = ' '
        for (index in line.indices) {
            val char = line[index]
            when {
                inQuotes -> if (char == quoteChar) inQuotes = false
                char == '"' || char == '\'' -> {
                    inQuotes = true
                    quoteChar = char
                }
                char == '=' -> return index
            }
        }
        throw TomlFormatException("Expected '=' in: $line")
    }

    private fun unquoteKey(key: String): String =
        if (key.length >= 2 && (key.first() == '"' || key.first() == '\'') && key.last() == key.first()) {
            key.substring(1, key.length - 1)
        } else {
            key
        }

    private fun parseValue(text: String): TomlValue =
        when {
            text.startsWith("\"") || text.startsWith("'") -> TomlValue.Str(parseQuotedString(text))
            text.startsWith("[") -> parseArray(text)
            text == "true" -> TomlValue.Bool(true)
            text == "false" -> TomlValue.Bool(false)
            else -> parseNumber(text)
        }

    private fun parseQuotedString(text: String): String {
        val quote = text.first()
        if (text.length < 2 || text.last() != quote) throw TomlFormatException("Unterminated string: $text")
        val body = text.substring(1, text.length - 1)
        if (quote == '\'') return body // literal string: no escapes
        val sb = StringBuilder()
        var i = 0
        while (i < body.length) {
            val char = body[i]
            if (char == '\\' && i + 1 < body.length) {
                when (val next = body[i + 1]) {
                    'n' -> sb.append('\n')
                    't' -> sb.append('\t')
                    'r' -> sb.append('\r')
                    '"' -> sb.append('"')
                    '\\' -> sb.append('\\')
                    else -> throw TomlFormatException("Unknown escape '\\$next' in: $text")
                }
                i += 2
            } else {
                sb.append(char)
                i++
            }
        }
        return sb.toString()
    }

    private fun parseNumber(text: String): TomlValue {
        val long = text.toLongOrNull()
        if (long != null) return TomlValue.Int64(long)
        val double = text.toDoubleOrNull()
        if (double != null) return TomlValue.Real(double)
        throw TomlFormatException("Not a valid value: $text")
    }

    private fun parseArray(text: String): TomlValue.Arr {
        if (!text.endsWith("]")) throw TomlFormatException("Unterminated array: $text")
        val inner = text.substring(1, text.length - 1).trim()
        if (inner.isEmpty()) return TomlValue.Arr(emptyList())
        // Blank elements are dropped rather than rejected: that is TOML's trailing comma, which is
        // how a multi-line list is normally written so that adding an entry touches only one line.
        return TomlValue.Arr(
            splitArrayElements(inner).map { it.trim() }.filter { it.isNotEmpty() }.map(::parseValue),
        )
    }

    private fun splitArrayElements(inner: String): List<String> {
        val elements = mutableListOf<String>()
        var depth = 0
        var inQuotes = false
        var quoteChar = ' '
        var start = 0
        for (index in inner.indices) {
            val char = inner[index]
            when {
                inQuotes -> if (char == quoteChar) inQuotes = false
                char == '"' || char == '\'' -> {
                    inQuotes = true
                    quoteChar = char
                }
                char == '[' -> depth++
                char == ']' -> depth--
                char == ',' && depth == 0 -> {
                    elements += inner.substring(start, index)
                    start = index + 1
                }
            }
        }
        elements += inner.substring(start)
        return elements
    }

    private fun stripComment(line: String): String {
        var inQuotes = false
        var quoteChar = ' '
        for (index in line.indices) {
            val char = line[index]
            when {
                inQuotes -> if (char == quoteChar) inQuotes = false
                char == '"' || char == '\'' -> {
                    inQuotes = true
                    quoteChar = char
                }
                char == '#' -> return line.substring(0, index)
            }
        }
        return line
    }
}
