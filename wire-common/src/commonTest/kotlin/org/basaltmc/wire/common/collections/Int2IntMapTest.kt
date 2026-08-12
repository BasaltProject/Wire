package org.basaltmc.wire.common.collections

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class Int2IntMapTest {
    @Test
    fun putAndGet() {
        val map = Int2IntMap()
        assertEquals(Int2IntMap.NO_VALUE, map.put(1, 100))
        assertEquals(100, map.get(1))
        assertEquals(1, map.size)
    }

    @Test
    fun missingKeyReturnsNoValue() {
        val map = Int2IntMap()
        assertEquals(Int2IntMap.NO_VALUE, map.get(42))
        assertFalse(map.containsKey(42))
        assertEquals(-1, map.getOrDefault(42, -1))
    }

    @Test
    fun updateReturnsPreviousValue() {
        val map = Int2IntMap()
        map.put(5, 1)
        assertEquals(1, map.put(5, 2))
        assertEquals(2, map.get(5))
        assertEquals(1, map.size)
    }

    @Test
    fun removeThenReinsertSameKeyWorks() {
        val map = Int2IntMap()
        map.put(5, 1)
        assertEquals(1, map.remove(5))
        assertEquals(0, map.size)
        assertFalse(map.containsKey(5))
        assertEquals(Int2IntMap.NO_VALUE, map.remove(5))

        map.put(5, 9)
        assertEquals(9, map.get(5))
    }

    @Test
    fun removalPreservesProbeChainForLaterKeys() {
        // Small initial capacity forces collisions across a handful of keys.
        val map = Int2IntMap(initialCapacity = 4)
        val keys = IntArray(6) { it * 8 } // spread out but still small table -> collisions likely
        keys.forEachIndexed { i, k -> map.put(k, i) }

        map.remove(keys[0])

        keys.drop(1).forEachIndexed { i, k ->
            assertEquals(i + 1, map.get(k), "key $k should still be found after an earlier removal")
        }
    }

    @Test
    fun growsAndKeepsAllEntries() {
        val map = Int2IntMap(initialCapacity = 4)
        repeat(200) { map.put(it, it * it) }
        assertEquals(200, map.size)
        repeat(200) { assertEquals(it * it, map.get(it)) }
    }

    @Test
    fun reservedKeysAreRejected() {
        val map = Int2IntMap()
        assertFailsWith<IllegalArgumentException> { map.put(Int.MIN_VALUE, 0) }
        assertFailsWith<IllegalArgumentException> { map.put(Int.MIN_VALUE + 1, 0) }
    }

    @Test
    fun isEmptyReflectsSize() {
        val map = Int2IntMap()
        assertTrue(map.isEmpty())
        map.put(1, 1)
        assertFalse(map.isEmpty())
        map.remove(1)
        assertTrue(map.isEmpty())
    }
}
