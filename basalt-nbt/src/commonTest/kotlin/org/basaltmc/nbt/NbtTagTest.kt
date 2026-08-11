package org.basaltmc.nbt

import kotlin.test.Test
import kotlin.test.assertEquals

class NbtTagTest {
    @Test
    fun deepCopySeparatesNestedContainersAndArrays() {
        val original =
            NbtCompound().apply {
                putByteArray("bytes", byteArrayOf(1, 2))
                putCompound("nested", NbtCompound().apply { putString("value", "original") })
                this["list"] =
                    NbtList(
                        NbtType.COMPOUND,
                        listOf(NbtCompound().apply { putIntArray("ints", intArrayOf(3, 4)) }),
                    )
            }

        val copy = original.deepCopy()
        copy.getByteArray("bytes")[0] = 9
        copy.getCompound("nested").putString("value", "changed")
        ((copy.getList("list")!![0] as NbtCompound).getIntArray("ints"))[0] = 8

        assertEquals(1, original.getByteArray("bytes")[0])
        assertEquals("original", original.getCompound("nested").getString("value"))
        assertEquals(3, ((original.getList("list")!![0] as NbtCompound).getIntArray("ints"))[0])
    }
}
