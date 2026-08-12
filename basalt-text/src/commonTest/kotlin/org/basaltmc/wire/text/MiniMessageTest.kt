package org.basaltmc.wire.text

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class MiniMessageTest {
    @Test
    fun parsesSimpleColorTag() {
        val component = MiniMessage.parse("<red>Hello</red>")
        assertEquals("Hello", PlainTextSerializer.serialize(component))
        assertEquals(TextColor.RED, firstText(component)?.style?.color)
    }

    @Test
    fun parsesNestedTags() {
        val component = MiniMessage.parse("<red><bold>Hi</bold> there</red>")
        assertEquals("Hi there", PlainTextSerializer.serialize(component))
    }

    @Test
    fun parsesHexColor() {
        val component = MiniMessage.parse("<#112233>Hi</#112233>")
        assertEquals(TextColor(0x112233), firstText(component)?.style?.color)
    }

    @Test
    fun parsesClickAndHoverTags() {
        val component = MiniMessage.parse("<click:run_command:'/help'><hover:show_text:'tip'>Go</hover></click>")
        val text = firstText(component)
        assertNotNull(text)
        assertEquals(ClickEvent.runCommand("/help"), text.style.clickEvent)
        assertEquals(HoverEvent.ShowText(Component.text("tip")), text.style.hoverEvent)
    }

    @Test
    fun parsesTextOutsideTags() {
        val component = MiniMessage.parse("plain <red>red</red> plain again")
        assertEquals("plain red plain again", PlainTextSerializer.serialize(component))
    }

    @Test
    fun gradientColorsEachCharacterAcrossStops() {
        val component = MiniMessage.gradient("abc", TextColor.RED, TextColor.BLUE)
        assertEquals("abc", PlainTextSerializer.serialize(component))
        assertEquals(
            TextColor.RED,
            (component as Component.Text)
                .children
                .first()
                .style.color,
        )
        assertEquals(
            TextColor.BLUE,
            component.children
                .last()
                .style.color,
        )
    }

    @Test
    fun parsesGradientTag() {
        val component = MiniMessage.parse("<gradient:red:blue>abc</gradient>")
        assertEquals("abc", PlainTextSerializer.serialize(component))
    }

    @Test
    fun escapedAngleBracketIsLiteral() {
        // A bare, unmatched '>' is just literal text; only '<' needs escaping to avoid being
        // read as the start of a tag.
        val component = MiniMessage.parse("\\<not a tag>")
        assertEquals("<not a tag>", PlainTextSerializer.serialize(component))
    }

    private fun firstText(component: Component): Component.Text? =
        when (component) {
            is Component.Text ->
                if (component.content.isNotEmpty()) {
                    component
                } else {
                    component.children.firstOrNull()?.let(
                        ::firstText,
                    )
                }
            else -> component.children.firstOrNull()?.let(::firstText)
        }
}
