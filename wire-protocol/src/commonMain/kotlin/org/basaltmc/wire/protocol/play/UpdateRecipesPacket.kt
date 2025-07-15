package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/** One row of a stonecutter's list: what goes in, and the stack the button shows. */
public data class StonecutterOption(
    val inputItemIds: List<Int>,
    val resultItemId: Int,
    val resultCount: Int,
)

/**
 * Clientbound, id 0x85: the recipes a client needs to have itself. Mirrors
 * `ClientboundUpdateRecipesPacket`.
 *
 * The stonecutter is why it exists: the client draws its buttons from this list and sends back an
 * index into it. The item property sets are the recipe book's highlighting and go out empty.
 */
public data class UpdateRecipesPacket(
    val stonecutter: List<StonecutterOption>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(0)
        buffer.writeVarInt(stonecutter.size)
        for (option in stonecutter) {
            // parity: `ByteBufCodecs.holderSet` writes the size plus one, since zero would mean a
            // tag follows instead. The items themselves are plain registry ids: `holderRegistry`
            // writes the id and nothing else, and adding one to it shifted every ingredient by a
            // place, which is why the stonecutter drew no buttons at all.
            buffer.writeVarInt(option.inputItemIds.size + 1)
            for (id in option.inputItemIds) buffer.writeVarInt(id)
            buffer.writeVarInt(ITEM_STACK_SLOT_DISPLAY)
            // parity: `ItemStackTemplate.STREAM_CODEC`, which is not an item stack: the item comes
            // first and the count second, and a stack's own order left every button showing air.
            buffer.writeVarInt(option.resultItemId)
            buffer.writeVarInt(option.resultCount)
            // An empty component patch: nothing added, nothing removed.
            buffer.writeVarInt(0)
            buffer.writeVarInt(0)
        }
    }

    public companion object {
        public const val ID: Int = 0x85

        public fun read(buffer: ByteBuffer): UpdateRecipesPacket {
            // The property sets go past unread: this server sends none and reads none back.
            repeat(buffer.readVarInt()) {
                buffer.readProtocolString()
                repeat(buffer.readVarInt()) { buffer.readVarInt() }
            }
            val options =
                List(buffer.readVarInt()) {
                    val inputs = List(buffer.readVarInt() - 1) { buffer.readVarInt() }
                    buffer.readVarInt() // The kind of slot display, which is always the stack one.
                    val item = buffer.readVarInt()
                    val count = buffer.readVarInt()
                    buffer.readVarInt()
                    buffer.readVarInt()
                    StonecutterOption(inputs, item, count)
                }
            return UpdateRecipesPacket(options)
        }

        /** parity: `SlotDisplays.bootstrap`, where `item_stack` is the sixth registered. */
        public const val ITEM_STACK_SLOT_DISPLAY: Int = 5
    }
}
