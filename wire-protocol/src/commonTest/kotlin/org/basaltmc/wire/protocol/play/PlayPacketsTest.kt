package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

class PlayPacketsTest {
    @Test
    fun globalPosRoundTrips() {
        val buffer = ByteBuffer()
        val pos = GlobalPos("minecraft:overworld", BlockPos(1, 64, -2))
        pos.write(buffer)
        assertEquals(pos, GlobalPos.read(buffer))
    }

    @Test
    fun spawnInfoRoundTripsWithoutDeathLocation() {
        val buffer = ByteBuffer()
        val info = SpawnInfo(0, "minecraft:overworld", 42L, Gamemode.SURVIVAL, 255, false, true, null, 0, 63)
        info.write(buffer)
        assertEquals(info, SpawnInfo.read(buffer))
    }

    @Test
    fun spawnInfoRoundTripsWithADeathLocation() {
        val buffer = ByteBuffer()
        val death = GlobalPos("minecraft:overworld", BlockPos(0, 70, 0))
        val info = SpawnInfo(0, "minecraft:overworld", 42L, Gamemode.SURVIVAL, 0, false, false, death, 10, 63)
        info.write(buffer)
        assertEquals(info, SpawnInfo.read(buffer))
    }

    @Test
    fun loginPlayRoundTrips() {
        val buffer = ByteBuffer()
        val info = SpawnInfo(0, "minecraft:overworld", 42L, Gamemode.SURVIVAL, 255, false, true, null, 0, 63)
        val packet =
            LoginPlayPacket(1, false, listOf("minecraft:overworld"), 20, 10, 10, false, true, false, info, false, false)
        packet.write(buffer)
        assertEquals(packet, LoginPlayPacket.read(buffer))
    }

    @Test
    fun heightmapRoundTrips() {
        val buffer = ByteBuffer()
        val heightmap = Heightmap(HeightmapType.MOTION_BLOCKING, longArrayOf(1L, 2L, 3L))
        heightmap.write(buffer)
        assertEquals(heightmap, Heightmap.read(buffer))
    }

    @Test
    fun mapChunkRoundTripsAFullyPopulatedChunk() {
        val buffer = ByteBuffer()
        val blockValues = IntArray(4096) { if (it < 256) BEDROCK_STATE else AIR_STATE }
        val chunkData = buildSingleSectionChunkData(blockValues)
        val beeNestTag = NbtCompound().apply { putString("custom_name", "Basalt nest") }
        val packet =
            MapChunkPacket(
                x = 3,
                z = -4,
                heightmaps = listOf(Heightmap(HeightmapType.MOTION_BLOCKING, longArrayOf(0L))),
                chunkData = chunkData,
                blockEntities = listOf(ChunkBlockEntityInfo(packedXZ = 0xa5, y = -12, typeId = 33, tag = beeNestTag)),
                skyLightMask = longArrayOf(0b1L),
                blockLightMask = longArrayOf(0L),
                emptySkyLightMask = longArrayOf(0L),
                emptyBlockLightMask = longArrayOf(0b1L),
                skyLight = listOf(NibbleArray.uniform(15)),
                blockLight = emptyList(),
            )
        packet.write(buffer)
        val result = MapChunkPacket.read(buffer)
        assertEquals(packet.x, result.x)
        assertEquals(packet.z, result.z)
        assertEquals(packet.heightmaps, result.heightmaps)
        assertEquals(packet.chunkData.toList(), result.chunkData.toList())
        assertEquals(packet.blockEntities, result.blockEntities)
        assertEquals(packet.skyLightMask.toList(), result.skyLightMask.toList())
        assertEquals(packet.emptyBlockLightMask.toList(), result.emptyBlockLightMask.toList())
        assertEquals(1, result.skyLight.size)
        assertEquals(packet.skyLight[0].toList(), result.skyLight[0].toList())
        assertEquals(0, result.blockLight.size)
    }

    @Test
    fun chunkBlockEntityInfoRoundTripsANullUpdateTag() {
        val buffer = ByteBuffer()
        val info = ChunkBlockEntityInfo(packedXZ = 0xff, y = 319, typeId = 33, tag = null)
        info.write(buffer)
        assertEquals(info, ChunkBlockEntityInfo.read(buffer))
    }

    @Test
    fun forgetLevelChunkRoundTripsPositiveAndNegativeCoordinates() {
        val buffer = ByteBuffer()
        val packet = ForgetLevelChunkPacket(chunkX = -12345, chunkZ = 67890)
        packet.write(buffer)
        val result = ForgetLevelChunkPacket.read(buffer)
        assertEquals(packet, result)
    }

    @Test
    fun updateLightRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            UpdateLightPacket(
                chunkX = 1,
                chunkZ = 2,
                skyLightMask = longArrayOf(0b101L),
                blockLightMask = longArrayOf(0L),
                emptySkyLightMask = longArrayOf(0L),
                emptyBlockLightMask = longArrayOf(0b1L),
                skyLight = listOf(NibbleArray.uniform(15), NibbleArray.uniform(0)),
                blockLight = emptyList(),
            )
        packet.write(buffer)
        assertEquals(packet, UpdateLightPacket.read(buffer))
    }

    @Test
    fun playerPositionRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayerPositionPacket(1, 8.5, 5.0, 8.5, 0.0, 0.0, 0.0, 90f, 0f, 0)
        packet.write(buffer)
        assertEquals(packet, PlayerPositionPacket.read(buffer))
    }

    @Test
    fun acceptTeleportationRoundTrips() {
        val buffer = ByteBuffer()
        val packet = AcceptTeleportationPacket(7)
        packet.write(buffer)
        assertEquals(packet, AcceptTeleportationPacket.read(buffer))
    }

    @Test
    fun spawnPositionRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SpawnPositionPacket(GlobalPos("minecraft:overworld", BlockPos(0, 4, 0)), 0f, 0f)
        packet.write(buffer)
        assertEquals(packet, SpawnPositionPacket.read(buffer))
    }

    @OptIn(ExperimentalUuidApi::class)
    @Test
    fun playerInfoAddRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            PlayerInfoUpdatePacket.initializing(
                listOf(
                    PlayerInfoEntry(
                        Uuid.random(),
                        "Steve",
                        listed = true,
                        latencyMillis = 42,
                        gamemode = Gamemode.ADVENTURE,
                    ),
                ),
            )
        packet.write(buffer)
        assertEquals(packet, PlayerInfoUpdatePacket.read(buffer))
    }

    @OptIn(ExperimentalUuidApi::class)
    @Test
    fun playerInfoRemoveRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayerInfoRemovePacket(listOf(Uuid.random(), Uuid.random()))
        packet.write(buffer)
        assertEquals(packet, PlayerInfoRemovePacket.read(buffer))
    }

    @Test
    fun entityTeleportRoundTrips() {
        val buffer = ByteBuffer()
        val packet = EntityTeleportPacket(5, 1.0, 2.0, 3.0, -64f, 32f, true)
        packet.write(buffer)
        assertEquals(packet, EntityTeleportPacket.read(buffer))
    }

    @Test
    fun playKeepAliveRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayKeepAlivePacket(9999999999L)
        packet.write(buffer)
        assertEquals(packet, PlayKeepAlivePacket.read(buffer))
    }

    @Test
    fun systemChatRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SystemChatPacket(Component.text("hello"), isActionBar = false)
        packet.write(buffer)
        assertEquals(packet, SystemChatPacket.read(buffer))
    }

    @Test
    fun playDisconnectRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayDisconnectPacket(Component.text("kicked"))
        packet.write(buffer)
        assertEquals(packet, PlayDisconnectPacket.read(buffer))
    }

    @Test
    fun chatMessageRoundTripsWithoutASignature() {
        val buffer = ByteBuffer()
        val packet = ChatMessagePacket("hi", 1L, 2L, null, 0, byteArrayOf(0, 0, 0), 0.toByte())
        writeChatMessage(buffer, packet)
        val result = ChatMessagePacket.read(buffer)
        assertEquals(packet.message, result.message)
        assertNull(result.signature)
    }

    @Test
    fun chatMessageRoundTripsWithASignature() {
        val buffer = ByteBuffer()
        val signature = ByteArray(256) { it.toByte() }
        val packet = ChatMessagePacket("signed message", 1L, 2L, signature, 3, byteArrayOf(1, 2, 3), 9.toByte())
        writeChatMessage(buffer, packet)
        val result = ChatMessagePacket.read(buffer)
        assertEquals(packet, result)
    }

    @Test
    fun serverboundPositionRoundTrips() {
        val buffer = ByteBuffer()
        buffer.writeDouble(1.0)
        buffer.writeDouble(2.0)
        buffer.writeDouble(3.0)
        buffer.writeByte(1.toByte())
        val result = ServerboundPositionPacket.read(buffer)
        assertEquals(ServerboundPositionPacket(1.0, 2.0, 3.0, onGround = true), result)
    }

    @Test
    fun serverboundPositionLookRoundTrips() {
        val buffer = ByteBuffer()
        buffer.writeDouble(1.0)
        buffer.writeDouble(2.0)
        buffer.writeDouble(3.0)
        buffer.writeFloat(90f)
        buffer.writeFloat(0f)
        buffer.writeByte(0.toByte())
        val result = ServerboundPositionLookPacket.read(buffer)
        assertEquals(ServerboundPositionLookPacket(1.0, 2.0, 3.0, 90f, 0f, onGround = false), result)
    }

    @Test
    fun clientboundBlockUpdateRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ClientboundBlockUpdatePacket(BlockPos(10, 64, -20), blockStateId = 12345)
        packet.write(buffer)
        assertEquals(packet, ClientboundBlockUpdatePacket.read(buffer))
    }

    @Test
    fun clientboundBlockChangedAckRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ClientboundBlockChangedAckPacket(sequence = 42)
        packet.write(buffer)
        assertEquals(packet, ClientboundBlockChangedAckPacket.read(buffer))
    }

    @Test
    fun serverboundPlayerActionRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            ServerboundPlayerActionPacket(
                action = ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK,
                pos = BlockPos(1, 65, -3),
                direction = Direction.UP,
                sequence = 7,
            )
        packet.write(buffer)
        assertEquals(packet, ServerboundPlayerActionPacket.read(buffer))
    }

    @Test
    fun serverboundPlayerActionRoundTripsForEveryActionAndDirection() {
        for (action in ServerboundPlayerActionPacket.Action.entries) {
            for (direction in Direction.entries) {
                val buffer = ByteBuffer()
                val packet = ServerboundPlayerActionPacket(action, BlockPos(0, 0, 0), direction, sequence = 1)
                packet.write(buffer)
                assertEquals(packet, ServerboundPlayerActionPacket.read(buffer))
            }
        }
    }

    @Test
    fun blockHitResultRoundTrips() {
        val buffer = ByteBuffer()
        val hit =
            BlockHitResult(
                BlockPos(5, 70, 5),
                Direction.NORTH,
                0.5f,
                1.0f,
                0.0f,
                inside = false,
                worldBorderHit = false,
            )
        hit.write(buffer)
        assertEquals(hit, BlockHitResult.read(buffer))
    }

    @Test
    fun serverboundUseItemOnRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.OFF_HAND,
                blockHit =
                    BlockHitResult(
                        BlockPos(2, 64, 2),
                        Direction.EAST,
                        0.25f,
                        0.75f,
                        0.5f,
                        inside = true,
                        worldBorderHit = false,
                    ),
                sequence = 99,
            )
        packet.write(buffer)
        assertEquals(packet, ServerboundUseItemOnPacket.read(buffer))
    }
}

private const val AIR_STATE = 0
private const val BEDROCK_STATE = 85

private fun buildSingleSectionChunkData(blockValues: IntArray): ByteArray {
    val buffer = ByteBuffer()
    buffer.writeShort(256.toShort())
    PalettedContainer.write(
        buffer,
        blockValues,
        PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
        PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
        8,
    )
    PalettedContainer.write(
        buffer,
        IntArray(64),
        PalettedContainer.BIOME_MIN_INDIRECT_BITS,
        PalettedContainer.BIOME_MAX_INDIRECT_BITS,
        6,
    )
    return buffer.toByteArray()
}

private fun writeChatMessage(
    buffer: ByteBuffer,
    packet: ChatMessagePacket,
) {
    buffer.writeProtocolString(packet.message, 256)
    buffer.writeLong(packet.timestamp)
    buffer.writeLong(packet.salt)
    buffer.writeBoolean(packet.signature != null)
    packet.signature?.let { buffer.writeBytes(it) }
    buffer.writeVarInt(packet.offset)
    buffer.writeBytes(packet.acknowledged)
    buffer.writeByte(packet.checksum)

    @Test
    fun aGameModeOnlyPlayerInfoUpdateCarriesJustThatField() {
        val buffer = ByteBuffer()
        val uuid = Uuid.random()
        val packet = PlayerInfoUpdatePacket.gameMode(uuid, Gamemode.SPECTATOR)
        packet.write(buffer)

        // One action bit (update_game_mode is ordinal 2), one entry: uuid then the mode, nothing else.
        assertEquals(packet, PlayerInfoUpdatePacket.read(ByteBuffer().also { it.writeBytes(buffer.toByteArray()) }))
        assertEquals(setOf(PlayerInfoUpdatePacket.Action.UPDATE_GAME_MODE), packet.actions)
        assertEquals(0b100, buffer.readByte().toInt())
    }
}
