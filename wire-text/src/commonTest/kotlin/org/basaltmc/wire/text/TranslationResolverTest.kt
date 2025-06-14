package org.basaltmc.wire.text

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class TranslationResolverTest {
    @Test
    fun missingKeyFallsBackToTheKeyItself() {
        val translatable = Component.Translatable("some.missing.key")
        assertEquals("some.missing.key", translatable.resolvePlainText(LangTable.EMPTY))
    }

    @Test
    fun missingKeyPrefersAnExplicitFallbackOverTheKey() {
        val translatable = Component.Translatable("some.missing.key", fallback = "Fallback text")
        assertEquals("Fallback text", translatable.resolvePlainText(LangTable.EMPTY))
    }

    @Test
    fun sequentialPercentSSubstitutesArgsInOrder() {
        val table = LangTable.parse("""{"greet": "Hello, %s! You have %s new messages."}""")
        val translatable = Component.Translatable("greet", args = listOf(Component.text("Steve"), Component.text("3")))
        assertEquals("Hello, Steve! You have 3 new messages.", translatable.resolvePlainText(table))
    }

    @Test
    fun positionalArgsCanBeReorderedOrRepeated() {
        val table = LangTable.parse("""{"swap": "%2${'$'}s before %1${'$'}s"}""")
        val translatable =
            Component.Translatable(
                "swap",
                args = listOf(Component.text("second"), Component.text("first")),
            )
        assertEquals("first before second", translatable.resolvePlainText(table))
    }

    @Test
    fun doublePercentIsALiteralPercent() {
        val table = LangTable.parse("""{"pct": "100%%"}""")
        val translatable = Component.Translatable("pct")
        assertEquals("100%", translatable.resolvePlainText(table))
    }

    @Test
    fun anUnsupportedFormatSpecifierThrowsInStrictMode() {
        val table = LangTable.parse("""{"bad": "%d apples"}""")
        assertFailsWith<TranslationFormatException> {
            Component.Translatable("bad", args = listOf(Component.text("3"))).resolveTemplateStrict(table)
        }
    }

    @Test
    fun anOutOfRangeArgumentIndexThrowsInStrictMode() {
        val table = LangTable.parse("""{"needsArg": "%s"}""")
        assertFailsWith<TranslationFormatException> {
            Component.Translatable("needsArg").resolveTemplateStrict(table)
        }
    }

    @Test
    fun aTemplateThatCannotBeFormattedRendersAsItsFormatString() {
        // parity: vanilla catches the same failure and shows the raw string, so a message with too
        // few arguments is ugly rather than fatal to whatever was printing it.
        val table = LangTable.parse("""{"needsArg": "wanted %s and %s"}""")
        assertEquals(
            "wanted %s and %s",
            Component.Translatable("needsArg", args = listOf(Component.text("one"))).resolvePlainText(table),
        )
    }

    @Test
    fun argsAreFlattenedRecursivelyThroughPlainTextSerialization() {
        val table = LangTable.parse("""{"nested": "%s"}""")
        val nestedArg = Component.text("outer").append(Component.text(" inner"))
        assertEquals("outer inner", Component.Translatable("nested", args = listOf(nestedArg)).resolvePlainText(table))
    }

    @Test
    fun resolvesEveryTranslationInATreeIncludingNestedArguments() {
        val table =
            LangTable.parse(
                """
                {
                  "chat.type.admin": "[%s: %s]",
                  "commands.gamemode.success.self": "Set own game mode to %s",
                  "gameMode.creative": "Creative Mode"
                }
                """.trimIndent(),
            )
        val message =
            Component.translatable(
                "chat.type.admin",
                Component.text("Whiron"),
                Component.translatable(
                    "commands.gamemode.success.self",
                    Component.translatable("gameMode.creative"),
                ),
            )

        assertEquals("[Whiron: Set own game mode to Creative Mode]", message.resolvePlainText(table))
    }

    @Test
    fun anEmptyTableLeavesTheKeysThemselves() {
        val message = Component.translatable("commands.seed.success", Component.text("42"))
        assertEquals("commands.seed.success", message.resolvePlainText(LangTable.EMPTY))
    }
}
