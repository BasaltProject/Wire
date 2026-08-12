package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtEnd
import org.basaltmc.wire.protocol.login.GameProfileProperty
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readUuid
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeUuid
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentNbt
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** A world position in another dimension, e.g. a bed/respawn anchor or a death location. */
public data class GlobalPos(
    val dimensionName: String,
    val location: BlockPos,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(dimensionName)
        buffer.writeLong(location.pack())
    }

    public companion object {
        public fun read(buffer: ByteBuffer): GlobalPos =
            GlobalPos(buffer.readProtocolString(), BlockPos.unpack(buffer.readLong()))
    }
}

public enum class Gamemode {
    SURVIVAL,
    CREATIVE,
    ADVENTURE,
    SPECTATOR,
}

/** The part of [LoginPlayPacket] vanilla calls `CommonPlayerSpawnInfo`. */
public data class SpawnInfo(
    val dimension: Int,
    val dimensionName: String,
    val hashedSeed: Long,
    val gamemode: Gamemode,
    val previousGamemode: Int,
    val isDebug: Boolean,
    val isFlat: Boolean,
    val death: GlobalPos?,
    val portalCooldown: Int,
    val seaLevel: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(dimension)
        buffer.writeProtocolString(dimensionName)
        buffer.writeLong(hashedSeed)
        buffer.writeByte(gamemode.ordinal.toByte())
        buffer.writeByte(previousGamemode.toByte())
        buffer.writeBoolean(isDebug)
        buffer.writeBoolean(isFlat)
        buffer.writeBoolean(death != null)
        death?.write(buffer)
        buffer.writeVarInt(portalCooldown)
        buffer.writeVarInt(seaLevel)
    }

    public companion object {
        public fun read(buffer: ByteBuffer): SpawnInfo =
            SpawnInfo(
                dimension = buffer.readVarInt(),
                dimensionName = buffer.readProtocolString(),
                hashedSeed = buffer.readLong(),
                gamemode = Gamemode.entries[buffer.readByte().toInt()],
                previousGamemode = buffer.readByte().toInt() and 0xFF,
                isDebug = buffer.readBoolean(),
                isFlat = buffer.readBoolean(),
                death = if (buffer.readBoolean()) GlobalPos.read(buffer) else null,
                portalCooldown = buffer.readVarInt(),
                seaLevel = buffer.readVarInt(),
            )
    }
}

/** Clientbound, id 0x31: enters Play. Mirrors `ClientboundLoginPacket`. */
public data class LoginPlayPacket(
    val entityId: Int,
    val isHardcore: Boolean,
    val worldNames: List<String>,
    val maxPlayers: Int,
    val viewDistance: Int,
    val simulationDistance: Int,
    val reducedDebugInfo: Boolean,
    val enableRespawnScreen: Boolean,
    val doLimitedCrafting: Boolean,
    val spawnInfo: SpawnInfo,
    val onlineMode: Boolean,
    val enforcesSecureChat: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeInt(entityId)
        buffer.writeBoolean(isHardcore)
        buffer.writeVarInt(worldNames.size)
        for (name in worldNames) buffer.writeProtocolString(name)
        buffer.writeVarInt(maxPlayers)
        buffer.writeVarInt(viewDistance)
        buffer.writeVarInt(simulationDistance)
        buffer.writeBoolean(reducedDebugInfo)
        buffer.writeBoolean(enableRespawnScreen)
        buffer.writeBoolean(doLimitedCrafting)
        spawnInfo.write(buffer)
        buffer.writeBoolean(onlineMode)
        buffer.writeBoolean(enforcesSecureChat)
    }

    public companion object {
        public const val ID: Int = 0x31

        public fun read(buffer: ByteBuffer): LoginPlayPacket {
            val entityId = buffer.readInt()
            val isHardcore = buffer.readBoolean()
            val worldCount = buffer.readVarInt()
            val worldNames = List(worldCount) { buffer.readProtocolString() }
            val maxPlayers = buffer.readVarInt()
            val viewDistance = buffer.readVarInt()
            val simulationDistance = buffer.readVarInt()
            val reducedDebugInfo = buffer.readBoolean()
            val enableRespawnScreen = buffer.readBoolean()
            val doLimitedCrafting = buffer.readBoolean()
            val spawnInfo = SpawnInfo.read(buffer)
            val onlineMode = buffer.readBoolean()
            val enforcesSecureChat = buffer.readBoolean()
            return LoginPlayPacket(
                entityId,
                isHardcore,
                worldNames,
                maxPlayers,
                viewDistance,
                simulationDistance,
                reducedDebugInfo,
                enableRespawnScreen,
                doLimitedCrafting,
                spawnInfo,
                onlineMode,
                enforcesSecureChat,
            )
        }
    }
}

public enum class HeightmapType {
    WORLD_SURFACE_WG,
    WORLD_SURFACE,
    OCEAN_FLOOR_WG,
    OCEAN_FLOOR,
    MOTION_BLOCKING,
    MOTION_BLOCKING_NO_LEAVES,
}

public data class Heightmap(
    val type: HeightmapType,
    val data: LongArray,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(type.ordinal)
        buffer.writeVarInt(data.size)
        for (long in data) buffer.writeLong(long)
    }

    override fun equals(other: Any?): Boolean =
        other is Heightmap && type == other.type && data.contentEquals(other.data)

    override fun hashCode(): Int = 31 * type.hashCode() + data.contentHashCode()

    public companion object {
        public fun read(buffer: ByteBuffer): Heightmap {
            val type = HeightmapType.entries[buffer.readVarInt()]
            val length = buffer.readVarInt()
            return Heightmap(type, LongArray(length) { buffer.readLong() })
        }
    }
}

/** Bitset per world section, matching `java.util.BitSet.toLongArray()`. */
private fun ByteBuffer.writeLongBitset(bits: LongArray) {
    writeVarInt(bits.size)
    for (long in bits) writeLong(long)
}

private fun ByteBuffer.readLongBitset(): LongArray {
    val length = readVarInt()
    return LongArray(length) { readLong() }
}

private fun ByteBuffer.writeByteArrayList(arrays: List<ByteArray>) {
    writeVarInt(arrays.size)
    for (array in arrays) {
        writeVarInt(array.size)
        writeBytes(array)
    }
}

private fun ByteBuffer.readByteArrayList(): List<ByteArray> {
    val count = readVarInt()
    return List(count) { readBytes(readVarInt()) }
}

/**
 * Clientbound, id 0x26: a miscellaneous world/game-state notification, one of a fixed set of
 * numeric event types (see companion constants). Mirrors `ClientboundGameEventPacket`.
 *
 * [LEVEL_CHUNKS_LOAD_START] is what real vanilla's join sequence sends right before chunk data, and
 * the client only starts tracking/waiting for chunks once it sees it (confirmed live: without it,
 * the "loading terrain" screen never dismisses, even with correct chunks and a correct chunk-batch
 * wrapper). [CHANGE_GAME_MODE] carries the new mode's id in [param]: vanilla has no dedicated
 * packet for a game mode change, it rides on this one.
 */
public data class GameEventPacket(
    val event: Int,
    val param: Float,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeByte(event.toByte())
        buffer.writeFloat(param)
    }

    public companion object {
        public const val ID: Int = 0x26
        public const val CHANGE_GAME_MODE: Int = 3
        public const val LEVEL_CHUNKS_LOAD_START: Int = 13

        public fun read(buffer: ByteBuffer): GameEventPacket =
            GameEventPacket(buffer.readByte().toInt() and 0xFF, buffer.readFloat())
    }
}

/** Serverbound, id 0x2c: the client confirming it has finished loading the world it spawned into. Empty body. Mirrors `ServerboundPlayerLoadedPacket`. */
public object PlayerLoadedPacket {
    public const val ID: Int = 0x2c
}

/** Network form of `ClientboundLevelChunkPacketData.BlockEntityInfo`. */
public data class ChunkBlockEntityInfo(
    val packedXZ: Int,
    val y: Int,
    val typeId: Int,
    val tag: NbtCompound?,
) {
    init {
        require(packedXZ in 0..255)
        require(y in Short.MIN_VALUE..Short.MAX_VALUE)
        require(typeId >= 0)
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeByte(packedXZ.toByte())
        buffer.writeShort(y.toShort())
        buffer.writeVarInt(typeId)
        buffer.writeAnonymousNbt(tag ?: NbtEnd)
    }

    public companion object {
        public fun read(buffer: ByteBuffer): ChunkBlockEntityInfo {
            val packedXZ = buffer.readByte().toInt() and 0xFF
            val y = buffer.readShort().toInt()
            val typeId = buffer.readVarInt()
            val nbt = buffer.readAnonymousNbt()
            val tag =
                when (nbt) {
                    NbtEnd -> null
                    is NbtCompound -> nbt
                    else -> error("block entity update tag is not a compound")
                }
            return ChunkBlockEntityInfo(packedXZ, y, typeId, tag)
        }
    }
}

/** Clientbound, id 0x2d: one chunk column's block/biome data. Mirrors `ClientboundLevelChunkPacket`. */
public data class MapChunkPacket(
    val x: Int,
    val z: Int,
    val heightmaps: List<Heightmap>,
    val chunkData: ByteArray,
    val blockEntities: List<ChunkBlockEntityInfo>,
    val skyLightMask: LongArray,
    val blockLightMask: LongArray,
    val emptySkyLightMask: LongArray,
    val emptyBlockLightMask: LongArray,
    val skyLight: List<ByteArray>,
    val blockLight: List<ByteArray>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeInt(x)
        buffer.writeInt(z)
        buffer.writeVarInt(heightmaps.size)
        for (heightmap in heightmaps) heightmap.write(buffer)
        buffer.writeVarInt(chunkData.size)
        buffer.writeBytes(chunkData)
        buffer.writeVarInt(blockEntities.size)
        for (blockEntity in blockEntities) blockEntity.write(buffer)
        buffer.writeLongBitset(skyLightMask)
        buffer.writeLongBitset(blockLightMask)
        buffer.writeLongBitset(emptySkyLightMask)
        buffer.writeLongBitset(emptyBlockLightMask)
        buffer.writeByteArrayList(skyLight)
        buffer.writeByteArrayList(blockLight)
    }

    public companion object {
        public const val ID: Int = 0x2d

        public fun read(buffer: ByteBuffer): MapChunkPacket {
            val x = buffer.readInt()
            val z = buffer.readInt()
            val heightmapCount = buffer.readVarInt()
            val heightmaps = List(heightmapCount) { Heightmap.read(buffer) }
            val chunkData = buffer.readBytes(buffer.readVarInt())
            val blockEntityCount = buffer.readVarInt()
            val blockEntities = List(blockEntityCount) { ChunkBlockEntityInfo.read(buffer) }
            val skyLightMask = buffer.readLongBitset()
            val blockLightMask = buffer.readLongBitset()
            val emptySkyLightMask = buffer.readLongBitset()
            val emptyBlockLightMask = buffer.readLongBitset()
            val skyLight = buffer.readByteArrayList()
            val blockLight = buffer.readByteArrayList()
            return MapChunkPacket(
                x,
                z,
                heightmaps,
                chunkData,
                blockEntities,
                skyLightMask,
                blockLightMask,
                emptySkyLightMask,
                emptyBlockLightMask,
                skyLight,
                blockLight,
            )
        }
    }
}

/** Clientbound, id 0x0c: marks the start of a batch of [MapChunkPacket] sends. Empty body. Mirrors `ClientboundChunkBatchStartPacket`. */
public object ChunkBatchStartPacket {
    public const val ID: Int = 0x0c
}

/**
 * Clientbound, id 0x25: tells the client to discard a chunk column it no longer needs (the player
 * moved far enough that it fell outside their view distance). Mirrors
 * `ClientboundForgetLevelChunkPacket`, whose body is a single `ChunkPos.pack`ed `Long`
 * (`x & 0xFFFFFFFF | (z & 0xFFFFFFFF) << 32`), not two separate ints like [MapChunkPacket].
 */
public data class ForgetLevelChunkPacket(
    val chunkX: Int,
    val chunkZ: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong((chunkX.toLong() and 0xFFFFFFFFL) or ((chunkZ.toLong() and 0xFFFFFFFFL) shl 32))
    }

    public companion object {
        public const val ID: Int = 0x25

        public fun read(buffer: ByteBuffer): ForgetLevelChunkPacket {
            val packed = buffer.readLong()
            return ForgetLevelChunkPacket((packed and 0xFFFFFFFFL).toInt(), (packed ushr 32).toInt())
        }
    }
}

/**
 * Clientbound, id 0x0b: marks the end of a chunk batch, with how many chunks it contained. Mirrors
 * `ClientboundChunkBatchFinishedPacket`. Without wrapping chunk sends in a start/finish pair
 * (confirmed live), a real client's "loading terrain" screen never dismisses: it tracks loading
 * progress by chunk batch, not by individual [MapChunkPacket] arrivals.
 */
public data class ChunkBatchFinishedPacket(
    val batchSize: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(batchSize)
    }

    public companion object {
        public const val ID: Int = 0x0b

        public fun read(buffer: ByteBuffer): ChunkBatchFinishedPacket =
            ChunkBatchFinishedPacket(
                buffer.readVarInt(),
            )
    }
}

/**
 * Serverbound, id 0x0b: the client's ack for a chunk batch, carrying its desired chunks-per-tick
 * rate. Mirrors `ServerboundChunkBatchReceivedPacket`.
 */
public data class ChunkBatchReceivedPacket(
    val desiredChunksPerTick: Float,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeFloat(desiredChunksPerTick)
    }

    public companion object {
        public const val ID: Int = 0x0b

        public fun read(buffer: ByteBuffer): ChunkBatchReceivedPacket =
            ChunkBatchReceivedPacket(
                buffer.readFloat(),
            )
    }
}

/** Clientbound, id 0x30: the light half of chunk sync, sent alongside [MapChunkPacket]. Mirrors `ClientboundLightUpdatePacket`. */
public data class UpdateLightPacket(
    val chunkX: Int,
    val chunkZ: Int,
    val skyLightMask: LongArray,
    val blockLightMask: LongArray,
    val emptySkyLightMask: LongArray,
    val emptyBlockLightMask: LongArray,
    val skyLight: List<ByteArray>,
    val blockLight: List<ByteArray>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(chunkX)
        buffer.writeVarInt(chunkZ)
        buffer.writeLongBitset(skyLightMask)
        buffer.writeLongBitset(blockLightMask)
        buffer.writeLongBitset(emptySkyLightMask)
        buffer.writeLongBitset(emptyBlockLightMask)
        buffer.writeByteArrayList(skyLight)
        buffer.writeByteArrayList(blockLight)
    }

    public companion object {
        public const val ID: Int = 0x30

        public fun read(buffer: ByteBuffer): UpdateLightPacket =
            UpdateLightPacket(
                chunkX = buffer.readVarInt(),
                chunkZ = buffer.readVarInt(),
                skyLightMask = buffer.readLongBitset(),
                blockLightMask = buffer.readLongBitset(),
                emptySkyLightMask = buffer.readLongBitset(),
                emptyBlockLightMask = buffer.readLongBitset(),
                skyLight = buffer.readByteArrayList(),
                blockLight = buffer.readByteArrayList(),
            )
    }

    override fun equals(other: Any?): Boolean =
        other is UpdateLightPacket &&
            chunkX == other.chunkX &&
            chunkZ == other.chunkZ &&
            skyLightMask.contentEquals(other.skyLightMask) &&
            blockLightMask.contentEquals(other.blockLightMask) &&
            emptySkyLightMask.contentEquals(other.emptySkyLightMask) &&
            emptyBlockLightMask.contentEquals(other.emptyBlockLightMask) &&
            skyLight.size == other.skyLight.size &&
            skyLight.indices.all { skyLight[it].contentEquals(other.skyLight[it]) } &&
            blockLight.size == other.blockLight.size &&
            blockLight.indices.all { blockLight[it].contentEquals(other.blockLight[it]) }

    override fun hashCode(): Int = chunkX * 31 + chunkZ
}

/** Clientbound, id 0x48: authoritative position sync (initial spawn or a server-forced teleport). Mirrors `ClientboundPlayerPositionPacket`. */
public data class PlayerPositionPacket(
    val teleportId: Int,
    val x: Double,
    val y: Double,
    val z: Double,
    val dx: Double,
    val dy: Double,
    val dz: Double,
    val yaw: Float,
    val pitch: Float,
    val relativeFlags: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(teleportId)
        buffer.writeDouble(x)
        buffer.writeDouble(y)
        buffer.writeDouble(z)
        buffer.writeDouble(dx)
        buffer.writeDouble(dy)
        buffer.writeDouble(dz)
        buffer.writeFloat(yaw)
        buffer.writeFloat(pitch)
        buffer.writeInt(relativeFlags)
    }

    public companion object {
        public const val ID: Int = 0x48

        public fun read(buffer: ByteBuffer): PlayerPositionPacket =
            PlayerPositionPacket(
                teleportId = buffer.readVarInt(),
                x = buffer.readDouble(),
                y = buffer.readDouble(),
                z = buffer.readDouble(),
                dx = buffer.readDouble(),
                dy = buffer.readDouble(),
                dz = buffer.readDouble(),
                yaw = buffer.readFloat(),
                pitch = buffer.readFloat(),
                relativeFlags = buffer.readInt(),
            )
    }
}

/** Serverbound, id 0x00: acknowledges a [PlayerPositionPacket] teleport. Mirrors `ServerboundAcceptTeleportationPacket`. */
public data class AcceptTeleportationPacket(
    val teleportId: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(teleportId)
    }

    public companion object {
        public const val ID: Int = 0

        public fun read(buffer: ByteBuffer): AcceptTeleportationPacket = AcceptTeleportationPacket(buffer.readVarInt())
    }
}

/** Clientbound, id 0x61: the world spawn point shown on the compass. Mirrors `ClientboundSetDefaultSpawnPositionPacket`. */
public data class SpawnPositionPacket(
    val globalPos: GlobalPos,
    val yaw: Float,
    val pitch: Float,
) {
    public fun write(buffer: ByteBuffer) {
        globalPos.write(buffer)
        buffer.writeFloat(yaw)
        buffer.writeFloat(pitch)
    }

    public companion object {
        public const val ID: Int = 0x61

        public fun read(buffer: ByteBuffer): SpawnPositionPacket =
            SpawnPositionPacket(GlobalPos.read(buffer), buffer.readFloat(), buffer.readFloat())
    }
}

/**
 * Clientbound, id 0x5e: tells the client which chunk is the center of its loaded area. Mirrors
 * `ClientboundSetChunkCacheCenterPacket`. Sent on join before any chunk data: without it (confirmed
 * live) a real client's "loading terrain" screen never dismisses, since it never learns which
 * chunks to expect.
 */
public data class SetChunkCacheCenterPacket(
    val chunkX: Int,
    val chunkZ: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(chunkX)
        buffer.writeVarInt(chunkZ)
    }

    public companion object {
        public const val ID: Int = 0x5e

        public fun read(buffer: ByteBuffer): SetChunkCacheCenterPacket =
            SetChunkCacheCenterPacket(buffer.readVarInt(), buffer.readVarInt())
    }
}

/**
 * Clientbound, id 0x5f: how many chunks around the center the client should expect. Mirrors
 * `ClientboundSetChunkCacheRadiusPacket`. Must match how many chunks the server actually sends
 * (confirmed live: declaring a larger radius than sent, or none at all, leaves the client waiting
 * for chunks that never arrive).
 */
public data class SetChunkCacheRadiusPacket(
    val radius: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(radius)
    }

    public companion object {
        public const val ID: Int = 0x5f

        public fun read(buffer: ByteBuffer): SetChunkCacheRadiusPacket =
            SetChunkCacheRadiusPacket(
                buffer.readVarInt(),
            )
    }
}

/** Clientbound, id 0x6f: the server's simulation distance. Mirrors `ClientboundSetSimulationDistancePacket`. */
public data class SetSimulationDistancePacket(
    val simulationDistance: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(simulationDistance)
    }

    public companion object {
        public const val ID: Int = 0x6f

        public fun read(buffer: ByteBuffer): SetSimulationDistancePacket =
            SetSimulationDistancePacket(buffer.readVarInt())
    }
}

/** One tab-list entry. Which of these fields actually travel is decided by the packet's action set. */
public data class PlayerInfoEntry(
    val uuid: Uuid,
    val name: String = "",
    val listed: Boolean = false,
    val latencyMillis: Int = 0,
    val gamemode: Gamemode = Gamemode.SURVIVAL,
    /**
     * The signed profile properties from the session server, which is where a skin and cape live
     * (the `textures` property). A client renders the default skin for a player it was told nothing
     * about, so these travelling is the whole difference between everyone looking like Steve and
     * everyone looking like themselves. Empty in offline mode, since nothing signs them there.
     */
    val properties: List<GameProfileProperty> = emptyList(),
)

/**
 * Clientbound, id 0x46: adds or updates tab list entries. Mirrors `ClientboundPlayerInfoUpdatePacket`.
 * Carries a set of [Action]s and, per entry, the fields those actions call for, in the enum's
 * order: the set is a bitset over that enum, so the ordinals below are protocol.
 *
 * [Action.UPDATE_GAME_MODE] is not cosmetic: a client reads its own tab-list entry to decide it is
 * a spectator, so the `game_event` alone switches the HUD while the player still collides.
 *
 * [Action.INITIALIZE_CHAT], [Action.UPDATE_DISPLAY_NAME], [Action.UPDATE_LIST_ORDER] and
 * [Action.UPDATE_HAT] exist to hold their bitset positions; writing one throws.
 */
@OptIn(ExperimentalUuidApi::class)
public data class PlayerInfoUpdatePacket(
    val actions: Set<Action>,
    val entries: List<PlayerInfoEntry>,
) {
    public enum class Action {
        ADD_PLAYER,
        INITIALIZE_CHAT,
        UPDATE_GAME_MODE,
        UPDATE_LISTED,
        UPDATE_LATENCY,
        UPDATE_DISPLAY_NAME,
        UPDATE_LIST_ORDER,
        UPDATE_HAT,
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeByte(actions.fold(0) { bits, action -> bits or (1 shl action.ordinal) }.toByte())
        buffer.writeVarInt(entries.size)
        for (entry in entries) {
            buffer.writeUuid(entry.uuid)
            for (action in Action.entries) {
                if (action !in actions) continue
                when (action) {
                    Action.ADD_PLAYER -> {
                        buffer.writeProtocolString(entry.name, 16)
                        buffer.writeVarInt(entry.properties.size)
                        for (property in entry.properties) {
                            buffer.writeProtocolString(property.name)
                            buffer.writeProtocolString(property.value)
                            buffer.writeBoolean(property.signature != null)
                            property.signature?.let { buffer.writeProtocolString(it) }
                        }
                    }
                    Action.UPDATE_GAME_MODE -> buffer.writeVarInt(entry.gamemode.ordinal)
                    Action.UPDATE_LISTED -> buffer.writeBoolean(entry.listed)
                    Action.UPDATE_LATENCY -> buffer.writeVarInt(entry.latencyMillis)
                    else -> error("player_info action $action is not implemented")
                }
            }
        }
    }

    public companion object {
        public const val ID: Int = 0x46

        /**
         * What vanilla's `createPlayerInitializing` sends for a player who just joined, minus the
         * actions above that are not implemented. The game mode belongs in here: a player who joins
         * in creative is a spectator-or-not to everyone else from this packet alone.
         */
        public val INITIALIZING: Set<Action> =
            setOf(Action.ADD_PLAYER, Action.UPDATE_GAME_MODE, Action.UPDATE_LISTED, Action.UPDATE_LATENCY)

        public fun initializing(entries: List<PlayerInfoEntry>): PlayerInfoUpdatePacket =
            PlayerInfoUpdatePacket(INITIALIZING, entries)

        /** The single-action update vanilla broadcasts to everyone when a player's game mode changes. */
        public fun gameMode(
            uuid: Uuid,
            gamemode: Gamemode,
        ): PlayerInfoUpdatePacket =
            PlayerInfoUpdatePacket(
                setOf(Action.UPDATE_GAME_MODE),
                listOf(PlayerInfoEntry(uuid, gamemode = gamemode)),
            )

        public fun read(buffer: ByteBuffer): PlayerInfoUpdatePacket {
            val bits = buffer.readByte().toInt() and 0xFF
            val actions = Action.entries.filterTo(mutableSetOf()) { bits and (1 shl it.ordinal) != 0 }
            val entries =
                List(buffer.readVarInt()) {
                    var entry = PlayerInfoEntry(buffer.readUuid())
                    for (action in Action.entries) {
                        if (action !in actions) continue
                        entry =
                            when (action) {
                                Action.ADD_PLAYER -> {
                                    val name = buffer.readProtocolString(16)
                                    val properties =
                                        List(buffer.readVarInt()) {
                                            GameProfileProperty(
                                                name = buffer.readProtocolString(),
                                                value = buffer.readProtocolString(),
                                                signature =
                                                    if (buffer.readBoolean()) buffer.readProtocolString() else null,
                                            )
                                        }
                                    entry.copy(name = name, properties = properties)
                                }
                                Action.UPDATE_GAME_MODE -> entry.copy(gamemode = Gamemode.entries[buffer.readVarInt()])
                                Action.UPDATE_LISTED -> entry.copy(listed = buffer.readBoolean())
                                Action.UPDATE_LATENCY -> entry.copy(latencyMillis = buffer.readVarInt())
                                else -> error("player_info action $action is not implemented")
                            }
                    }
                    entry
                }
            return PlayerInfoUpdatePacket(actions, entries)
        }
    }
}

/** Clientbound, id 0x45: removes entries from the tab list. Mirrors `ClientboundPlayerInfoRemovePacket`. */
@OptIn(ExperimentalUuidApi::class)
public data class PlayerInfoRemovePacket(
    val players: List<Uuid>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(players.size)
        for (uuid in players) buffer.writeUuid(uuid)
    }

    public companion object {
        public const val ID: Int = 0x45

        public fun read(buffer: ByteBuffer): PlayerInfoRemovePacket {
            val count = buffer.readVarInt()
            return PlayerInfoRemovePacket(List(count) { buffer.readUuid() })
        }
    }
}

/** Clientbound, id 0x7d: an absolute position/rotation update for an entity other than the receiving player. Mirrors `ClientboundTeleportEntityPacket`. */
public data class EntityTeleportPacket(
    val entityId: Int,
    val x: Double,
    val y: Double,
    val z: Double,
    val yaw: Float,
    val pitch: Float,
    val onGround: Boolean,
    val deltaX: Double = 0.0,
    val deltaY: Double = 0.0,
    val deltaZ: Double = 0.0,
    /**
     * Vanilla's `Relative` bitmask: which fields the client should add to what it already has
     * instead of replacing. Zero means every value is absolute, which is what a server telling a
     * client where somebody *is* always means.
     */
    val relatives: Int = 0,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        // parity: `PositionMoveRotation` is position, then velocity, then two floats of rotation.
        // The velocity is not optional and its absence is what broke this: a client read the three
        // doubles it was owed, ran off the end of the packet and dropped the connection with
        // "Failed to decode packet 'clientbound/minecraft:teleport_entity'".
        buffer.writeDouble(x)
        buffer.writeDouble(y)
        buffer.writeDouble(z)
        buffer.writeDouble(deltaX)
        buffer.writeDouble(deltaY)
        buffer.writeDouble(deltaZ)
        // Degrees as floats, not the packed angle bytes older protocols used here.
        buffer.writeFloat(yaw)
        buffer.writeFloat(pitch)
        buffer.writeInt(relatives)
        buffer.writeBoolean(onGround)
    }

    public companion object {
        public const val ID: Int = 0x7d

        public fun read(buffer: ByteBuffer): EntityTeleportPacket {
            val entityId = buffer.readVarInt()
            val x = buffer.readDouble()
            val y = buffer.readDouble()
            val z = buffer.readDouble()
            val deltaX = buffer.readDouble()
            val deltaY = buffer.readDouble()
            val deltaZ = buffer.readDouble()
            val yaw = buffer.readFloat()
            val pitch = buffer.readFloat()
            val relatives = buffer.readInt()
            return EntityTeleportPacket(
                entityId = entityId,
                x = x,
                y = y,
                z = z,
                yaw = yaw,
                pitch = pitch,
                onGround = buffer.readBoolean(),
                deltaX = deltaX,
                deltaY = deltaY,
                deltaZ = deltaZ,
                relatives = relatives,
            )
        }
    }
}

/** Clientbound, id 0x2c / serverbound, id 0x1c. Mirrors `Client|ServerboundKeepAlivePacket` in Play. */
public data class PlayKeepAlivePacket(
    val keepAliveId: Long,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(keepAliveId)
    }

    public companion object {
        public const val CLIENTBOUND_ID: Int = 0x2c
        public const val SERVERBOUND_ID: Int = 0x1c

        public fun read(buffer: ByteBuffer): PlayKeepAlivePacket = PlayKeepAlivePacket(buffer.readLong())
    }
}

/**
 * Clientbound, id 0x79: an unsigned chat message. Mirrors `ClientboundSystemChatPacket`.
 *
 * Used both for real system messages and, for now, to relay player chat: real signed
 * `player_chat` (chat session registration, message signing) is a documented gap, so relayed
 * messages currently render with system styling rather than a player name/head in the client's
 * chat UI. `basalt.toml`'s `online-mode` does not require signed chat, matching vanilla with
 * `enforce-secure-chat=false`.
 */
public data class SystemChatPacket(
    val content: Component,
    val isActionBar: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeAnonymousNbt(ComponentNbt.serialize(content))
        buffer.writeBoolean(isActionBar)
    }

    public companion object {
        public const val ID: Int = 0x79

        public fun read(buffer: ByteBuffer): SystemChatPacket {
            val content = ComponentNbt.deserialize(buffer.readAnonymousNbt())
            val isActionBar = buffer.readBoolean()
            return SystemChatPacket(content, isActionBar)
        }
    }
}

/**
 * Serverbound, id 0x09: a chat message. Mirrors `ServerboundChatPacket`. The signing-related
 * fields ([signature], [offset], [acknowledged], [checksum]) are read/written to keep the wire
 * format correct but are not validated: see [SystemChatPacket]'s KDoc for the signed-chat gap.
 */
public data class ChatMessagePacket(
    val message: String,
    val timestamp: Long,
    val salt: Long,
    val signature: ByteArray?,
    val offset: Int,
    val acknowledged: ByteArray,
    val checksum: Byte,
) {
    public companion object {
        public const val ID: Int = 0x09
        private const val SIGNATURE_LENGTH = 256
        private const val ACKNOWLEDGED_LENGTH = 3

        public fun read(buffer: ByteBuffer): ChatMessagePacket {
            val message = buffer.readProtocolString(256)
            val timestamp = buffer.readLong()
            val salt = buffer.readLong()
            val signature = if (buffer.readBoolean()) buffer.readBytes(SIGNATURE_LENGTH) else null
            val offset = buffer.readVarInt()
            val acknowledged = buffer.readBytes(ACKNOWLEDGED_LENGTH)
            val checksum = buffer.readByte()
            return ChatMessagePacket(message, timestamp, salt, signature, offset, acknowledged, checksum)
        }
    }

    override fun equals(other: Any?): Boolean =
        other is ChatMessagePacket &&
            message == other.message &&
            timestamp == other.timestamp &&
            salt == other.salt &&
            signature.contentEquals(other.signature) &&
            offset == other.offset &&
            acknowledged.contentEquals(other.acknowledged) &&
            checksum == other.checksum

    override fun hashCode(): Int = message.hashCode()
}

/** Clientbound, id 0x20: kicks the player with a reason. Mirrors `ClientboundDisconnectPacket`. */
public data class PlayDisconnectPacket(
    val reason: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeAnonymousNbt(ComponentNbt.serialize(reason))
    }

    public companion object {
        public const val ID: Int = 0x20

        public fun read(buffer: ByteBuffer): PlayDisconnectPacket =
            PlayDisconnectPacket(ComponentNbt.deserialize(buffer.readAnonymousNbt()))
    }
}

/** Serverbound, id 0x1e: a movement-only position update. Mirrors `ServerboundMovePlayerPacketPosOnly`. */
public data class ServerboundPositionPacket(
    val x: Double,
    val y: Double,
    val z: Double,
    val onGround: Boolean,
) {
    public companion object {
        public const val ID: Int = 0x1e

        public fun read(buffer: ByteBuffer): ServerboundPositionPacket {
            val x = buffer.readDouble()
            val y = buffer.readDouble()
            val z = buffer.readDouble()
            val flags = buffer.readByte().toInt()
            return ServerboundPositionPacket(x, y, z, onGround = (flags and 1) != 0)
        }
    }
}

/** Serverbound, id 0x1f: a position and rotation update. Mirrors `ServerboundMovePlayerPacketPosRot`. */
public data class ServerboundPositionLookPacket(
    val x: Double,
    val y: Double,
    val z: Double,
    val yaw: Float,
    val pitch: Float,
    val onGround: Boolean,
) {
    public companion object {
        public const val ID: Int = 0x1f

        public fun read(buffer: ByteBuffer): ServerboundPositionLookPacket {
            val x = buffer.readDouble()
            val y = buffer.readDouble()
            val z = buffer.readDouble()
            val yaw = buffer.readFloat()
            val pitch = buffer.readFloat()
            val flags = buffer.readByte().toInt()
            return ServerboundPositionLookPacket(x, y, z, yaw, pitch, onGround = (flags and 1) != 0)
        }
    }
}

/** Clientbound, id 0x08: a single block changed. Mirrors `ClientboundBlockUpdatePacket`. */
public data class ClientboundBlockUpdatePacket(
    val pos: BlockPos,
    val blockStateId: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(pos.pack())
        buffer.writeVarInt(blockStateId)
    }

    public companion object {
        public const val ID: Int = 0x08

        public fun read(buffer: ByteBuffer): ClientboundBlockUpdatePacket =
            ClientboundBlockUpdatePacket(BlockPos.unpack(buffer.readLong()), buffer.readVarInt())
    }
}

/**
 * Clientbound, id 0x04: acknowledges a client-predicted change (dig/place) up to [sequence], so the
 * client can stop predicting past that point. Mirrors `ClientboundBlockChangedAckPacket`.
 */
public data class ClientboundBlockChangedAckPacket(
    val sequence: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(sequence)
    }

    public companion object {
        public const val ID: Int = 0x04

        public fun read(buffer: ByteBuffer): ClientboundBlockChangedAckPacket =
            ClientboundBlockChangedAckPacket(buffer.readVarInt())
    }
}

/**
 * Serverbound, id 0x29: digging/breaking a block (start/abort/stop), or a non-block player action
 * (drop item, swap hands...). Mirrors `ServerboundPlayerActionPacket`.
 */
public data class ServerboundPlayerActionPacket(
    val action: Action,
    val pos: BlockPos,
    val direction: Direction,
    val sequence: Int,
) {
    public enum class Action {
        START_DESTROY_BLOCK,
        ABORT_DESTROY_BLOCK,
        STOP_DESTROY_BLOCK,
        DROP_ALL_ITEMS,
        DROP_ITEM,
        RELEASE_USE_ITEM,
        SWAP_ITEM_WITH_OFFHAND,
        STAB,
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(action.ordinal)
        buffer.writeLong(pos.pack())
        buffer.writeByte(direction.ordinal.toByte())
        buffer.writeVarInt(sequence)
    }

    public companion object {
        public const val ID: Int = 0x29

        public fun read(buffer: ByteBuffer): ServerboundPlayerActionPacket {
            val action = Action.entries[buffer.readVarInt()]
            val pos = BlockPos.unpack(buffer.readLong())
            val direction = Direction.entries[buffer.readByte().toInt() and 0xFF]
            val sequence = buffer.readVarInt()
            return ServerboundPlayerActionPacket(action, pos, direction, sequence)
        }
    }
}

/** Where on a block's face a `use_item_on` interaction landed. Mirrors `BlockHitResult`'s wire shape (`FriendlyByteBuf.readBlockHitResult`). */
public data class BlockHitResult(
    val pos: BlockPos,
    val direction: Direction,
    val clickX: Float,
    val clickY: Float,
    val clickZ: Float,
    val inside: Boolean,
    val worldBorderHit: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(pos.pack())
        buffer.writeVarInt(direction.ordinal)
        buffer.writeFloat(clickX)
        buffer.writeFloat(clickY)
        buffer.writeFloat(clickZ)
        buffer.writeBoolean(inside)
        buffer.writeBoolean(worldBorderHit)
    }

    public companion object {
        public fun read(buffer: ByteBuffer): BlockHitResult {
            val pos = BlockPos.unpack(buffer.readLong())
            val direction = Direction.entries[buffer.readVarInt()]
            val clickX = buffer.readFloat()
            val clickY = buffer.readFloat()
            val clickZ = buffer.readFloat()
            val inside = buffer.readBoolean()
            val worldBorderHit = buffer.readBoolean()
            return BlockHitResult(pos, direction, clickX, clickY, clickZ, inside, worldBorderHit)
        }
    }
}

/** Serverbound, id 0x42: using the held item on a block (placing, opening a container...). Mirrors `ServerboundUseItemOnPacket`. */
public data class ServerboundUseItemOnPacket(
    val hand: Hand,
    val blockHit: BlockHitResult,
    val sequence: Int,
) {
    public enum class Hand {
        MAIN_HAND,
        OFF_HAND,
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(hand.ordinal)
        blockHit.write(buffer)
        buffer.writeVarInt(sequence)
    }

    public companion object {
        public const val ID: Int = 0x42

        public fun read(buffer: ByteBuffer): ServerboundUseItemOnPacket {
            val hand = Hand.entries[buffer.readVarInt()]
            val blockHit = BlockHitResult.read(buffer)
            val sequence = buffer.readVarInt()
            return ServerboundUseItemOnPacket(hand, blockHit, sequence)
        }
    }
}

/**
 * Serverbound, id 0x24: middle-click "pick block" on a targeted block. Mirrors
 * `ServerboundPickItemFromBlockPacket`. [includeData] asks for the block's block-entity data
 * (a sign's text, a container's contents) copied onto the picked stack; this build ignores it,
 * since neither block entities nor item data components exist here yet.
 */
public data class ServerboundPickItemFromBlockPacket(
    val pos: BlockPos,
    val includeData: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(pos.pack())
        buffer.writeBoolean(includeData)
    }

    public companion object {
        public const val ID: Int = 0x24

        public fun read(buffer: ByteBuffer): ServerboundPickItemFromBlockPacket =
            ServerboundPickItemFromBlockPacket(BlockPos.unpack(buffer.readLong()), buffer.readBoolean())
    }
}
