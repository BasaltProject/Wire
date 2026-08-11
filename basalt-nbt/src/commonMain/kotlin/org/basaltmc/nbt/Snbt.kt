package org.basaltmc.nbt

/**
 * Stringified NBT ("SNBT"): the human-readable form used in commands (`/give`, `/data`).
 * [stringify] produces a self-consistent canonical form (explicit `b`/`s`/`L`/`f`/`d` suffixes,
 * bare integers for [NbtInt]) that [parse] reads back exactly; it is not guaranteed to be
 * byte-identical to vanilla's own formatter, which is a cosmetic difference the game does not
 * observe since SNBT is re-parsed into the same tag tree, not compared as text.
 */
public object Snbt {
    private val UNQUOTED_KEY_CHARS = ('a'..'z') + ('A'..'Z') + ('0'..'9') + listOf('_', '-', '.', '+')

    public fun stringify(tag: NbtTag): String = StringBuilder().also { appendTag(it, tag) }.toString()

    private fun appendTag(
        sb: StringBuilder,
        tag: NbtTag,
    ) {
        when (tag) {
            is NbtEnd -> Unit
            is NbtByte -> sb.append(tag.value).append('b')
            is NbtShort -> sb.append(tag.value).append('s')
            is NbtInt -> sb.append(tag.value)
            is NbtLong -> sb.append(tag.value).append('L')
            is NbtFloat -> sb.append(tag.value).append('f')
            is NbtDouble -> sb.append(tag.value).append('d')
            is NbtString -> appendQuotedString(sb, tag.value)
            is NbtByteArray -> {
                sb.append("[B;")
                tag.value.forEachIndexed { i, v ->
                    if (i > 0) sb.append(',')
                    sb.append(v).append('b')
                }
                sb.append(']')
            }
            is NbtIntArray -> {
                sb.append("[I;")
                tag.value.forEachIndexed { i, v ->
                    if (i > 0) sb.append(',')
                    sb.append(v)
                }
                sb.append(']')
            }
            is NbtLongArray -> {
                sb.append("[L;")
                tag.value.forEachIndexed { i, v ->
                    if (i > 0) sb.append(',')
                    sb.append(v).append('L')
                }
                sb.append(']')
            }
            is NbtList -> {
                sb.append('[')
                tag.toList().forEachIndexed { i, element ->
                    if (i > 0) sb.append(',')
                    appendTag(sb, element)
                }
                sb.append(']')
            }
            is NbtCompound -> {
                sb.append('{')
                var first = true
                for ((key, value) in tag) {
                    if (!first) sb.append(',')
                    first = false
                    appendKey(sb, key)
                    sb.append(':')
                    appendTag(sb, value)
                }
                sb.append('}')
            }
        }
    }

    private fun appendKey(
        sb: StringBuilder,
        key: String,
    ) {
        if (key.isNotEmpty() && key.all { it in UNQUOTED_KEY_CHARS }) {
            sb.append(key)
        } else {
            appendQuotedString(sb, key)
        }
    }

    private fun appendQuotedString(
        sb: StringBuilder,
        value: String,
    ) {
        sb.append('"')
        for (char in value) {
            when (char) {
                '"' -> sb.append("\\\"")
                '\\' -> sb.append("\\\\")
                else -> sb.append(char)
            }
        }
        sb.append('"')
    }

    public fun parse(text: String): NbtTag {
        val parser = Parser(text)
        parser.skipWhitespace()
        val tag = parser.parseValue()
        parser.skipWhitespace()
        if (!parser.atEnd) throw NbtFormatException("Trailing data in SNBT at index ${parser.position}")
        return tag
    }

    private class Parser(
        private val text: String,
    ) {
        var position: Int = 0
            private set

        val atEnd: Boolean get() = position >= text.length

        private fun peek(): Char {
            if (atEnd) throw NbtFormatException("Unexpected end of SNBT input")
            return text[position]
        }

        fun skipWhitespace() {
            while (!atEnd && text[position].isWhitespace()) position++
        }

        private fun expect(char: Char) {
            if (atEnd || text[position] != char) {
                throw NbtFormatException("Expected '$char' at index $position in SNBT input")
            }
            position++
        }

        fun parseValue(): NbtTag {
            skipWhitespace()
            return when (peek()) {
                '{' -> parseCompound()
                '[' -> parseListOrArray()
                '"', '\'' -> NbtString(parseQuotedString())
                else -> parseUnquoted()
            }
        }

        private fun parseCompound(): NbtCompound {
            expect('{')
            val compound = NbtCompound()
            skipWhitespace()
            if (!atEnd && peek() == '}') {
                position++
                return compound
            }
            while (true) {
                skipWhitespace()
                val key = if (peek() == '"' || peek() == '\'') parseQuotedString() else parseUnquotedKey()
                skipWhitespace()
                expect(':')
                skipWhitespace()
                compound[key] = parseValue()
                skipWhitespace()
                when (peek()) {
                    ',' -> {
                        position++
                    }
                    '}' -> {
                        position++
                        return compound
                    }
                    else -> throw NbtFormatException("Expected ',' or '}' at index $position in SNBT input")
                }
            }
        }

        private fun parseListOrArray(): NbtTag {
            expect('[')
            if (!atEnd && (peek() == 'B' || peek() == 'I' || peek() == 'L') && lookaheadIsArrayPrefix()) {
                val kind = peek()
                position += 2 // type letter + ';'
                return parseTypedArray(kind)
            }
            skipWhitespace()
            val elements = mutableListOf<NbtTag>()
            if (!atEnd && peek() == ']') {
                position++
                return NbtList(NbtType.END, elements)
            }
            while (true) {
                skipWhitespace()
                elements += parseValue()
                skipWhitespace()
                when (peek()) {
                    ',' -> position++
                    ']' -> {
                        position++
                        return NbtList(elements.first().type, elements)
                    }
                    else -> throw NbtFormatException("Expected ',' or ']' at index $position in SNBT input")
                }
            }
        }

        private fun lookaheadIsArrayPrefix(): Boolean = position + 1 < text.length && text[position + 1] == ';'

        private fun parseTypedArray(kind: Char): NbtTag {
            skipWhitespace()
            val values = mutableListOf<String>()
            if (!atEnd && peek() == ']') {
                position++
            } else {
                while (true) {
                    skipWhitespace()
                    values += parseUnquotedToken()
                    skipWhitespace()
                    when (peek()) {
                        ',' -> position++
                        ']' -> {
                            position++
                            break
                        }
                        else -> throw NbtFormatException("Expected ',' or ']' at index $position in SNBT input")
                    }
                }
            }
            return when (kind) {
                'B' -> NbtByteArray(values.map { it.trimEnd('b', 'B').toByte() }.toByteArray())
                'I' -> NbtIntArray(values.map { it.toInt() }.toIntArray())
                'L' -> NbtLongArray(values.map { it.trimEnd('l', 'L').toLong() }.toLongArray())
                else -> throw NbtFormatException("Unknown typed array prefix '$kind'")
            }
        }

        private fun parseUnquotedKey(): String {
            val start = position
            while (!atEnd && text[position] in UNQUOTED_KEY_CHARS) position++
            if (position == start) throw NbtFormatException("Expected a compound key at index $position")
            return text.substring(start, position)
        }

        private fun parseUnquotedToken(): String {
            val start = position
            while (!atEnd && (text[position] in UNQUOTED_KEY_CHARS)) position++
            if (position == start) throw NbtFormatException("Expected a value at index $position")
            return text.substring(start, position)
        }

        private fun parseUnquoted(): NbtTag {
            val token = parseUnquotedToken()
            return parseNumberOrKeyword(token)
                ?: throw NbtFormatException("Invalid unquoted SNBT value: '$token'")
        }

        private fun parseNumberOrKeyword(token: String): NbtTag? {
            if (token == "true") return NbtByte.TRUE
            if (token == "false") return NbtByte.FALSE

            val suffix = token.lastOrNull()
            val hasTypeSuffix = suffix != null && suffix.lowercaseChar() in "bsldf" && token.length > 1
            val numberPart = if (hasTypeSuffix) token.dropLast(1) else token
            val looksNumeric =
                numberPart.isNotEmpty() &&
                    numberPart.all { it.isDigit() || it == '.' || it == '+' || it == '-' } &&
                    numberPart.count { it == '.' } <= 1 &&
                    numberPart.any { it.isDigit() }
            if (!looksNumeric) return NbtString(token)

            return try {
                when (suffix?.lowercaseChar()) {
                    'b' -> NbtByte(numberPart.toByte())
                    's' -> NbtShort(numberPart.toShort())
                    'l' -> NbtLong(numberPart.toLong())
                    'f' -> NbtFloat(numberPart.toFloat())
                    'd' -> NbtDouble(numberPart.toDouble())
                    else -> if ('.' in numberPart) NbtDouble(numberPart.toDouble()) else NbtInt(numberPart.toInt())
                }
            } catch (_: NumberFormatException) {
                NbtString(token)
            }
        }

        private fun parseQuotedString(): String {
            val quote = peek()
            position++
            val sb = StringBuilder()
            while (true) {
                if (atEnd) throw NbtFormatException("Unterminated string in SNBT input")
                val char = text[position++]
                when {
                    char == '\\' && !atEnd -> {
                        sb.append(text[position])
                        position++
                    }
                    char == quote -> return sb.toString()
                    else -> sb.append(char)
                }
            }
        }
    }
}
