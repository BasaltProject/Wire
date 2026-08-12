package org.basaltmc.wire.text

/**
 * Parser for the MiniMessage tag markup (`<red>Hello</red>`), the format BSMod plugins use to
 * author text. Covers colors (named and `#rrggbb`),
 * decorations, `<reset>`, `<click:...>`/`<hover:show_text:...>`, and two-or-more-stop
 * `<gradient:...>` (applied to the literal text up to the matching `</gradient>`, without
 * recursing into nested tags inside it). Not implemented: `<rainbow>`, `<transition>`,
 * `<insert>`, `<keybind>`, `<translate>` tags, and escaped quotes inside tag arguments; these are
 * deferred.
 */
public object MiniMessage {
    public fun parse(input: String): Component = Parser(input).parseUntil()

    /** Applies a linear color gradient across [text], one stop per character. */
    public fun gradient(
        text: String,
        vararg stops: TextColor,
    ): Component {
        if (stops.size < 2 || text.isEmpty()) return Component.text(text)
        val segments = text.length - 1
        val children =
            text.mapIndexed { index, char ->
                val fraction = if (segments == 0) 0f else index.toFloat() / segments
                val scaled = fraction * (stops.size - 1)
                val segmentIndex = scaled.toInt().coerceIn(0, stops.size - 2)
                val localFraction = scaled - segmentIndex
                val color = TextColor.lerp(stops[segmentIndex], stops[segmentIndex + 1], localFraction)
                Component.text(char.toString(), color)
            }
        return Component.Text("", children = children)
    }

    private data class OpenTag(
        val name: String,
        val style: Style,
    )

    private class Parser(
        private val text: String,
    ) {
        private var position = 0
        private val styleStack = ArrayDeque<OpenTag>()

        fun parseUntil(): Component {
            val parts = mutableListOf<Component>()
            val literal = StringBuilder()

            fun flushLiteral() {
                if (literal.isNotEmpty()) {
                    parts += Component.Text(literal.toString(), currentStyle())
                    literal.clear()
                }
            }

            while (position < text.length) {
                val char = text[position]
                if (char == '\\' && position + 1 < text.length && text[position + 1] == '<') {
                    literal.append('<')
                    position += 2
                    continue
                }
                if (char != '<') {
                    literal.append(char)
                    position++
                    continue
                }

                val tagEnd = text.indexOf('>', position)
                if (tagEnd == -1) {
                    literal.append(char)
                    position++
                    continue
                }
                val tagContent = text.substring(position + 1, tagEnd)

                if (tagContent.startsWith("/")) {
                    val name = tagContent.substring(1).trim().lowercase()
                    flushLiteral()
                    popTag(name)
                    position = tagEnd + 1
                    continue
                }

                val tagNameParts = splitTagArguments(tagContent)
                if (tagNameParts.first().lowercase() == "gradient") {
                    flushLiteral()
                    position = tagEnd + 1
                    parts += parseGradientBlock(tagNameParts.drop(1))
                    continue
                }

                flushLiteral()
                position = tagEnd + 1
                applyTag(tagContent)
            }

            flushLiteral()
            return joinSiblings(parts)
        }

        private fun parseGradientBlock(colorArgs: List<String>): Component {
            val stops = colorArgs.mapNotNull { TextColor.parse(it) }
            val closeIndex = text.indexOf("</gradient>", position)
            val inner = if (closeIndex == -1) text.substring(position) else text.substring(position, closeIndex)
            position = if (closeIndex == -1) text.length else closeIndex + "</gradient>".length
            val base = if (stops.size >= 2) gradient(inner, *stops.toTypedArray()) else Component.text(inner)
            return base.withStyle(currentStyle().merge(base.style))
        }

        private fun joinSiblings(parts: List<Component>): Component {
            if (parts.isEmpty()) return Component.empty()
            var result = parts.first()
            for (i in 1 until parts.size) result = result.append(parts[i])
            return result
        }

        private fun currentStyle(): Style = styleStack.lastOrNull()?.style ?: Style.EMPTY

        private fun pushTag(
            name: String,
            style: Style,
        ) {
            styleStack.addLast(OpenTag(name, currentStyle().merge(style)))
        }

        private fun popTag(name: String) {
            if (name.isEmpty()) {
                if (styleStack.isNotEmpty()) styleStack.removeLast()
                return
            }
            val index = styleStack.indexOfLast { it.name == name }
            if (index >= 0) {
                while (styleStack.size > index) styleStack.removeLast()
            }
        }

        private fun applyTag(rawTag: String) {
            val parts = splitTagArguments(rawTag)
            val name = parts.first().lowercase()
            val args = parts.drop(1)

            when (name) {
                "reset" -> styleStack.clear()
                "bold", "b" -> pushTag(name, Style(bold = true))
                "italic", "i", "em" -> pushTag(name, Style(italic = true))
                "underlined", "u" -> pushTag(name, Style(underlined = true))
                "strikethrough", "st" -> pushTag(name, Style(strikethrough = true))
                "obfuscated", "obf" -> pushTag(name, Style(obfuscated = true))
                "color", "colour", "c" -> {
                    val color = args.firstOrNull()?.let { TextColor.parse(it) }
                    pushTag(name, Style(color = color))
                }
                "click" -> {
                    val action = args.getOrNull(0)?.let { ClickEvent.Action.fromId(it) }
                    val value = args.getOrNull(1)
                    if (action != null && value != null) pushTag(name, Style(clickEvent = ClickEvent(action, value)))
                }
                "hover" -> {
                    val action = args.getOrNull(0)
                    val value = args.getOrNull(1)
                    if (action == "show_text" && value != null) {
                        pushTag(name, Style(hoverEvent = HoverEvent.ShowText(Component.text(value))))
                    }
                }
                else -> {
                    val color = TextColor.named(name) ?: (if (name.startsWith("#")) TextColor.fromHex(name) else null)
                    if (color != null) pushTag(name, Style(color = color))
                }
            }
        }

        private fun splitTagArguments(raw: String): List<String> {
            val result = mutableListOf<String>()
            val current = StringBuilder()
            var quote: Char? = null
            for (char in raw) {
                when {
                    quote != null -> {
                        if (char == quote) quote = null else current.append(char)
                    }
                    char == '\'' || char == '"' -> quote = char
                    char == ':' -> {
                        result += current.toString()
                        current.clear()
                    }
                    else -> current.append(char)
                }
            }
            result += current.toString()
            return result
        }
    }
}
