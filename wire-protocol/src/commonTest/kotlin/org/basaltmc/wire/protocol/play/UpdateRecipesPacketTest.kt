package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readVarInt
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `ClientboundUpdateRecipesPacket`'s own stream codec, field by field. */
class UpdateRecipesPacketTest {
    @Test
    fun itWritesWhatTheStonecutterScreenReads() {
        val buffer = ByteBuffer()
        UpdateRecipesPacket(
            listOf(StonecutterOption(inputItemIds = listOf(1, 2), result = WireItemStack(itemId = 7, count = 2))),
        ).write(buffer)

        assertEquals(0, buffer.readVarInt(), "no item property sets")
        assertEquals(1, buffer.readVarInt(), "one stonecutter option")
        // parity: `ByteBufCodecs.holderSet` writes the size plus one, since zero means a tag.
        assertEquals(3, buffer.readVarInt())
        // parity: `holderRegistry` writes a plain registry id, with nothing added to it.
        assertEquals(1, buffer.readVarInt(), "the first item")
        assertEquals(2, buffer.readVarInt())
        assertEquals(UpdateRecipesPacket.ITEM_STACK_SLOT_DISPLAY, buffer.readVarInt())
        assertEquals(2, buffer.readVarInt(), "the result's count")
        assertEquals(7, buffer.readVarInt(), "and its item")
    }

    /** An empty list still has to say so: the client reads both fields either way. */
    @Test
    fun itWritesTheEmptyCaseWhole() {
        val buffer = ByteBuffer()
        UpdateRecipesPacket(emptyList()).write(buffer)

        assertEquals(0, buffer.readVarInt())
        assertEquals(0, buffer.readVarInt())
    }
}
