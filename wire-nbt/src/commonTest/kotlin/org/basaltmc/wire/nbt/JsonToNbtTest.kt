package org.basaltmc.wire.nbt

import kotlinx.serialization.json.Json
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class JsonToNbtTest {
    private fun convert(json: String): NbtTag = JsonToNbt.convert(Json.parseToJsonElement(json))

    @Test
    fun objectsBecomeCompoundsAndKeepTheirTypes() {
        val compound = convert("""{"anvil_cost":1,"weight":2.5,"name":"sharpness","cursed":true}""") as NbtCompound
        assertEquals(1, (compound["anvil_cost"] as NbtInt).value)
        assertEquals(2.5, (compound["weight"] as NbtDouble).value)
        assertEquals("sharpness", (compound["name"] as NbtString).value)
        assertEquals(1, (compound["cursed"] as NbtByte).value)
    }

    @Test
    fun arraysBecomeListsOfOneType() {
        val list = convert("""["mainhand","offhand"]""") as NbtList
        assertEquals(NbtType.STRING, list.elementType)
        assertEquals(2, list.size)
    }

    @Test
    fun anEmptyArrayIsAnEmptyList() {
        val list = convert("[]") as NbtList
        assertEquals(NbtType.END, list.elementType)
        assertEquals(0, list.size)
    }

    @Test
    fun aMixedArrayIsRefusedRatherThanCoerced() {
        assertFailsWith<NbtConversionException> { convert("""[1,"two"]""") }
    }

    @Test
    fun nullFieldsAreDroppedTheWayAnOptionalCodecExpects() {
        val compound = convert("""{"a":1,"b":null}""") as NbtCompound
        assertTrue("b" !in compound.keys)
    }

    @Test
    fun nestedStructuresSurviveWhole() {
        val compound =
            convert("""{"effects":{"minecraft:damage":[{"effect":{"type":"minecraft:add"}}]}}""") as NbtCompound
        val effects = compound["effects"] as NbtCompound
        val damage = effects["minecraft:damage"] as NbtList
        val first = damage[0] as NbtCompound
        assertEquals("minecraft:add", ((first["effect"] as NbtCompound)["type"] as NbtString).value)
    }
}
