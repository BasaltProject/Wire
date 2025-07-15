package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.ChunkRewriter
import org.basaltmc.wire.multiversion.IdMap
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.PalettedContainer
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The numbers are read out of the two versions' own reports (26.1.2 and 26.2), so a failure here
 * means the table is wrong rather than the expectation being stale.
 */
class RenumberingLinkTest {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V26_1))

    private fun clientbound(
        id: Int,
        write: (ByteBuffer) -> Unit,
    ): ByteBuffer {
        val body = ByteBuffer().also(write).toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, id, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        return out
    }

    @Test
    fun `a block state keeps its number where nothing moved`() {
        val body =
            clientbound(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(BLOCK_POS, STONE_776).write(it)
            }
        assertEquals(STONE_775, ClientboundBlockUpdatePacket.read(body).blockStateId)
    }

    @Test
    fun `a block 26_1 never heard of is shown as something it has`() {
        assertEquals(RED_SANDSTONE_775, TABLES_775.blockStates.map(CINNABAR_776))
    }

    @Test
    fun `an item moves to where 26_1 keeps it`() {
        val body =
            clientbound(ContainerSetSlotPacket.ID) {
                ContainerSetSlotPacket(0, 1, 5, WireItemStack(OAK_PLANKS_776, 3)).write(it)
            }
        val slot = ContainerSetSlotPacket.read(body)
        assertEquals(OAK_PLANKS_775, slot.item?.itemId)
        assertEquals(3, slot.item?.count)
    }

    @Test
    fun `an item that is new becomes the one it was modelled on`() {
        val body =
            clientbound(ContainerSetSlotPacket.ID) {
                ContainerSetSlotPacket(0, 1, 5, WireItemStack(CINNABAR_ITEM_776, 1)).write(it)
            }
        assertEquals(RED_SANDSTONE_ITEM_775, ContainerSetSlotPacket.read(body).item?.itemId)
    }

    @Test
    fun `an attribute 26_1 does not have is left out instead of sent wrong`() {
        val body =
            clientbound(UpdateAttributesPacket.ID) {
                UpdateAttributesPacket(
                    entityId = 7,
                    attributes =
                        listOf(
                            UpdateAttributesPacket.AttributeSnapshot(MAX_HEALTH_776, 20.0),
                            UpdateAttributesPacket.AttributeSnapshot(BOUNCINESS_776, 1.0),
                        ),
                ).write(it)
            }
        val attributes = UpdateAttributesPacket.read(body)
        assertEquals(1, attributes.attributes.size, "the attribute 26.1 has no name for went out anyway")
        assertEquals(MAX_HEALTH_775, attributes.attributes.single().attribute)
    }

    @Test
    fun `a particle that exists on both sides is renumbered`() {
        val body =
            clientbound(LevelParticlesPacket.ID) {
                LevelParticlesPacket(FLAME_776, 1.0, 2.0, 3.0).write(it)
            }
        assertEquals(FLAME_775, LevelParticlesPacket.read(body).particleTypeId)
    }

    @Test
    fun `a chunk comes out renumbered with its biomes untouched`() {
        val blocks = IntArray(4096) { if (it < 2048) STONE_776 else DIAMOND_BLOCK_776 }
        val biomes = IntArray(64) { 3 }
        val sections =
            ByteBuffer()
                .apply {
                    writeShort(2048)
                    writeShort(0)
                    PalettedContainer.write(this, blocks, 4, 8, 15)
                    PalettedContainer.write(this, biomes, 0, 3, 6)
                }.toByteArray()

        val rewritten = ChunkRewriter(TABLES_775.blockStates, 15).rewrite(sections)
        val input = ByteBuffer().apply { writeBytes(rewritten) }
        assertEquals(2048, input.readShort().toInt())
        assertEquals(0, input.readShort().toInt())
        val readBlocks = PalettedContainer.read(input, 4096, 8)
        assertEquals(STONE_775, readBlocks[0])
        assertEquals(DIAMOND_BLOCK_775, readBlocks[4095])
        assertTrue(PalettedContainer.read(input, 64, 3).all { it == 3 })
        assertEquals(0, input.readableBytes, "the walk left bytes behind")
    }

    @Test
    fun `a chunk of only air survives the walk`() {
        val sections =
            ByteBuffer()
                .apply {
                    writeShort(0)
                    writeShort(0)
                    PalettedContainer.write(this, IntArray(4096), 4, 8, 15)
                    PalettedContainer.write(this, IntArray(64) { 1 }, 0, 3, 6)
                }.toByteArray()
        val rewritten = ChunkRewriter(TABLES_775.blockStates, 15).rewrite(sections)
        val input = ByteBuffer().apply { writeBytes(rewritten) }
        input.readShort()
        input.readShort()
        assertTrue(PalettedContainer.read(input, 4096, 8).all { it == 0 })
    }

    @Test
    fun `an id with no counterpart says so rather than guessing`() {
        assertEquals(IdMap.NONE, TABLES_775.attribute.map(BOUNCINESS_776))
        assertEquals(IdMap.NONE, TABLES_775.entityType.map(SULFUR_CUBE_776))
    }

    @Test
    fun `a stack of nothing stays nothing`() {
        val body = clientbound(ContainerSetSlotPacket.ID) { ContainerSetSlotPacket(0, 1, 5, null).write(it) }
        assertNull(ContainerSetSlotPacket.read(body).item)
    }

    @Test
    fun `an id map answers the way back`() {
        assertEquals(OAK_PLANKS_776, TABLES_775.item.unmap(OAK_PLANKS_775))
        assertEquals(STONE_776, TABLES_775.blockStates.unmap(STONE_775))
    }

    private companion object {
        val BLOCK_POS =
            org.basaltmc.wire.protocol.play
                .BlockPos(1, 2, 3)
        const val STONE_776 = 1
        const val STONE_775 = 1
        const val DIAMOND_BLOCK_776 = 5309
        const val DIAMOND_BLOCK_775 = 5309
        const val CINNABAR_776 = 25926
        const val RED_SANDSTONE_775 = 13247
        const val OAK_PLANKS_776 = 63
        const val OAK_PLANKS_775 = 36
        const val CINNABAR_ITEM_776 = 40
        const val RED_SANDSTONE_ITEM_775 = 570
        const val MAX_HEALTH_776 = 23
        const val MAX_HEALTH_775 = 19
        const val BOUNCINESS_776 = 9
        const val SULFUR_CUBE_776 = 130
        const val FLAME_776 = 39
        const val FLAME_775 = 32
    }
}
