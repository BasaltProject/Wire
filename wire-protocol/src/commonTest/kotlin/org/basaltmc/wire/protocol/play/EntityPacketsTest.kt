package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

@OptIn(ExperimentalUuidApi::class)
class EntityPacketsTest {
    @Test
    fun lpVec3RoundTripsZero() {
        val buffer = ByteBuffer()
        LpVec3.write(buffer, 0.0, 0.0, 0.0)
        assertEquals(Triple(0.0, 0.0, 0.0), LpVec3.read(buffer))
    }

    @Test
    fun lpVec3RoundTripsWithinQuantizationError() {
        val buffer = ByteBuffer()
        LpVec3.write(buffer, 0.2, -0.4, 1.5)
        val (x, y, z) = LpVec3.read(buffer)
        assertTrue(abs(0.2 - x) < 0.001)
        assertTrue(abs(-0.4 - y) < 0.001)
        assertTrue(abs(1.5 - z) < 0.001)
    }

    @Test
    fun spawnEntityRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            SpawnEntityPacket(
                entityId = 42,
                uuid = Uuid.parse("11111111-2222-3333-4444-555555555555"),
                entityType = SpawnEntityPacket.ITEM_ENTITY_TYPE,
                x = 10.5,
                y = 64.0,
                z = -10.5,
            )
        packet.write(buffer)
        assertEquals(packet, SpawnEntityPacket.read(buffer))
    }

    @Test
    fun setEntityDataRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SetEntityDataPacket.item(entityId = 7, stack = WireItemStack(itemId = 5, count = 3))
        packet.write(buffer)
        assertEquals(packet, SetEntityDataPacket.read(buffer))
    }

    @Test
    fun setEntityDataRoundTripsEmptyStack() {
        val buffer = ByteBuffer()
        val packet = SetEntityDataPacket.item(entityId = 7, stack = null)
        packet.write(buffer)
        assertEquals(packet, SetEntityDataPacket.read(buffer))
    }

    @Test
    fun villageEntityMetadataSerializersRoundTrip() {
        val buffer = ByteBuffer()
        val packet =
            SetEntityDataPacket(
                entityId = 42,
                entries =
                    listOf(
                        SetEntityDataPacket.Entry.Ints(19, 261),
                        SetEntityDataPacket.Entry.Floats(9, 24f),
                        SetEntityDataPacket.Entry.Booleans(16, true),
                        SetEntityDataPacket.Entry.Rotations(17, 3.978817f, 1.5454245f, 0f),
                        SetEntityDataPacket.Entry.VillagerData(20, typeId = 0, professionId = 11, level = 1),
                        SetEntityDataPacket.Entry.RegistryId(20, 1),
                    ),
            )

        packet.write(buffer)

        assertEquals(packet, SetEntityDataPacket.read(buffer))
    }

    @Test
    fun equipmentRoundTripsContinuationAndEmptySlots() {
        val buffer = ByteBuffer()
        val packet =
            SetEquipmentPacket(
                entityId = 17,
                slots =
                    listOf(
                        SetEquipmentPacket.Slot(SetEquipmentPacket.Type.MAINHAND, null),
                        SetEquipmentPacket.Slot(
                            SetEquipmentPacket.Type.HEAD,
                            WireItemStack(itemId = 994, count = 1),
                        ),
                    ),
            )

        packet.write(buffer)

        assertEquals(packet, SetEquipmentPacket.read(buffer))
    }

    @Test
    fun itemFrameUsesItsHierarchySpecificStackIndex() {
        val buffer = ByteBuffer()
        val packet = SetEntityDataPacket.itemFrame(entityId = 9, stack = WireItemStack(itemId = 890, count = 1))
        packet.write(buffer)

        val decoded = SetEntityDataPacket.read(buffer)
        assertEquals(packet, decoded)
        assertEquals(
            SetEntityDataPacket.ITEM_FRAME_STACK_INDEX,
            (decoded.entries.single() as SetEntityDataPacket.Entry.Item).index,
        )
    }

    @Test
    fun clientboundTakeItemEntityRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ClientboundTakeItemEntityPacket(collectedEntityId = 9, collectorEntityId = 1, pickupItemCount = 16)
        packet.write(buffer)
        assertEquals(packet, ClientboundTakeItemEntityPacket.read(buffer))
    }

    @Test
    fun serverboundPickItemFromBlockRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ServerboundPickItemFromBlockPacket(BlockPos(1, 64, -1), includeData = true)
        packet.write(buffer)
        assertEquals(packet, ServerboundPickItemFromBlockPacket.read(buffer))
    }

    @Test
    fun skinPartsMetadataRoundTrips() {
        val buffer = ByteBuffer()
        // Index 16 is `Avatar.DATA_PLAYER_MODE_CUSTOMISATION`, counted down the real 26.2 hierarchy
        // (Entity 0-7, LivingEntity 8-14, Avatar 15-16), not where an older version would put it.
        val packet = SetEntityDataPacket.skinParts(entityId = 3, skinParts = 0x7F)
        packet.write(buffer)

        val decoded = SetEntityDataPacket.read(buffer)
        assertEquals(packet, decoded)
        assertEquals(
            SetEntityDataPacket.PLAYER_SKIN_PARTS_INDEX,
            (decoded.entries.single() as SetEntityDataPacket.Entry.Bytes).index,
        )
    }

    @Test
    fun setEntityDataRejectsAnUnknownSerializer() {
        val buffer = ByteBuffer()
        buffer.writeVarInt(1)
        buffer.writeByte(4)
        buffer.writeVarInt(99) // a serializer this build has no reader for
        buffer.writeByte(0xFF.toByte())
        assertFailsWith<org.basaltmc.wire.protocol.ProtocolException> { SetEntityDataPacket.read(buffer) }
    }

    @Test
    fun trackedWaypointRoundTripsWithAndWithoutColour() {
        for (color in listOf(null, 0x55FFFF)) {
            val buffer = ByteBuffer()
            val packet =
                TrackedWaypointPacket(
                    operation = TrackedWaypointPacket.Operation.TRACK,
                    entityUuid = Uuid.fromLongs(1L, 2L),
                    style = "minecraft:default",
                    color = color,
                    x = 10,
                    y = 64,
                    z = -20,
                )
            packet.write(buffer)
            assertEquals(packet, TrackedWaypointPacket.read(buffer))
        }
    }

    @Test
    fun trackedWaypointWritesTheColourAsThreeBytes() {
        // parity: `ByteBufCodecs.RGB_COLOR`. A fourth byte shifts the waypoint type and the client
        // drops the connection decoding it, which is exactly what happened with `/waypoint modify`.
        val withColour = ByteBuffer()
        val without = ByteBuffer()
        val base =
            TrackedWaypointPacket(
                operation = TrackedWaypointPacket.Operation.UPDATE,
                entityUuid = Uuid.fromLongs(1L, 2L),
                style = "minecraft:default",
                color = null,
                x = 0,
                y = 0,
                z = 0,
            )
        base.copy(color = 0x123456).write(withColour)
        base.write(without)
        assertEquals(3, withColour.readableBytes - without.readableBytes)
    }

    /** parity: `Entity`'s shared flags at index 0 and its air supply at index 1. */
    @Test
    fun submersionMetadataRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SetEntityDataPacket.submersion(entityId = 42, airSupply = 140, onFire = true)
        packet.write(buffer)

        val read = SetEntityDataPacket.read(buffer)
        assertEquals(packet, read)
        assertEquals(
            SetEntityDataPacket.Entry.Bytes(SetEntityDataPacket.SHARED_FLAGS_INDEX, 1),
            read.entries.first(),
        )
        assertEquals(
            SetEntityDataPacket.Entry.Ints(SetEntityDataPacket.AIR_SUPPLY_INDEX, 140),
            read.entries.last(),
        )
    }
}
