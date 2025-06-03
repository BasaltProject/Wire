package org.basaltmc.wire.common

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class IdentifierTest {
    @Test
    fun toStringJoinsNamespaceAndPath() {
        assertEquals("minecraft:stone", Identifier("minecraft", "stone").toString())
    }

    @Test
    fun parseSplitsOnColon() {
        assertEquals(Identifier("basalt", "some_block"), Identifier.parse("basalt:some_block"))
    }

    @Test
    fun parseDefaultsToVanillaNamespace() {
        assertEquals(Identifier("minecraft", "stone"), Identifier.parse("stone"))
    }

    @Test
    fun vanillaHelperUsesTheMinecraftNamespace() {
        assertEquals(Identifier("minecraft", "stone"), Identifier.vanilla("stone"))
    }

    @Test
    fun equalityIsByNamespaceAndPath() {
        assertEquals(Identifier("minecraft", "stone"), Identifier("minecraft", "stone"))
    }

    @Test
    fun parseRejectsMultipleColons() {
        assertFailsWith<IdentifierFormatException> { Identifier.parse("a:b:c") }
    }

    @Test
    fun parseRejectsEmptyNamespaceOrPath() {
        assertFailsWith<IdentifierFormatException> { Identifier.parse(":stone") }
        assertFailsWith<IdentifierFormatException> { Identifier.parse("minecraft:") }
    }
}
