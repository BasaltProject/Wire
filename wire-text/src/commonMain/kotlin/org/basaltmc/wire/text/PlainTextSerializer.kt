package org.basaltmc.wire.text

/** Flattens a [Component] tree to a plain string, dropping all styling. */
public object PlainTextSerializer {
    public fun serialize(component: Component): String {
        val sb = StringBuilder()
        appendFlattened(sb, component)
        return sb.toString()
    }

    private fun appendFlattened(
        sb: StringBuilder,
        component: Component,
    ) {
        when (component) {
            is Component.Text -> sb.append(component.content)
            is Component.Translatable -> sb.append(component.fallback ?: component.key)
            is Component.Keybind -> sb.append(component.key)
            is Component.Score -> sb.append(component.name)
            is Component.Selector -> sb.append(component.pattern)
        }
        for (child in component.children) appendFlattened(sb, child)
    }
}
