package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt

/**
 * One slot of a recipe as the book draws it. parity: `SlotDisplay`, of which the shapes a crafting
 * recipe needs are here: nothing, one item, or a choice between several.
 *
 * The type ids are `SlotDisplays`' own registration order, which is what the wire carries.
 */
public sealed interface SlotDisplay {
    public val type: Int

    public fun write(buffer: ByteBuffer)

    /** An empty cell of a shaped recipe. */
    public data object Empty : SlotDisplay {
        override val type: Int get() = 0

        override fun write(buffer: ByteBuffer) {
            // Nothing follows the type.
        }
    }

    /** One item, by its protocol id. */
    public data class Item(
        val itemId: Int,
    ) : SlotDisplay {
        override val type: Int get() = 4

        override fun write(buffer: ByteBuffer) {
            // A holder reference: the id plus one, zero being an inline value.
            buffer.writeVarInt(itemId)
        }
    }

    /** Any of several items, which is what an ingredient built from a tag looks like. */
    public data class Composite(
        val options: List<SlotDisplay>,
    ) : SlotDisplay {
        override val type: Int get() = 10

        override fun write(buffer: ByteBuffer) {
            buffer.writeVarInt(options.size)
            for (option in options) {
                buffer.writeVarInt(option.type)
                option.write(buffer)
            }
        }
    }

    public companion object {
        public fun read(buffer: ByteBuffer): SlotDisplay =
            when (val type = buffer.readVarInt()) {
                0 -> Empty
                4 -> Item(buffer.readVarInt())
                10 -> Composite(List(buffer.readVarInt()) { read(buffer) })
                else -> error("slot display type $type is not written by this build")
            }
    }
}

/**
 * A recipe as the book draws it. parity: `RecipeDisplay`, limited to the two crafting shapes this
 * server understands; the type ids are `RecipeDisplays`' registration order.
 */
public sealed interface RecipeDisplay {
    public val type: Int

    public fun write(buffer: ByteBuffer)

    public data class Shapeless(
        val ingredients: List<SlotDisplay>,
        val result: SlotDisplay,
        val craftingStation: SlotDisplay,
    ) : RecipeDisplay {
        override val type: Int get() = 0

        override fun write(buffer: ByteBuffer) {
            buffer.writeVarInt(ingredients.size)
            for (ingredient in ingredients) {
                buffer.writeVarInt(ingredient.type)
                ingredient.write(buffer)
            }
            buffer.writeVarInt(result.type)
            result.write(buffer)
            buffer.writeVarInt(craftingStation.type)
            craftingStation.write(buffer)
        }
    }

    public data class Shaped(
        val width: Int,
        val height: Int,
        val ingredients: List<SlotDisplay>,
        val result: SlotDisplay,
        val craftingStation: SlotDisplay,
    ) : RecipeDisplay {
        override val type: Int get() = 1

        override fun write(buffer: ByteBuffer) {
            buffer.writeVarInt(width)
            buffer.writeVarInt(height)
            buffer.writeVarInt(ingredients.size)
            for (ingredient in ingredients) {
                buffer.writeVarInt(ingredient.type)
                ingredient.write(buffer)
            }
            buffer.writeVarInt(result.type)
            result.write(buffer)
            buffer.writeVarInt(craftingStation.type)
            craftingStation.write(buffer)
        }
    }

    public companion object {
        public fun read(buffer: ByteBuffer): RecipeDisplay =
            when (val type = buffer.readVarInt()) {
                0 ->
                    Shapeless(
                        List(buffer.readVarInt()) { SlotDisplay.read(buffer) },
                        SlotDisplay.read(buffer),
                        SlotDisplay.read(buffer),
                    )
                1 -> {
                    val width = buffer.readVarInt()
                    val height = buffer.readVarInt()
                    Shaped(
                        width,
                        height,
                        List(buffer.readVarInt()) { SlotDisplay.read(buffer) },
                        SlotDisplay.read(buffer),
                        SlotDisplay.read(buffer),
                    )
                }
                else -> error("recipe display type $type is not written by this build")
            }
    }
}

/**
 * One entry of the recipe book. parity: `RecipeDisplayEntry`.
 *
 * [craftingRequirements] is what the book highlights with: each ingredient is a holder set of item
 * ids, and a recipe with none can be shown but never suggested.
 */
public data class RecipeBookEntry(
    val recipeId: Int,
    val display: RecipeDisplay,
    val category: Int,
    val craftingRequirements: List<List<Int>>,
    val notification: Boolean = false,
    val highlight: Boolean = false,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(recipeId)
        buffer.writeVarInt(display.type)
        display.write(buffer)
        // The optional group id, which nothing here groups by.
        buffer.writeBoolean(false)
        buffer.writeVarInt(category)
        buffer.writeBoolean(craftingRequirements.isNotEmpty())
        if (craftingRequirements.isNotEmpty()) {
            buffer.writeVarInt(craftingRequirements.size)
            for (ingredient in craftingRequirements) {
                // A holder set: the size plus one, zero being a tag reference instead.
                buffer.writeVarInt(ingredient.size + 1)
                for (item in ingredient) buffer.writeVarInt(item)
            }
        }
        var flags = 0
        if (notification) flags = flags or 1
        if (highlight) flags = flags or 2
        buffer.writeByte(flags.toByte())
    }

    public companion object {
        /** parity: `RecipeBookCategories`' registration order; `crafting_misc` is what a command adds to. */
        public const val CRAFTING_MISC_CATEGORY: Int = 3

        public fun read(buffer: ByteBuffer): RecipeBookEntry {
            val recipeId = buffer.readVarInt()
            val display = RecipeDisplay.read(buffer)
            check(!buffer.readBoolean()) { "a group is never written here" }
            val category = buffer.readVarInt()
            val requirements =
                if (buffer.readBoolean()) {
                    List(buffer.readVarInt()) {
                        val size = buffer.readVarInt() - 1
                        List(size) { buffer.readVarInt() }
                    }
                } else {
                    emptyList()
                }
            val flags = buffer.readByte().toInt()
            return RecipeBookEntry(
                recipeId,
                display,
                category,
                requirements,
                notification = flags and 1 != 0,
                highlight = flags and 2 != 0,
            )
        }
    }
}

/** Clientbound, id 0x4a: adds recipes to the book. Mirrors `ClientboundRecipeBookAddPacket`. */
public data class RecipeBookAddPacket(
    val entries: List<RecipeBookEntry>,
    val replace: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entries.size)
        for (entry in entries) entry.write(buffer)
        buffer.writeBoolean(replace)
    }

    public companion object {
        public const val ID: Int = 0x4a

        public fun read(buffer: ByteBuffer): RecipeBookAddPacket =
            RecipeBookAddPacket(List(buffer.readVarInt()) { RecipeBookEntry.read(buffer) }, buffer.readBoolean())
    }
}

/** Clientbound, id 0x4b: takes recipes back out of the book. Mirrors `ClientboundRecipeBookRemovePacket`. */
public data class RecipeBookRemovePacket(
    val recipeIds: List<Int>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(recipeIds.size)
        for (id in recipeIds) buffer.writeVarInt(id)
    }

    public companion object {
        public const val ID: Int = 0x4b

        public fun read(buffer: ByteBuffer): RecipeBookRemovePacket =
            RecipeBookRemovePacket(List(buffer.readVarInt()) { buffer.readVarInt() })
    }
}
