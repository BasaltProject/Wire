package org.basaltmc.common.collections

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class Long2ObjectMapTest {
    @Test
    fun putAndGet() {
        val map = Long2ObjectMap<String>()
        assertNull(map.put(1L, "a"))
        assertEquals("a", map.get(1L))
        assertEquals(1, map.size)
    }

    @Test
    fun missingKeyReturnsNull() {
        val map = Long2ObjectMap<String>()
        assertNull(map.get(42L))
        assertFalse(map.containsKey(42L))
    }

    @Test
    fun updateReturnsPreviousValue() {
        val map = Long2ObjectMap<String>()
        map.put(5L, "a")
        assertEquals("a", map.put(5L, "b"))
        assertEquals("b", map.get(5L))
        assertEquals(1, map.size)
    }

    @Test
    fun removeThenReinsertSameKeyWorks() {
        val map = Long2ObjectMap<String>()
        map.put(5L, "a")
        assertEquals("a", map.remove(5L))
        assertEquals(0, map.size)
        assertFalse(map.containsKey(5L))
        assertNull(map.remove(5L))

        map.put(5L, "z")
        assertEquals("z", map.get(5L))
    }

    @Test
    fun removalPreservesProbeChainForLaterKeys() {
        val map = Long2ObjectMap<Int>(initialCapacity = 4)
        val keys = LongArray(6) { it * 8L }
        keys.forEachIndexed { i, k -> map.put(k, i) }

        map.remove(keys[0])

        keys.drop(1).forEachIndexed { i, k ->
            assertEquals(i + 1, map.get(k), "key $k should still be found after an earlier removal")
        }
    }

    @Test
    fun growsAndKeepsAllEntries() {
        val map = Long2ObjectMap<Int>(initialCapacity = 4)
        repeat(200) { map.put(it.toLong(), it * it) }
        assertEquals(200, map.size)
        repeat(200) { assertEquals(it * it, map.get(it.toLong())) }
    }

    @Test
    fun reservedKeysAreRejected() {
        val map = Long2ObjectMap<Int>()
        assertFailsWith<IllegalArgumentException> { map.put(Long.MIN_VALUE, 0) }
        assertFailsWith<IllegalArgumentException> { map.put(Long.MIN_VALUE + 1, 0) }
    }

    @Test
    fun isEmptyReflectsSize() {
        val map = Long2ObjectMap<Int>()
        assertTrue(map.isEmpty())
        map.put(1L, 1)
        assertFalse(map.isEmpty())
        map.remove(1L)
        assertTrue(map.isEmpty())
    }
}
