package org.basaltmc.wire.nbt

import kotlin.test.Test
import kotlin.test.assertEquals

class SnbtTest {
    @Test
    fun parsesPrimitives() {
        assertEquals(NbtByte(5), Snbt.parse("5b"))
        assertEquals(NbtShort(5), Snbt.parse("5s"))
        assertEquals(NbtInt(5), Snbt.parse("5"))
        assertEquals(NbtInt(-42), Snbt.parse("-42"))
        assertEquals(NbtLong(5L), Snbt.parse("5L"))
        assertEquals(NbtLong(5L), Snbt.parse("5l"))
        assertEquals(NbtFloat(3.5f), Snbt.parse("3.5f"))
        assertEquals(NbtDouble(3.5), Snbt.parse("3.5d"))
        assertEquals(NbtDouble(3.5), Snbt.parse("3.5"))
        assertEquals(NbtByte.TRUE, Snbt.parse("true"))
        assertEquals(NbtByte.FALSE, Snbt.parse("false"))
    }

    @Test
    fun parsesQuotedAndUnquotedStrings() {
        assertEquals(NbtString("hello"), Snbt.parse("hello"))
        assertEquals(NbtString("hello world"), Snbt.parse("\"hello world\""))
        assertEquals(NbtString("with \"quote\""), Snbt.parse("\"with \\\"quote\\\"\""))
        assertEquals(NbtString("single"), Snbt.parse("'single'"))
    }

    @Test
    fun parsesCompoundsAndLists() {
        val parsed = Snbt.parse("{a:1,b:\"two\",c:[1,2,3],d:{e:5b}}") as NbtCompound
        assertEquals(NbtInt(1), parsed["a"])
        assertEquals(NbtString("two"), parsed["b"])
        assertEquals(listOf(NbtInt(1), NbtInt(2), NbtInt(3)), (parsed["c"] as NbtList).toList())
        assertEquals(NbtByte(5), (parsed["d"] as NbtCompound)["e"])
    }

    @Test
    fun parsesTypedArrays() {
        assertEquals(
            listOf<Byte>(1, 2, 3),
            (Snbt.parse("[B;1b,2b,3b]") as NbtByteArray).value.toList(),
        )
        assertEquals(listOf(1, 2, 3), (Snbt.parse("[I;1,2,3]") as NbtIntArray).value.toList())
        assertEquals(listOf(1L, 2L, 3L), (Snbt.parse("[L;1L,2L,3L]") as NbtLongArray).value.toList())
        assertEquals(emptyList<Int>(), (Snbt.parse("[I;]") as NbtIntArray).value.toList())
    }

    @Test
    fun stringifyThenParseRoundTripsComplexTree() {
        val root = NbtCompound()
        root.putByte("byte", -12)
        root.putShort("short", 1234)
        root.putInt("int", -70000)
        root.putLong("long", 1234567890123L)
        root.putFloat("float", 3.25f)
        root.putDouble("double", 2.71828)
        root.putString("plainKey", "unquoted value works because it matches the key charset")
        root.putString("key with spaces", "needs quoting")
        root.putIntArray("ints", intArrayOf(1, -2, 3))
        root["list"] = NbtList(NbtType.STRING, listOf(NbtString("a"), NbtString("b")))

        val nested = NbtCompound().also { it.putBoolean("flag", true) }
        root["nested"] = nested

        val text = Snbt.stringify(root)
        val reparsed = Snbt.parse(text)

        assertEquals(root, reparsed)
    }

    @Test
    fun parsesNestedListsOfCompounds() {
        val parsed = Snbt.parse("[{id:1},{id:2},{id:3}]") as NbtList
        assertEquals(3, parsed.size)
        assertEquals(NbtInt(2), (parsed[1] as NbtCompound)["id"])
    }
}
