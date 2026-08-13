package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.ProtocolException
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class InventoryPacketsTest {
    @Test
    fun aStackWithNoComponentsSurvivesARoundTrip() {
        val buffer = ByteBuffer()
        WireItemStack(itemId = 1, count = 64).write(buffer)

        assertEquals(WireItemStack(1, 64), WireItemStack.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun anEmptySlotIsACountOfZeroAndNothingElse() {
        val buffer = ByteBuffer()
        WireItemStack.write(buffer, null)

        // One byte: writing the item id of an empty slot would be a byte the client does not read.
        assertEquals(1, buffer.readableBytes)
        assertNull(WireItemStack.read(buffer))
    }

    @Test
    fun aStackCarryingComponentsIsRefusedRatherThanMisread() {
        val buffer = ByteBuffer()
        // Hand-written: one component added, and one this build has no codec for (1 is
        // `minecraft:max_stack_size`, not the enchantments it can read).
        buffer.writeByte(1)
        buffer.writeByte(1)
        buffer.writeByte(1)
        buffer.writeByte(0)
        buffer.writeByte(1)

        // Refused, because a component cannot be skipped without knowing its length: a reader that
        // ignored it would not know where the next stack starts.
        assertFailsWith<ProtocolException> { WireItemStack.read(buffer) }
    }

    @Test
    fun theWholeWindowSurvivesARoundTrip() {
        val packet =
            ContainerSetContentPacket(
                containerId = ContainerSetContentPacket.PLAYER_INVENTORY,
                stateId = 7,
                items = listOf(WireItemStack(1, 1), null, WireItemStack(2, 32)),
                carriedItem = null,
            )
        val buffer = ByteBuffer()
        packet.write(buffer)

        assertEquals(packet, ContainerSetContentPacket.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun oneSlotSurvivesARoundTrip() {
        val packet = ContainerSetSlotPacket(0, 3, 36, WireItemStack(5, 12))
        val buffer = ByteBuffer()
        packet.write(buffer)

        assertEquals(packet, ContainerSetSlotPacket.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun theHeldSlotSurvivesARoundTripInBothDirections() {
        val outgoing = ByteBuffer()
        SetHeldSlotPacket(4).write(outgoing)
        assertEquals(SetHeldSlotPacket(4), SetHeldSlotPacket.read(outgoing))

        val incoming = ByteBuffer()
        ServerboundSetCarriedItemPacket(8).write(incoming)
        assertEquals(ServerboundSetCarriedItemPacket(8), ServerboundSetCarriedItemPacket.read(incoming))
    }

    @Test
    fun creativeModeSlotSurvivesARoundTrip() {
        val buffer = ByteBuffer()
        val packet = ServerboundSetCreativeModeSlotPacket(slot = 36, item = WireItemStack(itemId = 10, count = 64))
        packet.write(buffer)

        assertEquals(packet, ServerboundSetCreativeModeSlotPacket.read(buffer))
    }

    @Test
    fun creativeModeSlotSkipsLengthPrefixedComponentsInsteadOfRefusingThem() {
        // Hand-written in `set_creative_slot`'s own delimited form: count 1, item 55, one added
        // component (type 7, a 3-byte payload) and one removed component (type 9). Every added
        // component carries its own varint length there, which is what makes skipping safe.
        val buffer = ByteBuffer()
        buffer.writeShort(36)
        buffer.writeVarInt(1)
        buffer.writeVarInt(55)
        buffer.writeVarInt(1)
        buffer.writeVarInt(1)
        buffer.writeVarInt(7)
        buffer.writeVarInt(3)
        buffer.writeBytes(byteArrayOf(1, 2, 3))
        buffer.writeVarInt(9)

        val packet = ServerboundSetCreativeModeSlotPacket.read(buffer)

        assertEquals(36, packet.slot)
        assertEquals(WireItemStack(itemId = 55, count = 1), packet.item)
        // Everything was consumed: a leftover byte here is a desynchronised stream, which is how a
        // misparse turns into the server holding an item the player never picked.
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun aContainerClickIsParsedPastItsHashedPredictionState() {
        // The client's predicted result travels as component *hashes* (`HashedStack`), not as real
        // stacks: an optional flag, item and count, then added components as a type id plus a
        // fixed four-byte hash each, then removed component type ids. Getting that walk wrong
        // desynchronises every packet after this one, so the whole body has to be consumed.
        val buffer = ByteBuffer()
        buffer.writeVarInt(0) // container 0
        buffer.writeVarInt(7) // state id
        buffer.writeShort(36) // slot
        buffer.writeByte(0) // left button
        buffer.writeVarInt(1) // QUICK_MOVE
        buffer.writeVarInt(1) // one changed slot
        buffer.writeShort(36)
        buffer.writeBoolean(true) // that slot's predicted stack is present
        buffer.writeVarInt(55)
        buffer.writeVarInt(3)
        buffer.writeVarInt(1) // one added component hash
        buffer.writeVarInt(9)
        buffer.writeInt(0x1234)
        buffer.writeVarInt(1) // one removed component
        buffer.writeVarInt(11)
        buffer.writeBoolean(false) // empty cursor

        val packet = ServerboundContainerClickPacket.read(buffer)

        assertEquals(0, packet.containerId)
        assertEquals(7, packet.stateId)
        assertEquals(36, packet.slot)
        assertEquals(ServerboundContainerClickPacket.Mode.QUICK_MOVE, packet.mode)
        assertEquals(false, packet.clientHasCarriedItem)
        assertEquals(0, buffer.readableBytes, "the hashed prediction state was not fully consumed")
    }

    @Test
    fun creativeModeSlotDiscardHasNoItem() {
        val buffer = ByteBuffer()
        // Vanilla's own "drag out of the window to delete it" gesture: slot -1, no item.
        val packet = ServerboundSetCreativeModeSlotPacket(slot = -1, item = null)
        packet.write(buffer)

        assertEquals(packet, ServerboundSetCreativeModeSlotPacket.read(buffer))
    }

    @Test
    fun anEnchantedStackRoundTripsItsComponent() {
        val buffer = ByteBuffer()
        val stack = WireItemStack(itemId = 900, count = 1, enchantments = mapOf(3 to 5, 7 to 1))
        stack.write(buffer)
        assertEquals(stack, WireItemStack.read(buffer))
    }

    @Test
    fun aStackWithNoComponentsStillWritesAnEmptyPatch() {
        val buffer = ByteBuffer()
        val stack = WireItemStack(itemId = 1, count = 2)
        stack.write(buffer)
        assertEquals(stack, WireItemStack.read(buffer))
    }

    @Test
    fun aDelimitedStackKeepsItsEnchantmentsAndSkipsTheRest() {
        // What a creative client echoes back when the inventory screen opens: the enchantments have
        // to survive the round trip or the item is stripped just for being looked at.
        val buffer = ByteBuffer()
        buffer.writeVarInt(1) // count
        buffer.writeVarInt(7) // item id
        buffer.writeVarInt(2) // added components
        buffer.writeVarInt(0) // removed
        val enchantments = ByteBuffer()
        enchantments.writeVarInt(1)
        enchantments.writeVarInt(3) // the enchantment's plain registry id
        enchantments.writeVarInt(5)
        val payload = enchantments.readBytes(enchantments.readableBytes)
        buffer.writeVarInt(WireItemStack.ENCHANTMENTS_COMPONENT)
        buffer.writeVarInt(payload.size)
        buffer.writeBytes(payload)
        buffer.writeVarInt(999) // a component this build does not model
        buffer.writeVarInt(3)
        buffer.writeBytes(byteArrayOf(1, 2, 3))

        val decoded = assertNotNull(WireItemStack.readDelimited(buffer))
        assertEquals(mapOf(3 to 5), decoded.enchantments)
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun enchantmentsAreWrittenAsPlainRegistryIds() {
        // parity: `Enchantment.STREAM_CODEC` is `holderRegistry`. Writing id plus one put every
        // enchantment on the next registry entry: asking for efficiency gave feather falling.
        val buffer = ByteBuffer()
        WireItemStack(itemId = 7, count = 1, enchantments = mapOf(12 to 5)).write(buffer)
        buffer.readVarInt() // count
        buffer.readVarInt() // item id
        buffer.readVarInt() // added components
        buffer.readVarInt() // removed components
        assertEquals(WireItemStack.ENCHANTMENTS_COMPONENT, buffer.readVarInt())
        assertEquals(1, buffer.readVarInt())
        assertEquals(12, buffer.readVarInt())
        assertEquals(5, buffer.readVarInt())
    }
}
