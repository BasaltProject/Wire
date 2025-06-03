package org.basaltmc.text

/**
 * An immutable, styled piece of text. Mirrors Adventure's `Component` model design (MIT-licensed):
 * every public API that shows text to a player accepts a [Component], never a formatted [String].
 * Legacy `§` formatting is never used as the internal representation.
 *
 * `nbt` and `object` component variants are not implemented yet (dynamic `/data`-query text and
 * embedded map-icon text respectively); both are rare enough to defer.
 */
public sealed class Component {
    public abstract val style: Style
    public abstract val children: List<Component>

    public data class Text(
        val content: String,
        override val style: Style = Style.EMPTY,
        override val children: List<Component> = emptyList(),
    ) : Component()

    public data class Translatable(
        val key: String,
        val args: List<Component> = emptyList(),
        val fallback: String? = null,
        override val style: Style = Style.EMPTY,
        override val children: List<Component> = emptyList(),
    ) : Component()

    public data class Keybind(
        val key: String,
        override val style: Style = Style.EMPTY,
        override val children: List<Component> = emptyList(),
    ) : Component()

    public data class Score(
        val name: String,
        val objective: String,
        override val style: Style = Style.EMPTY,
        override val children: List<Component> = emptyList(),
    ) : Component()

    public data class Selector(
        val pattern: String,
        val separator: Component? = null,
        override val style: Style = Style.EMPTY,
        override val children: List<Component> = emptyList(),
    ) : Component()

    public fun withStyle(newStyle: Style): Component = withStyleAndChildren(newStyle, children)

    /** The same component with different children, for rewriting a tree without knowing its shape. */
    public fun withChildren(newChildren: List<Component>): Component = withStyleAndChildren(style, newChildren)

    public fun append(child: Component): Component = withStyleAndChildren(style, children + child)

    public fun color(color: TextColor?): Component = withStyle(style.copy(color = color))

    public fun bold(value: Boolean = true): Component = withStyle(style.copy(bold = value))

    public fun italic(value: Boolean = true): Component = withStyle(style.copy(italic = value))

    public fun underlined(value: Boolean = true): Component = withStyle(style.copy(underlined = value))

    public fun strikethrough(value: Boolean = true): Component = withStyle(style.copy(strikethrough = value))

    public fun obfuscated(value: Boolean = true): Component = withStyle(style.copy(obfuscated = value))

    public fun clickEvent(event: ClickEvent?): Component = withStyle(style.copy(clickEvent = event))

    public fun hoverEvent(event: HoverEvent?): Component = withStyle(style.copy(hoverEvent = event))

    private fun withStyleAndChildren(
        newStyle: Style,
        newChildren: List<Component>,
    ): Component =
        when (this) {
            is Text -> copy(style = newStyle, children = newChildren)
            is Translatable -> copy(style = newStyle, children = newChildren)
            is Keybind -> copy(style = newStyle, children = newChildren)
            is Score -> copy(style = newStyle, children = newChildren)
            is Selector -> copy(style = newStyle, children = newChildren)
        }

    public companion object {
        public fun text(content: String): Component = Text(content)

        public fun text(
            content: String,
            color: TextColor,
        ): Component = Text(content, Style(color = color))

        public fun translatable(
            key: String,
            vararg args: Component,
        ): Component = Translatable(key, args.toList())

        public fun keybind(key: String): Component = Keybind(key)

        public fun score(
            name: String,
            objective: String,
        ): Component = Score(name, objective)

        public fun empty(): Component = Text("")

        public fun join(
            separator: Component,
            components: List<Component>,
        ): Component {
            if (components.isEmpty()) return empty()
            val result = mutableListOf<Component>()
            components.forEachIndexed { index, component ->
                if (index > 0) result += separator
                result += component
            }
            return Text("", children = result)
        }
    }
}
