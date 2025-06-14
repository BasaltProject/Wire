package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentNbt

/**
 * One entry of an item stack's component patch: `minecraft:damage`, `minecraft:custom_name` and the
 * rest. parity: `DataComponentPatch`, whose wire form is the added components and then the removed
 * ones, each by its registry id.
 *
 * [Raw] is what keeps this honest. A component with no case here is carried as the bytes it arrived
 * as instead of being dropped, so a stack that goes out to a client and comes back is the same
 * stack; only writing a *new* value of one needs a case, and those are the ones a command can set.
 */
public sealed interface ItemComponent {
    public val id: Int

    public fun writePayload(buffer: ByteBuffer)

    /** `minecraft:enchantments` (13) and `minecraft:stored_enchantments` (42): registry id to level. */
    public data class Enchantments(
        val levels: Map<Int, Int>,
        val stored: Boolean = false,
    ) : ItemComponent {
        override val id: Int get() = if (stored) STORED_ENCHANTMENTS else ENCHANTMENTS

        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeVarInt(levels.size)
            for ((enchantment, level) in levels) {
                // parity: `Enchantment.STREAM_CODEC` is `holderRegistry`, which writes the plain id.
                // Adding one landed every enchantment on the next entry of the registry.
                buffer.writeVarInt(enchantment)
                buffer.writeVarInt(level)
            }
        }
    }

    /** A component whose whole payload is one varint: damage, max_damage, max_stack_size, repair_cost, rarity. */
    public data class Number(
        override val id: Int,
        val value: Int,
    ) : ItemComponent {
        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeVarInt(value)
        }
    }

    /** `minecraft:dyed_color` (44), a plain big-endian int rather than a varint. */
    public data class Color(
        val rgb: Int,
    ) : ItemComponent {
        override val id: Int get() = DYED_COLOR

        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeInt(rgb)
        }
    }

    /** `minecraft:custom_name` (6) and `minecraft:item_name` (9). */
    public data class Text(
        override val id: Int,
        val text: Component,
    ) : ItemComponent {
        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeAnonymousNbt(ComponentNbt.serialize(text))
        }
    }

    /** `minecraft:lore` (11): a list of components, at most 256 as vanilla's own codec allows. */
    public data class Lore(
        val lines: List<Component>,
    ) : ItemComponent {
        override val id: Int get() = LORE

        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeVarInt(lines.size)
            for (line in lines) buffer.writeAnonymousNbt(ComponentNbt.serialize(line))
        }
    }

    /** `minecraft:custom_data` (0): a compound nothing but a datapack reads. */
    public data class CustomData(
        val tag: NbtCompound,
    ) : ItemComponent {
        override val id: Int get() = CUSTOM_DATA

        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeAnonymousNbt(tag)
        }
    }

    /** `minecraft:enchantment_glint_override` (21), the only boolean component. */
    public data class Flag(
        override val id: Int,
        val value: Boolean,
    ) : ItemComponent {
        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeBoolean(value)
        }
    }

    /** A component that is present or absent and carries nothing: `unbreakable`, `creative_slot_lock`. */
    public data class Unit(
        override val id: Int,
    ) : ItemComponent {
        override fun writePayload(buffer: ByteBuffer): kotlin.Unit = Unit
    }

    /** A component this build has no codec for, kept as the bytes it arrived as. */
    public data class Raw(
        override val id: Int,
        val payload: ByteArray,
    ) : ItemComponent {
        override fun writePayload(buffer: ByteBuffer) {
            buffer.writeBytes(payload)
        }

        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is Raw) return false
            return id == other.id && payload.contentEquals(other.payload)
        }

        override fun hashCode(): Int = id * 31 + payload.contentHashCode()
    }

    public companion object {
        public const val CUSTOM_DATA: Int = 0
        public const val MAX_STACK_SIZE: Int = 1
        public const val MAX_DAMAGE: Int = 2
        public const val DAMAGE: Int = 3
        public const val UNBREAKABLE: Int = 4
        public const val CUSTOM_NAME: Int = 6
        public const val ITEM_NAME: Int = 9
        public const val LORE: Int = 11
        public const val RARITY: Int = 12
        public const val ENCHANTMENTS: Int = 13
        public const val REPAIR_COST: Int = 19
        public const val CREATIVE_SLOT_LOCK: Int = 20
        public const val ENCHANTMENT_GLINT_OVERRIDE: Int = 21
        public const val STORED_ENCHANTMENTS: Int = 42
        public const val DYED_COLOR: Int = 44

        /**
         * Reads one component's payload, given how many bytes it occupies, or null when the length
         * is unknown (the undelimited form) and this build has no codec for that id.
         *
         * The typed cases are the ones a command can set, so they are also the ones that have to
         * survive a round trip through a client rather than coming back as opaque bytes.
         */
        public fun readPayload(
            buffer: ByteBuffer,
            id: Int,
            length: Int?,
        ): ItemComponent? {
            when (id) {
                ENCHANTMENTS, STORED_ENCHANTMENTS ->
                    return Enchantments(readLevels(buffer), stored = id == STORED_ENCHANTMENTS)
                MAX_STACK_SIZE, MAX_DAMAGE, DAMAGE, RARITY, REPAIR_COST ->
                    return Number(id, buffer.readVarInt())
                UNBREAKABLE, CREATIVE_SLOT_LOCK -> return Unit(id)
                ENCHANTMENT_GLINT_OVERRIDE -> return Flag(id, buffer.readBoolean())
                DYED_COLOR -> return Color(buffer.readInt())
                CUSTOM_NAME, ITEM_NAME -> return Text(id, ComponentNbt.deserialize(buffer.readAnonymousNbt()))
                LORE -> return Lore(List(buffer.readVarInt()) { ComponentNbt.deserialize(buffer.readAnonymousNbt()) })
                CUSTOM_DATA -> return CustomData(buffer.readAnonymousNbt() as? NbtCompound ?: NbtCompound())
            }
            return length?.let { Raw(id, buffer.readBytes(it)) }
        }

        private fun readLevels(buffer: ByteBuffer): Map<Int, Int> =
            buildMap { repeat(buffer.readVarInt()) { put(buffer.readVarInt(), buffer.readVarInt()) } }
    }
}
