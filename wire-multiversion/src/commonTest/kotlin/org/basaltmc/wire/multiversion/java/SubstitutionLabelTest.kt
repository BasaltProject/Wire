package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.multiversion.SubstitutionLabels
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.ItemComponent
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue
import kotlin.uuid.Uuid

/** What a client too old for an item is shown, and what it is told that stand-in really is. */
class SubstitutionLabelTest {
    @Test
    fun `an item the older version never had arrives named after the real one`() {
        val stack = through(SULFUR_776)
        assertNotEquals(SULFUR_776, stack.itemId, "26.1 has no sulfur of its own to be sent")
        assertTrue(
            stack.components.any { it is ItemComponent.Text && it.text == LABEL },
            "the stand-in arrived unnamed: ${stack.components}",
        )
    }

    @Test
    fun `an item both versions have is left alone`() {
        val stack = through(STONE_776)
        assertEquals(STONE_776, stack.itemId)
        assertEquals(emptyList(), stack.components)
    }

    @Test
    fun `a name somebody chose is not overwritten`() {
        val chosen = Component.text("Steve's rock")
        val stack = through(SULFUR_776, listOf(ItemComponent.Text(ItemComponent.CUSTOM_NAME, chosen)))
        assertEquals(1, stack.components.size)
        assertEquals(chosen, (stack.components.single() as ItemComponent.Text).text)
    }

    @Test
    fun `a mob the older version never had arrives as one it has and says what it is`() {
        val sent = mutableListOf<Pair<Int, ByteArray>>()
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V26_1, LABELS))
        val body =
            ByteBuffer()
                .also {
                    SpawnEntityPacket(
                        entityId = 7,
                        uuid = Uuid.fromLongs(0, 7),
                        entityType = SULFUR_CUBE_776,
                        x = 0.0,
                        y = 0.0,
                        z = 0.0,
                    ).write(it)
                }.toByteArray()
        chain.clientbound(ProtocolState.PLAY, SpawnEntityPacket.ID, body) { id, bytes ->
            sent += id to bytes
        }

        assertEquals(2, sent.size, "the spawn should be followed by the name")
        val spawn = SpawnEntityPacket.read(ByteBuffer().apply { writeBytes(sent[0].second) })
        assertNotEquals(SULFUR_CUBE_776, spawn.entityType, "26.1 has no sulfur cube to be sent")
        val named = SetEntityDataPacket.read(ByteBuffer().apply { writeBytes(sent[1].second) })
        assertEquals(7, named.entityId)
        assertEquals(
            listOf<SetEntityDataPacket.Entry>(
                SetEntityDataPacket.Entry.OptionalText(SetEntityDataPacket.CUSTOM_NAME_INDEX, MOB_LABEL),
                SetEntityDataPacket.Entry.Booleans(SetEntityDataPacket.CUSTOM_NAME_VISIBLE_INDEX, true),
            ),
            named.entries,
        )
    }

    private fun through(
        itemId: Int,
        components: List<ItemComponent> = emptyList(),
    ): WireItemStack {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V26_1, LABELS))
        val body =
            ByteBuffer()
                .also { ContainerSetSlotPacket(0, 0, 0, WireItemStack(itemId, 1, components)).write(it) }
                .toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, ContainerSetSlotPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        return checkNotNull(ContainerSetSlotPacket.read(out).item)
    }

    private companion object {
        val LABEL = Component.text("Sulfur (26.2)")
        val MOB_LABEL = Component.text("Sulfur Cube (26.2)")

        val LABELS =
            object : SubstitutionLabels {
                override fun itemLabel(nativeItemId: Int): Component = LABEL

                override fun entityLabel(nativeEntityTypeId: Int): Component = MOB_LABEL
            }

        /** `minecraft:sulfur_cube`, the mob 26.2 added: 26.1 is shown a magma cube in its place. */
        const val SULFUR_CUBE_776 = 130

        /** `minecraft:sulfur`, which 26.2 added: 26.1 is shown something else in its place. */
        const val SULFUR_776 = 26

        const val STONE_776 = 1
    }
}
