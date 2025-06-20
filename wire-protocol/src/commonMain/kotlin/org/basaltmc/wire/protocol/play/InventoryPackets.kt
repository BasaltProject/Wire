package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.ProtocolException
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentNbt

/**
 * One stack on the wire: how many, of what, plus whatever data components it carries.
 *
 * The components are a *patch*, vanilla's `DataComponentPatch`: what this stack adds on top of its
 * item's defaults, and what it removes from them. Both directions are carried, because a stack that
 * a client echoes back must come out of here as the stack that went in.
 *
 * Only the components with a case in [ItemComponent] are understood; the rest ride along as bytes
 * ([ItemComponent.Raw]) when the form they arrived in says how long they are. In the undelimited
 * form it does not, so an unknown component there is still refused rather than guessed at: skipping
 * it blindly would lose track of where the next stack begins and desynchronise the stream.
 */
public data class WireItemStack(
    val itemId: Int,
    val count: Int,
    /** What this stack adds to its item's defaults, in the order it was written. */
    val components: List<ItemComponent> = emptyList(),
    /** Component ids this stack removes from its item's defaults. */
    val removedComponents: List<Int> = emptyList(),
) {
    /** The `minecraft:enchantments` levels, or empty: by far the most asked-for component. */
    val enchantments: Map<Int, Int>
        get() =
            components
                .filterIsInstance<ItemComponent.Enchantments>()
                .firstOrNull { !it.stored }
                ?.levels
                .orEmpty()

    /** The `minecraft:damage` this stack has taken, or 0 when it carries none. */
    val damage: Int
        get() =
            components
                .filterIsInstance<ItemComponent.Number>()
                .firstOrNull { it.id == ItemComponent.DAMAGE }
                ?.value ?: 0

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(count)
        if (count <= 0) return
        buffer.writeVarInt(itemId)
        buffer.writeVarInt(components.size)
        buffer.writeVarInt(removedComponents.size)
        for (component in components) {
            buffer.writeVarInt(component.id)
            component.writePayload(buffer)
        }
        for (id in removedComponents) buffer.writeVarInt(id)
    }

    public companion object {
        /** A count of zero is how vanilla writes "this slot is empty"; the rest of the stack follows only if non-empty. */
        public val EMPTY: WireItemStack = WireItemStack(0, 0)

        /** parity: `minecraft:enchantments`' index in `DataComponents`' own registration order. */
        public const val ENCHANTMENTS_COMPONENT: Int = ItemComponent.ENCHANTMENTS

        public fun enchanted(
            itemId: Int,
            count: Int,
            enchantments: Map<Int, Int>,
        ): WireItemStack =
            WireItemStack(
                itemId,
                count,
                if (enchantments.isEmpty()) emptyList() else listOf(ItemComponent.Enchantments(enchantments)),
            )

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
            val components =
                List(added) {
                    val id = buffer.readVarInt()
                    ItemComponent.readPayload(buffer, id, length = null)
                        ?: throw ProtocolException("Data component $id is not supported yet on item $itemId")
                }
            return WireItemStack(itemId, count, components, List(removed) { buffer.readVarInt() })
        }

        /**
         * The same stack in the delimited form `set_creative_slot` uses
         * (`ItemStack.OPTIONAL_UNTRUSTED_STREAM_CODEC` over `DataComponentPatch.DELIMITED_STREAM_CODEC`).
         *
         * Every component is length-prefixed here, so unlike [read] nothing has to be understood to
         * be kept: a creative client echoes a slot whenever the inventory screen opens, and an
         * earlier version of this dropped whatever it could not name, stripping an enchanted item
         * its owner had merely looked at.
         */
        public fun readDelimited(buffer: ByteBuffer): WireItemStack? {
            val count = buffer.readVarInt()
            if (count <= 0) return null
            val itemId = buffer.readVarInt()
            val added = buffer.readVarInt()
            val removed = buffer.readVarInt()
            val components =
                List(added) {
                    val id = buffer.readVarInt()
                    val length = buffer.readVarInt()
                    val before = buffer.readableBytes
                    val component = ItemComponent.readPayload(buffer, id, length)
                    // Whatever was read, the next component starts where this one's length said.
                    buffer.readBytes(length - (before - buffer.readableBytes))
                    component ?: ItemComponent.Raw(id, ByteArray(0))
                }
            // Removals are just the type ids, with no payload to skip.
            return WireItemStack(itemId, count, components, List(removed) { buffer.readVarInt() })
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

        /** `minecraft:generic_9x3`, the three rows a single chest and a barrel show. */
        public const val CHEST_MENU: Int = 2

        /** `minecraft:smithing`, three slots in and one out. */
        public const val SMITHING_MENU: Int = 21

        /** `minecraft:stonecutter`, one slot in, one out, and a list of buttons. */
        public const val STONECUTTER_MENU: Int = 24

        /** `minecraft:generic_9x6`, the six rows a double chest shows. */
        public const val DOUBLE_CHEST_MENU: Int = 5

        /** `minecraft:crafter_3x3`, the nine slots of a crafter with no result slot. */
        public const val CRAFTER_MENU: Int = 7

        /** `minecraft:merchant`, a villager's own window. */
        public const val MERCHANT_MENU: Int = 19

        /** The three cookers, whose screens differ only in which recipes their book offers. */
        public const val FURNACE_MENU: Int = 14
        public const val BLAST_FURNACE_MENU: Int = 10
        public const val SMOKER_MENU: Int = 22

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

/**
 * Clientbound, id 0x13: one number of an open window, which is how a furnace draws its two bars.
 *
 * Mirrors `ClientboundContainerSetDataPacket`. The meaning of [id] is the window's own: for a
 * furnace, 0 is the fire left, 1 is how long that fuel lasts, 2 is the progress and 3 the total.
 */
public data class ContainerSetDataPacket(
    val containerId: Int,
    val id: Int,
    val value: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeShort(id.toShort())
        buffer.writeShort(value.toShort())
    }

    public companion object {
        public const val ID: Int = 0x13

        /** parity: `AbstractFurnaceMenu`'s own four data slots. */
        public const val FURNACE_LIT_TIME: Int = 0
        public const val FURNACE_LIT_DURATION: Int = 1
        public const val FURNACE_COOKING_PROGRESS: Int = 2
        public const val FURNACE_COOKING_TOTAL: Int = 3

        public fun read(buffer: ByteBuffer): ContainerSetDataPacket =
            ContainerSetDataPacket(
                containerId = buffer.readVarInt(),
                id = buffer.readShort().toInt(),
                value = buffer.readShort().toInt(),
            )
    }
}

/** One thing a merchant asks for: an item, how many, and the components it has to match. */
public data class MerchantCost(
    val itemId: Int,
    val count: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(itemId)
        buffer.writeVarInt(count)
        // parity: `DataComponentExactPredicate`, empty for every trade the jar ships as data.
        buffer.writeVarInt(0)
    }

    public companion object {
        public fun read(buffer: ByteBuffer): MerchantCost {
            val itemId = buffer.readVarInt()
            val count = buffer.readVarInt()
            val exactComponents = buffer.readVarInt()
            repeat(exactComponents) {
                buffer.readVarInt()
                buffer.readVarInt()
            }
            return MerchantCost(itemId, count)
        }
    }
}

/** One offer as the client draws it. Mirrors `MerchantOffer`'s own stream form. */
public data class MerchantOffer(
    val costA: MerchantCost,
    val result: WireItemStack,
    val costB: MerchantCost?,
    val outOfStock: Boolean,
    val uses: Int,
    val maxUses: Int,
    val experience: Int,
    val specialPrice: Int,
    val priceMultiplier: Float,
    val demand: Int,
) {
    public fun write(buffer: ByteBuffer) {
        costA.write(buffer)
        result.write(buffer)
        buffer.writeBoolean(costB != null)
        costB?.write(buffer)
        buffer.writeBoolean(outOfStock)
        buffer.writeInt(uses)
        buffer.writeInt(maxUses)
        buffer.writeInt(experience)
        buffer.writeInt(specialPrice)
        buffer.writeFloat(priceMultiplier)
        buffer.writeInt(demand)
    }
}

/**
 * Clientbound, id 0x33: the trades a merchant is offering. Mirrors `ClientboundMerchantOffersPacket`.
 *
 * Sent right after the merchant window opens, and again whenever a trade is taken: the client draws
 * the list it was last sent and nothing else.
 */
public data class MerchantOffersPacket(
    val containerId: Int,
    val offers: List<MerchantOffer>,
    val level: Int,
    val experience: Int,
    val showProgress: Boolean,
    val canRestock: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeVarInt(offers.size)
        for (offer in offers) offer.write(buffer)
        buffer.writeVarInt(level)
        buffer.writeVarInt(experience)
        buffer.writeBoolean(showProgress)
        buffer.writeBoolean(canRestock)
    }

    public companion object {
        public const val ID: Int = 0x33
    }
}

/**
 * Serverbound, id 0x33: which of the offers the player picked. Mirrors `ServerboundSelectTradePacket`.
 */
public data class SelectTradePacket(
    val index: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(index)
    }

    public companion object {
        public const val ID: Int = 0x33

        public fun read(buffer: ByteBuffer): SelectTradePacket = SelectTradePacket(buffer.readVarInt())
    }
}

/**
 * Serverbound, id 0x11: a button inside an open window. Mirrors `ServerboundContainerButtonClickPacket`.
 *
 * The lectern is what makes this worth having: its pages are buttons (one back, two forward, three
 * to take the book, and a hundred plus the page to jump straight to one).
 */
public data class ContainerButtonClickPacket(
    val containerId: Int,
    val buttonId: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeVarInt(buttonId)
    }

    public companion object {
        public const val ID: Int = 0x11

        /** parity: `LecternMenu.clickMenuButton`'s own numbering. */
        public const val LECTERN_PREVIOUS_PAGE: Int = 1
        public const val LECTERN_NEXT_PAGE: Int = 2
        public const val LECTERN_TAKE_BOOK: Int = 3
        public const val LECTERN_JUMP_TO_PAGE: Int = 100

        public fun read(buffer: ByteBuffer): ContainerButtonClickPacket =
            ContainerButtonClickPacket(buffer.readVarInt(), buffer.readVarInt())
    }
}

/** `minecraft:lectern`, whose window is one book and a page number. */
public object LecternMenuType {
    public const val ID: Int = 17
}
