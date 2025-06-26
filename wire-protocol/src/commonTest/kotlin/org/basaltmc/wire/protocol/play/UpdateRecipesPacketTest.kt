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
            listOf(StonecutterOption(inputItemIds = listOf(1, 2), resultItemId = 7, resultCount = 2)),
        ).write(buffer)

        assertEquals(0, buffer.readVarInt(), "no item property sets")
        assertEquals(1, buffer.readVarInt(), "one stonecutter option")
        // parity: `ByteBufCodecs.holderSet` writes the size plus one, since zero means a tag.
        assertEquals(3, buffer.readVarInt())
        // parity: `holderRegistry` writes a plain registry id, with nothing added to it.
        assertEquals(1, buffer.readVarInt(), "the first item")
        assertEquals(2, buffer.readVarInt())
        assertEquals(UpdateRecipesPacket.ITEM_STACK_SLOT_DISPLAY, buffer.readVarInt())
        // parity: `ItemStackTemplate.STREAM_CODEC`: the item first, then the count, then a patch.
        assertEquals(7, buffer.readVarInt(), "the result's item")
        assertEquals(2, buffer.readVarInt(), "and its count")
        assertEquals(0, buffer.readVarInt(), "no components added")
        assertEquals(0, buffer.readVarInt(), "none removed")
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
