package org.basaltmc.wire.text

/** Renders a [Component] tree as ANSI-escaped text, for terminal/console output. */
public object AnsiSerializer {
    private const val ESC = "["
    private const val RESET = "${ESC}0m"

    public fun serialize(component: Component): String {
        val sb = StringBuilder()
        var stylesApplied = false
        appendStyled(sb, component, Style.EMPTY) { stylesApplied = true }
        if (stylesApplied) sb.append(RESET)
        return sb.toString()
    }

    private fun appendStyled(
        sb: StringBuilder,
        component: Component,
        inherited: Style,
        onStyled: () -> Unit,
    ) {
        val effective = inherited.merge(component.style)
        if (!effective.isEmpty) {
            sb.append(ansiCodes(effective))
            onStyled()
        }
        when (component) {
            is Component.Text -> sb.append(component.content)
            is Component.Translatable -> sb.append(component.fallback ?: component.key)
            is Component.Keybind -> sb.append(component.key)
            is Component.Score -> sb.append(component.name)
            is Component.Selector -> sb.append(component.pattern)
        }
        for (child in component.children) appendStyled(sb, child, effective, onStyled)
    }

    private fun ansiCodes(style: Style): String {
        val codes = mutableListOf<String>()
        style.color?.let { color -> codes += "38;2;${color.red};${color.green};${color.blue}" }
        if (style.bold == true) codes += "1"
        if (style.italic == true) codes += "3"
        if (style.underlined == true) codes += "4"
        if (style.obfuscated == true) codes += "5"
        if (style.strikethrough == true) codes += "9"
        return "$ESC${codes.joinToString(";")}m"
    }
}
