package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.writeVarInt

/** One row of a stonecutter's list: what goes in, and the stack the button shows. */
public data class StonecutterOption(
    val inputItemIds: List<Int>,
    val result: WireItemStack,
)

/**
 * Clientbound, id 0x85: the recipes a client needs to have itself. Mirrors
 * `ClientboundUpdateRecipesPacket`.
 *
 * The stonecutter is the reason this exists at all: its buttons are drawn by the client from this
 * list, filtered by whatever is in the input slot, and the index it sends back is an index into
 * that filtered list. Without the packet the screen opens empty however many recipes the server
 * knows.
 *
 * The item property sets are the recipe book's own highlighting and are sent empty here.
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
            option.result.write(buffer)
        }
    }

    public companion object {
        public const val ID: Int = 0x85

        /** parity: `SlotDisplays.bootstrap`, where `item_stack` is the sixth registered. */
        public const val ITEM_STACK_SLOT_DISPLAY: Int = 5
    }
}
