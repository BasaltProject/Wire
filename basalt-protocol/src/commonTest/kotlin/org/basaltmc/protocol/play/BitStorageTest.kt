package org.basaltmc.protocol.play

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class BitStorageTest {
    @Test
    fun roundTripsEveryValueAtFourBits() {
        val storage = BitStorage(bitsPerEntry = 4, size = 20)
        for (i in 0 until 20) storage[i] = i % 16
        for (i in 0 until 20) assertEquals(i % 16, storage[i])
    }

    @Test
    fun entriesDoNotSpanTwoLongs() {
        // 5 bits/entry: 64 / 5 = 12 entries per long, 4 bits of padding left over each long.
        val storage = BitStorage(bitsPerEntry = 5, size = 13)
        for (i in 0 until 13) storage[i] = 31 - (i % 32)
        for (i in 0 until 13) assertEquals(31 - (i % 32), storage[i])
        assertEquals(2, storage.data.size)
    }

    @Test
    fun zeroBitsPerEntryAlwaysReadsZero() {
        val storage = BitStorage(bitsPerEntry = 0, size = 4096)
        assertEquals(0, storage[0])
        assertEquals(0, storage[4095])
        assertEquals(0, storage.data.size)
    }

    @Test
    fun rejectsAValueThatDoesNotFit() {
        val storage = BitStorage(bitsPerEntry = 4, size = 1)
        assertFailsWith<IllegalArgumentException> { storage[0] = 16 }
    }

    @Test
    fun rejectsAnOutOfBoundsIndex() {
        val storage = BitStorage(bitsPerEntry = 4, size = 4)
        assertFailsWith<IllegalArgumentException> { storage[4] }
        assertFailsWith<IllegalArgumentException> { storage[-1] }
    }

    @Test
    fun wrapsExistingDataInsteadOfAllocatingFresh() {
        val data = longArrayOf(0x12L)
        val storage = BitStorage(bitsPerEntry = 4, size = 1, data = data)
        assertEquals(0x2, storage[0])
    }
}
