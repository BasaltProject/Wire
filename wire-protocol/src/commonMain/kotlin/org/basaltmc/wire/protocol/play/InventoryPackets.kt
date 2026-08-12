package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.ProtocolException
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentNbt

/**
 * One stack on the wire: how many, of what, plus whatever data components it carries.
 *
 * Components are the open-ended part (enchantments, a custom name, damage) and each one has a codec
 * of its own keyed by registry id. This build **writes** stacks with none and **refuses to read**
 * one that has any, rather than pretending: a component cannot be skipped without knowing its
 * length, so a reader that ignored them would not know where the next stack begins and would
 * desynchronise the whole stream. That is the boundary of the inventory support in this build, and
 * it is a boundary that reports itself.
 */
public data class WireItemStack(
    val itemId: Int,
    val count: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(count)
        if (count <= 0) return
        buffer.writeVarInt(itemId)
        // The component patch: nothing added, nothing removed.
        buffer.writeVarInt(0)
        buffer.writeVarInt(0)
    }

    public companion object {
        /** A count of zero is how vanilla writes "this slot is empty"; the rest of the stack follows only if non-empty. */
        public val EMPTY: WireItemStack = WireItemStack(0, 0)

        public fun write(
            buffer: ByteBuffer,
            stack: WireItemStack?,
        ) {
            (stack ?: EMPTY).write(buffer)
        }

        public fun read(buffer: ByteBuffer): WireItemStack? {
            val count = buffer.readVarInt()
            if (count <= 0) return null
            val itemId = buffer.readVarInt()
            val added = buffer.readVarInt()
            val removed = buffer.readVarInt()
            if (added != 0 || removed != 0) {
                throw ProtocolException(
                    "Item stacks with data components are not supported yet " +
                        "($added added, $removed removed on item $itemId)",
                )
            }
            return WireItemStack(itemId, count)
        }

        /**
         * The same stack in the delimited form `set_creative_slot` uses
         * (`ItemStack.OPTIONAL_UNTRUSTED_STREAM_CODEC` over `DataComponentPatch.DELIMITED_STREAM_CODEC`).
         *
         * Each component carries its own length prefix here, so unlike [read] this can skip them
         * blindly instead of refusing the stack: refusing dropped the session whenever a player
         * dragged a damaged tool or a written book out of the creative menu. The components are
         * still discarded, the same gap [read] documents.
         */
        public fun readDelimited(buffer: ByteBuffer): WireItemStack? {
            val count = buffer.readVarInt()
            if (count <= 0) return null
            val itemId = buffer.readVarInt()
            val added = buffer.readVarInt()
            val removed = buffer.readVarInt()
            repeat(added) {
                buffer.readVarInt() // component type id
                buffer.readBytes(buffer.readVarInt()) // its length-prefixed payload
            }
            // Removals are just the type ids, with no payload to skip.
            repeat(removed) { buffer.readVarInt() }
            return WireItemStack(itemId, count)
        }
    }
}

/**
 * Every slot of an open container at once, vanilla's `ClientboundContainerSetContentPacket`.
 *
 * Container 0 is the player's own inventory, which is the only one this build has.
 *
 * [stateId] is vanilla's optimistic-locking counter: the client echoes it back with every click so
 * the server can tell a click made against the current contents from one made against contents it
 * has since replaced. Sending a fresh one is what makes the client drop whatever it was predicting.
 */
public data class ContainerSetContentPacket(
    val containerId: Int,
    val stateId: Int,
    val items: List<WireItemStack?>,
    val carriedItem: WireItemStack? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeVarInt(stateId)
        buffer.writeVarInt(items.size)
        for (item in items) WireItemStack.write(buffer, item)
        WireItemStack.write(buffer, carriedItem)
    }

    public companion object {
        public const val ID: Int = 0x12
        public const val PLAYER_INVENTORY: Int = 0

        public fun read(buffer: ByteBuffer): ContainerSetContentPacket {
            val containerId = buffer.readVarInt()
            val stateId = buffer.readVarInt()
            val size = buffer.readVarInt()
            val items = (0 until size).map { WireItemStack.read(buffer) }
            return ContainerSetContentPacket(containerId, stateId, items, WireItemStack.read(buffer))
        }
    }
}

/** One slot of one container, for when a single thing changed. */
public data class ContainerSetSlotPacket(
    val containerId: Int,
    val stateId: Int,
    val slot: Int,
    val item: WireItemStack?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeVarInt(stateId)
        buffer.writeShort(slot.toShort())
        WireItemStack.write(buffer, item)
    }

    public companion object {
        public const val ID: Int = 0x14

        public fun read(buffer: ByteBuffer): ContainerSetSlotPacket =
            ContainerSetSlotPacket(
                containerId = buffer.readVarInt(),
                stateId = buffer.readVarInt(),
                slot = buffer.readShort().toInt(),
                item = WireItemStack.read(buffer),
            )
    }
}

/** Which hotbar slot the player is holding, server to client (`ClientboundSetHeldSlotPacket`). */
public data class SetHeldSlotPacket(
    val slot: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(slot)
    }

    public companion object {
        public const val ID: Int = 0x69

        public fun read(buffer: ByteBuffer): SetHeldSlotPacket = SetHeldSlotPacket(buffer.readVarInt())
    }
}

/** The same, client to server: the player turned the mouse wheel or pressed a number key. */
public data class ServerboundSetCarriedItemPacket(
    val slot: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeShort(slot.toShort())
    }

    public companion object {
        public const val ID: Int = 0x35

        /** Hotbar slots only; anything else is a client sending nonsense. */
        public const val HOTBAR_SIZE: Int = 9

        public fun read(buffer: ByteBuffer): ServerboundSetCarriedItemPacket =
            ServerboundSetCarriedItemPacket(buffer.readShort().toInt())
    }
}

/**
 * Serverbound, id 0x12: a click inside an open container. Mirrors
 * `ServerboundContainerClickPacket`.
 *
 * [changedSlots] and [carriedItem] are the client's own *prediction* of the result, sent as
 * component **hashes** rather than real stacks (`HashedStack`), so a server can tell a stale
 * prediction from a current one without shipping every component back. This build does not verify
 * them: the click is re-derived from [slot]/[button]/[mode] against the server's own inventory,
 * which is authoritative anyway. They are still parsed exactly, because a stack skipped wrong
 * desynchronises every packet after it.
 */
public data class ServerboundContainerClickPacket(
    val containerId: Int,
    val stateId: Int,
    val slot: Int,
    val button: Int,
    val mode: Mode,
    /**
     * Whether the client believes it is holding something on the cursor after this click. Its full
     * predicted stack is hashed and not reconstructible here, but the presence flag alone is enough
     * to notice that the two sides disagree about whether a stack is in hand at all, which is the
     * shape a click desynchronisation takes.
     */
    val clientHasCarriedItem: Boolean,
) {
    /** parity: `ContainerInput`, by its own numeric ids. */
    public enum class Mode {
        PICKUP,
        QUICK_MOVE,
        SWAP,
        CLONE,
        THROW,
        QUICK_CRAFT,
        PICKUP_ALL,
    }

    public companion object {
        public const val ID: Int = 0x12

        public fun read(buffer: ByteBuffer): ServerboundContainerClickPacket {
            val containerId = buffer.readVarInt()
            val stateId = buffer.readVarInt()
            val slot = buffer.readShort().toInt()
            val button = buffer.readByte().toInt()
            val modeId = buffer.readVarInt()
            // parity: `ByIdMap.OutOfBoundsStrategy.ZERO`, which folds anything unknown to PICKUP
            // rather than refusing the packet.
            val mode = Mode.entries.getOrElse(modeId) { Mode.PICKUP }
            repeat(buffer.readVarInt()) {
                buffer.readShort()
                skipHashedStack(buffer)
            }
            val clientHasCarriedItem = skipHashedStack(buffer)
            return ServerboundContainerClickPacket(containerId, stateId, slot, button, mode, clientHasCarriedItem)
        }

        /**
         * Walks one `HashedStack` without interpreting it: an optional flag, then item and count,
         * then the added component hashes (a type id and a four-byte hash each) and the removed
         * component type ids. Hashes are fixed width, so unlike a real component patch this can
         * always be skipped.
         */
        private fun skipHashedStack(buffer: ByteBuffer): Boolean {
            if (!buffer.readBoolean()) return false
            buffer.readVarInt() // item id
            buffer.readVarInt() // count
            repeat(buffer.readVarInt()) {
                buffer.readVarInt() // component type id
                buffer.readInt() // its hash
            }
            repeat(buffer.readVarInt()) { buffer.readVarInt() } // removed component type ids
            return true
        }
    }
}

/**
 * Serverbound, id 0x38: creative-mode inventory editing. Mirrors
 * `ServerboundSetCreativeModeSlotPacket`. This is the *only* way a creative client's dragged item
 * (from the creative menu into a slot, or a slot into the void to discard it, which arrives as
 * [slot] `-1`) reaches the server: creative item movement never goes through a window-click packet.
 */
public data class ServerboundSetCreativeModeSlotPacket(
    val slot: Int,
    val item: WireItemStack?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeShort(slot.toShort())
        WireItemStack.write(buffer, item)
    }

    public companion object {
        public const val ID: Int = 0x38

        public fun read(buffer: ByteBuffer): ServerboundSetCreativeModeSlotPacket =
            ServerboundSetCreativeModeSlotPacket(buffer.readShort().toInt(), WireItemStack.readDelimited(buffer))
    }
}

/**
 * Clientbound, id 0x3b: opens a container's screen on the client. Mirrors
 * `ClientboundOpenScreenPacket`.
 *
 * [menuType] is the numeric id in the `minecraft:menu` registry, not a name: `minecraft:crafting`
 * is 12 in the real 26.2 jar. The client picks the screen *and the slot layout* from it, so the
 * server's own idea of the layout has to agree with whatever is sent here.
 */
public data class OpenScreenPacket(
    val containerId: Int,
    val menuType: Int,
    val title: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeVarInt(menuType)
        buffer.writeAnonymousNbt(ComponentNbt.serialize(title))
    }

    public companion object {
        public const val ID: Int = 0x3b

        /** `minecraft:crafting`, the 3x3 crafting table screen. */
        public const val CRAFTING_MENU: Int = 12

        public fun read(buffer: ByteBuffer): OpenScreenPacket =
            OpenScreenPacket(
                containerId = buffer.readVarInt(),
                menuType = buffer.readVarInt(),
                title = ComponentNbt.deserialize(buffer.readAnonymousNbt()),
            )
    }
}

/**
 * The container was closed. Both directions carry the same body, and both exist: the client says so
 * when the player presses escape (id 0x13), and the server says so when it closes one on their
 * behalf (id 0x11).
 */
public data class ContainerClosePacket(
    val containerId: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
    }

    public companion object {
        public const val CLIENTBOUND_ID: Int = 0x11
        public const val SERVERBOUND_ID: Int = 0x13

        public fun read(buffer: ByteBuffer): ContainerClosePacket = ContainerClosePacket(buffer.readVarInt())
    }
}
