package org.basaltmc.wire.text

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ComponentTest {
    @Test
    fun styleMergePrefersOtherWhenSet() {
        val base = Style(color = TextColor.RED, bold = true)
        val overlay = Style(color = TextColor.BLUE, italic = true)
        val merged = base.merge(overlay)

        assertEquals(TextColor.BLUE, merged.color)
        assertEquals(true, merged.bold)
        assertEquals(true, merged.italic)
    }

    @Test
    fun styleMergeKeepsBaseWhenOtherIsNull() {
        val base = Style(color = TextColor.RED, bold = true)
        val overlay = Style()
        val merged = base.merge(overlay)

        assertEquals(TextColor.RED, merged.color)
        assertEquals(true, merged.bold)
    }

    @Test
    fun builderHelpersProduceExpectedShape() {
        val component =
            Component
                .text("hello", TextColor.GREEN)
                .bold()
                .append(Component.text(" world"))

        assertTrue(component is Component.Text)
        assertEquals("hello", (component as Component.Text).content)
        assertEquals(TextColor.GREEN, component.style.color)
        assertEquals(true, component.style.bold)
        assertEquals(1, component.children.size)
        assertEquals(" world", (component.children.first() as Component.Text).content)
    }

    @Test
    fun joinInsertsSeparatorBetweenElements() {
        val joined =
            Component.join(
                Component.text(", "),
                listOf(Component.text("a"), Component.text("b"), Component.text("c")),
            )
        assertEquals("a, b, c", PlainTextSerializer.serialize(joined))
    }

    @Test
    fun colorParsesNamedAndHex() {
        assertEquals(TextColor.RED, TextColor.parse("red"))
        assertEquals(TextColor(0x123456), TextColor.parse("#123456"))
        assertNull(TextColor.parse("not-a-color"))
    }
}
