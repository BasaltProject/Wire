package org.basaltmc.wire.common.toml

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class TomlTest {
    @Test
    fun parsesTopLevelScalars() {
        val table =
            Toml.parse(
                """
                name = "basalt"
                port = 25565
                pi = 3.5
                enabled = true
                disabled = false
                """.trimIndent(),
            )
        assertEquals("basalt", table.stringOrNull("name"))
        assertEquals(25565L, table.intOrNull("port"))
        assertEquals(true, table.boolOrNull("enabled"))
        assertEquals(false, table.boolOrNull("disabled"))
    }

    @Test
    fun parsesNestedTables() {
        val table =
            Toml.parse(
                """
                [server]
                port = 25565

                [world.overworld]
                name = "overworld"
                """.trimIndent(),
            )
        assertEquals(25565L, table.tableOrNull("server")?.intOrNull("port"))
        assertEquals("overworld", table.tableOrNull("world")?.tableOrNull("overworld")?.stringOrNull("name"))
    }

    @Test
    fun ignoresCommentsAndBlankLines() {
        val table =
            Toml.parse(
                """
                # a top-level comment
                port = 25565 # trailing comment

                [server] # section comment
                motd = "hi" # another one
                """.trimIndent(),
            )
        assertEquals(25565L, table.intOrNull("port"))
        assertEquals("hi", table.tableOrNull("server")?.stringOrNull("motd"))
    }

    @Test
    fun handlesEscapesInBasicStrings() {
        val table = Toml.parse("""s = "line one\nline two\ttabbed \"quoted\"" """)
        assertEquals("line one\nline two\ttabbed \"quoted\"", table.stringOrNull("s"))
    }

    @Test
    fun literalStringsDoNotProcessEscapes() {
        val table = Toml.parse("""s = 'no \n escapes here'""")
        assertEquals("no \\n escapes here", table.stringOrNull("s"))
    }

    @Test
    fun parsesArraysOfPrimitives() {
        val table = Toml.parse("""nums = [1, 2, 3]""")
        val arr = table["nums"] as TomlValue.Arr
        assertEquals(listOf(1L, 2L, 3L), arr.values.map { (it as TomlValue.Int64).value })
    }

    @Test
    fun aCommaInsideAQuotedArrayElementIsNotASeparator() {
        val table = Toml.parse("""strs = ["a, b", "c"]""")
        val arr = table["strs"] as TomlValue.Arr
        assertEquals(listOf("a, b", "c"), arr.values.map { (it as TomlValue.Str).value })
    }

    @Test
    fun missingKeysReturnNullRatherThanThrowing() {
        val table = Toml.parse("port = 25565")
        assertNull(table.stringOrNull("missing"))
        assertNull(table.tableOrNull("missing"))
    }

    @Test
    fun malformedTableHeaderThrows() {
        assertFailsWith<TomlFormatException> { Toml.parse("[server") }
    }

    @Test
    fun missingEqualsThrows() {
        assertFailsWith<TomlFormatException> { Toml.parse("just some words") }
    }
}

class MultiLineArrayTest {
    @Test
    fun anArrayMaySpanSeveralLinesWithCommentsBetween() {
        // The way anyone hand-edits a list: one entry per line, and a note next to one of them.
        val root =
            Toml.parse(
                """
                [groups.operator]
                permissions = [
                    "basalt.command.*",   # everything
                    "-basalt.command.stop",
                ]
                level = 4
                """.trimIndent(),
            )
        val group = root.tableOrNull("groups")?.tableOrNull("operator")

        assertEquals(listOf("basalt.command.*", "-basalt.command.stop"), group?.stringListOrNull("permissions"))
        // The key after the array still parses: the reader has to resume in the right place.
        assertEquals(4L, group?.intOrNull("level"))
    }

    @Test
    fun anArrayThatNeverClosesIsRejected() {
        assertFailsWith<TomlFormatException> { Toml.parse("permissions = [\n  \"a\",\n") }
    }
}
