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
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
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

    @Test
    fun `an older client is sent the name in the shape its own version reads`() {
        val sent = mutableListOf<ByteArray>()
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_18_2, LABELS))
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
        chain.clientbound(ProtocolState.PLAY, SpawnEntityPacket.ID, body) { _, bytes -> sent += bytes }

        assertEquals(2, sent.size, "the spawn should be followed by the name")
        val named = ByteBuffer().apply { writeBytes(sent[1]) }
        assertEquals(7, named.readVarInt(), "the entity the name belongs to")
        assertEquals(CUSTOM_NAME_INDEX, named.readByte().toInt(), "the name lives at the same index")
        // 1.18.2 numbers an optional name 5, not the 6 this build uses, and reads it as json.
        assertEquals(5, named.readVarInt(), "the kind is the one that version has")
        assertTrue(named.readBoolean(), "the name is present")
        assertTrue("Sulfur Cube (26.2)" in named.readProtocolString(), "the name arrived as json")
    }

    @Test
    fun `a field a later version added moves what sits under it`() {
        val sent = mutableListOf<ByteArray>()
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11, LABELS))
        val spawn =
            ByteBuffer()
                .also {
                    SpawnEntityPacket(
                        entityId = 3,
                        uuid = Uuid.fromLongs(0, 3),
                        entityType = AXOLOTL_776,
                        x = 0.0,
                        y = 0.0,
                        z = 0.0,
                    ).write(it)
                }.toByteArray()
        chain.clientbound(ProtocolState.PLAY, SpawnEntityPacket.ID, spawn) { _, _ -> }

        val data =
            ByteBuffer()
                .also {
                    SetEntityDataPacket(
                        3,
                        listOf(SetEntityDataPacket.Entry.Ints(AXOLOTL_VARIANT_776, 2)),
                    ).write(it)
                }.toByteArray()
        chain.clientbound(ProtocolState.PLAY, SetEntityDataPacket.ID, data) { _, bytes -> sent += bytes }

        val out = ByteBuffer().apply { writeBytes(sent.single()) }
        assertEquals(3, out.readVarInt())
        // 26.1 added the age-locked flag at 17, so everything an ageable mob keeps below it moved.
        assertEquals(AXOLOTL_VARIANT_776 - 1, out.readByte().toInt())
    }

    @Test
    fun `an item inside an entity's data is written the way that version writes one`() {
        val sent = mutableListOf<ByteArray>()
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_18_2, LABELS))
        val data =
            ByteBuffer()
                .also {
                    SetEntityDataPacket(
                        4,
                        listOf(
                            SetEntityDataPacket.Entry.Item(
                                SetEntityDataPacket.ITEM_STACK_INDEX,
                                WireItemStack(STONE_776, 3),
                            ),
                        ),
                    ).write(it)
                }.toByteArray()
        chain.clientbound(ProtocolState.PLAY, SetEntityDataPacket.ID, data) { _, bytes -> sent += bytes }

        val out = ByteBuffer().apply { writeBytes(sent.single()) }
        assertEquals(4, out.readVarInt())
        assertEquals(SetEntityDataPacket.ITEM_STACK_INDEX, out.readByte().toInt())
        // 1.18.2 numbers a stack 6, three before this build does, and reads it as present,
        // item, count and tag.
        assertEquals(6, out.readVarInt())
        assertTrue(out.readBoolean(), "the stack is there")
        assertEquals(STONE_776, out.readVarInt(), "stone did not move between these two")
        assertEquals(3, out.readByte().toInt())
    }

    @Test
    fun `an entity's data reaches 1 dot 8 in the one byte it packs a kind and an index into`() {
        val sent = mutableListOf<ByteArray>()
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_8, LABELS))
        val spawn =
            ByteBuffer()
                .also {
                    SpawnEntityPacket(
                        entityId = 5,
                        uuid = Uuid.fromLongs(0, 5),
                        entityType = ZOMBIE_776,
                        x = 0.0,
                        y = 0.0,
                        z = 0.0,
                    ).write(it)
                }.toByteArray()
        chain.clientbound(ProtocolState.PLAY, SpawnEntityPacket.ID, spawn) { _, _ -> }
        val data =
            ByteBuffer()
                .also {
                    SetEntityDataPacket(
                        5,
                        listOf(SetEntityDataPacket.Entry.Floats(HEALTH_INDEX, 20f)),
                    ).write(it)
                }.toByteArray()
        chain.clientbound(ProtocolState.PLAY, SetEntityDataPacket.ID, data) { _, bytes -> sent += bytes }

        val out = ByteBuffer().apply { writeBytes(sent.single()) }
        assertEquals(5, out.readVarInt())
        // 1.8 numbers a float 3, and writes it in the top bits of the index's own byte.
        val header = out.readByte().toInt() and 0xFF
        assertEquals(3, header shr 5, "the kind rides in the same byte")
        assertEquals(20f, out.readFloat())
        assertEquals(0x7F, out.readByte().toInt() and 0xFF, "1.8 ends the list with its own byte")
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

        const val CUSTOM_NAME_INDEX = 2

        const val AXOLOTL_776 = 7
        const val AXOLOTL_VARIANT_776 = 18

        /** `LivingEntity.DATA_HEALTH_ID`, which every mob sends. */
        const val HEALTH_INDEX = 9

        const val ZOMBIE_776 = 151
    }
}
