package org.basaltmc.wire.text

/**
 * Formatting for a [Component]. Every field is tri-state (`null` = inherited from the parent
 * component, not "off"), matching vanilla's actual inheritance model: a bold parent with a
 * plain child still renders bold unless the child explicitly sets `bold = false`.
 */
public data class Style(
    val color: TextColor? = null,
    val bold: Boolean? = null,
    val italic: Boolean? = null,
    val underlined: Boolean? = null,
    val strikethrough: Boolean? = null,
    val obfuscated: Boolean? = null,
    val font: String? = null,
    val insertion: String? = null,
    val clickEvent: ClickEvent? = null,
    val hoverEvent: HoverEvent? = null,
) {
    public val isEmpty: Boolean
        get() = this == EMPTY

    /** Returns a style with every field of [other] overlaid on this one where [other] sets it. */
    public fun merge(other: Style): Style =
        Style(
            color = other.color ?: color,
            bold = other.bold ?: bold,
            italic = other.italic ?: italic,
            underlined = other.underlined ?: underlined,
            strikethrough = other.strikethrough ?: strikethrough,
            obfuscated = other.obfuscated ?: obfuscated,
            font = other.font ?: font,
            insertion = other.insertion ?: insertion,
            clickEvent = other.clickEvent ?: clickEvent,
            hoverEvent = other.hoverEvent ?: hoverEvent,
        )

    public companion object {
        public val EMPTY: Style = Style()
    }
}
