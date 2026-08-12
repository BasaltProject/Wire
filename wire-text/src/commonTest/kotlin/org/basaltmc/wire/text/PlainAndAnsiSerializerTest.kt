package org.basaltmc.wire.text

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class PlainAndAnsiSerializerTest {
    @Test
    fun plainSerializerFlattensChildrenAndDropsStyle() {
        val component =
            Component
                .text("Hello ", TextColor.RED)
                .bold()
                .append(Component.text("World"))
        assertEquals("Hello World", PlainTextSerializer.serialize(component))
    }

    @Test
    fun plainSerializerUsesFallbackForTranslatable() {
        val component = Component.Translatable("some.key", fallback = "Some Fallback")
        assertEquals("Some Fallback", PlainTextSerializer.serialize(component))
    }

    @Test
    fun ansiSerializerEmitsEscapeCodesForColor() {
        val component = Component.text("hi", TextColor.RED)
        val rendered = AnsiSerializer.serialize(component)
        val esc = 0x1B.toChar()
        assertTrue(rendered.contains("$esc[38;2;255;85;85m"))
        assertTrue(rendered.contains("hi"))
        assertTrue(rendered.endsWith("$esc[0m"))
    }

    @Test
    fun ansiSerializerProducesPlainOutputWhenUnstyled() {
        val rendered = AnsiSerializer.serialize(Component.text("plain"))
        assertEquals("plain", rendered)
    }
}
