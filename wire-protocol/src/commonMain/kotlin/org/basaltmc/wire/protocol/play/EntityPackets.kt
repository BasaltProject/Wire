package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.ProtocolException
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readUuid
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeUuid
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.round
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Vanilla's "low precision" `Vec3` wire encoding, used for entity velocity. Mirrors `LpVec3`: the
 * zero vector is a single zero byte, anything else shares one common power-of-four scale across
 * all three axes, quantized to 15 bits each. Verified against the real 26.2 server jar's decompiled
 * `LpVec3`, not the flat `* 8000` short encoding older protocol versions used here.
 */
public object LpVec3 {
    private const val DATA_BITS_MASK = 32767L
    private const val MAX_QUANTIZED_VALUE = 32766.0
    private const val CONTINUATION_FLAG = 4L
    private const val ABS_MAX_VALUE = 1.7179869183E10
    private const val ABS_MIN_VALUE = 3.051944088384301E-5

    public fun write(
        buffer: ByteBuffer,
        x: Double,
        y: Double,
        z: Double,
    ) {
        val sx = sanitize(x)
        val sy = sanitize(y)
        val sz = sanitize(z)
        val chessboard = maxOf(abs(sx), abs(sy), abs(sz))
        if (chessboard < ABS_MIN_VALUE) {
            buffer.writeByte(0)
            return
        }
        val scale = ceil(chessboard).toLong()
        val isPartial = (scale and 3L) != scale
        val markers = if (isPartial) (scale and 3L) or CONTINUATION_FLAG else scale
        val packed =
            markers or
                (pack(sx / scale) shl 3) or
                (pack(sy / scale) shl 18) or
                (pack(sz / scale) shl 33)
        buffer.writeByte(packed.toByte())
        buffer.writeByte((packed shr 8).toByte())
        buffer.writeInt((packed shr 16).toInt())
        if (isPartial) buffer.writeVarInt((scale shr 2).toInt())
    }

    public fun read(buffer: ByteBuffer): Triple<Double, Double, Double> {
        val lowest = buffer.readByte().toInt() and 0xFF
        if (lowest == 0) return Triple(0.0, 0.0, 0.0)
        val middle = buffer.readByte().toInt() and 0xFF
        val highest = buffer.readInt().toLong() and 0xFFFFFFFFL
        val packed = (highest shl 16) or (middle.toLong() shl 8) or lowest.toLong()
        var scale = lowest.toLong() and 3L
        if ((lowest.toLong() and CONTINUATION_FLAG) == CONTINUATION_FLAG) {
            scale = scale or ((buffer.readVarInt().toLong() and 0xFFFFFFFFL) shl 2)
        }
        return Triple(
            unpack(packed shr 3) * scale,
            unpack(packed shr 18) * scale,
            unpack(packed shr 33) * scale,
        )
    }

    private fun sanitize(value: Double): Double =
        if (value.isNaN()) 0.0 else value.coerceIn(-ABS_MAX_VALUE, ABS_MAX_VALUE)

    private fun pack(value: Double): Long = round((value * 0.5 + 0.5) * MAX_QUANTIZED_VALUE).toLong()

    private fun unpack(value: Long): Double =
        minOf(value and DATA_BITS_MASK, MAX_QUANTIZED_VALUE.toLong()).toDouble() * 2.0 / MAX_QUANTIZED_VALUE - 1.0
}

/** `Mth.packDegrees`/`unpackDegrees`: a rotation as a single byte, 256 steps per full turn. */
private fun packDegrees(value: Float): Byte = floor(value * 256f / 360f).toInt().toByte()

private fun unpackDegrees(value: Byte): Float = value * 360f / 256f

/**
 * Clientbound, id 0x01: spawns an entity.
 * Mirrors `ClientboundAddEntityPacket`. [entityType] is the numeric protocol id of the entity's
 * type (verified against the real 26.2 registries; `minecraft:item` is 71). [objectData] carries
 * type-specific extra data such as an item-frame direction; it is zero for types that do not need
 * it.
 */
@OptIn(ExperimentalUuidApi::class)
public data class SpawnEntityPacket(
    val entityId: Int,
    val uuid: Uuid,
    val entityType: Int,
    val x: Double,
    val y: Double,
    val z: Double,
    val velocityX: Double = 0.0,
    val velocityY: Double = 0.0,
    val velocityZ: Double = 0.0,
    val pitch: Float = 0f,
    val yaw: Float = 0f,
    val headYaw: Float = 0f,
    val objectData: Int = 0,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeUuid(uuid)
        buffer.writeVarInt(entityType)
        buffer.writeDouble(x)
        buffer.writeDouble(y)
        buffer.writeDouble(z)
        LpVec3.write(buffer, velocityX, velocityY, velocityZ)
        buffer.writeByte(packDegrees(pitch))
        buffer.writeByte(packDegrees(yaw))
        buffer.writeByte(packDegrees(headYaw))
        buffer.writeVarInt(objectData)
    }

    public companion object {
        public const val ID: Int = 0x01

        /** Protocol id of `minecraft:item` in the real 26.2 entity type registry. */
        public const val ITEM_ENTITY_TYPE: Int = 71

        /**
         * Protocol id of `minecraft:player` in the real 26.2 registry. Read off the jar's own
         * `registries.json`, not a published table: `minecraft-data` still lists 155 for 26.1 and
         * the entry moved, so a copied constant would spawn the wrong entity entirely.
         */
        public const val PLAYER_ENTITY_TYPE: Int = 156

        public fun read(buffer: ByteBuffer): SpawnEntityPacket {
            val entityId = buffer.readVarInt()
            val uuid = buffer.readUuid()
            val entityType = buffer.readVarInt()
            val x = buffer.readDouble()
            val y = buffer.readDouble()
            val z = buffer.readDouble()
            val (vx, vy, vz) = LpVec3.read(buffer)
            val pitch = unpackDegrees(buffer.readByte())
            val yaw = unpackDegrees(buffer.readByte())
            val headYaw = unpackDegrees(buffer.readByte())
            val objectData = buffer.readVarInt()
            return SpawnEntityPacket(entityId, uuid, entityType, x, y, z, vx, vy, vz, pitch, yaw, headYaw, objectData)
        }
    }
}

/**
 * Clientbound, id 0x63: entity metadata. Mirrors `ClientboundSetEntityDataPacket`: a stream of
 * entries, each an index byte, a serializer id and the value, terminated by [END_OF_METADATA].
 *
 * Indices are positions in the entity's own `defineId` order, counted down the real class
 * hierarchy, which is why shared indices are named rather than written as literals at call sites:
 * in 26.2 a new `Avatar` class sits between `LivingEntity` and `Player`, so the player indices are
 * not where an older version would put them.
 */
public data class SetEntityDataPacket(
    val entityId: Int,
    val entries: List<Entry>,
) {
    public sealed interface Entry {
        public val index: Int

        /** A single byte, `EntityDataSerializers.BYTE` (serializer 0). */
        public data class Bytes(
            override val index: Int,
            val value: Byte,
        ) : Entry

        /** A VarInt, `EntityDataSerializers.INT` (serializer 1). */
        public data class Ints(
            override val index: Int,
            val value: Int,
        ) : Entry

        /** A float, `EntityDataSerializers.FLOAT` (serializer 3). */
        public data class Floats(
            override val index: Int,
            val value: Float,
        ) : Entry

        /** An item stack, `EntityDataSerializers.ITEM_STACK` (serializer 7). */
        public data class Item(
            override val index: Int,
            val stack: WireItemStack?,
        ) : Entry

        /** A boolean, `EntityDataSerializers.BOOLEAN` (serializer 8). */
        public data class Booleans(
            override val index: Int,
            val value: Boolean,
        ) : Entry

        /** Three Euler-angle floats, `EntityDataSerializers.ROTATIONS` (serializer 9). */
        public data class Rotations(
            override val index: Int,
            val x: Float,
            val y: Float,
            val z: Float,
        ) : Entry

        /** Built-in villager type/profession ids and level, serializer 18. */
        public data class VillagerData(
            override val index: Int,
            val typeId: Int,
            val professionId: Int,
            val level: Int,
        ) : Entry

        /** An optional block position, `EntityDataSerializers.OPTIONAL_BLOCK_POS` (serializer 11). */
        public data class OptionalBlockPos(
            override val index: Int,
            val pos: BlockPos?,
        ) : Entry

        /** A pose, `EntityDataSerializers.POSE` (serializer 20), written as its own id. */
        public data class Poses(
            override val index: Int,
            val pose: Pose,
        ) : Entry

        /** A holder in a synchronized dynamic registry, encoded as its VarInt id. */
        public data class RegistryId(
            override val index: Int,
            val value: Int,
        ) : Entry
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        for (entry in entries) {
            buffer.writeByte(entry.index.toByte())
            when (entry) {
                is Entry.Bytes -> {
                    buffer.writeVarInt(BYTE_SERIALIZER)
                    buffer.writeByte(entry.value)
                }
                is Entry.Ints -> {
                    buffer.writeVarInt(INT_SERIALIZER)
                    buffer.writeVarInt(entry.value)
                }
                is Entry.Floats -> {
                    buffer.writeVarInt(FLOAT_SERIALIZER)
                    buffer.writeFloat(entry.value)
                }
                is Entry.Item -> {
                    buffer.writeVarInt(ITEM_STACK_SERIALIZER)
                    WireItemStack.write(buffer, entry.stack)
                }
                is Entry.Booleans -> {
                    buffer.writeVarInt(BOOLEAN_SERIALIZER)
                    buffer.writeBoolean(entry.value)
                }
                is Entry.Rotations -> {
                    buffer.writeVarInt(ROTATIONS_SERIALIZER)
                    buffer.writeFloat(entry.x)
                    buffer.writeFloat(entry.y)
                    buffer.writeFloat(entry.z)
                }
                is Entry.VillagerData -> {
                    buffer.writeVarInt(VILLAGER_DATA_SERIALIZER)
                    buffer.writeVarInt(entry.typeId)
                    buffer.writeVarInt(entry.professionId)
                    buffer.writeVarInt(entry.level)
                }
                is Entry.OptionalBlockPos -> {
                    buffer.writeVarInt(OPTIONAL_BLOCK_POS_SERIALIZER)
                    buffer.writeBoolean(entry.pos != null)
                    entry.pos?.let { buffer.writeLong(it.pack()) }
                }
                is Entry.Poses -> {
                    buffer.writeVarInt(POSE_SERIALIZER)
                    buffer.writeVarInt(entry.pose.ordinal)
                }
                is Entry.RegistryId -> {
                    buffer.writeVarInt(CAT_VARIANT_SERIALIZER)
                    buffer.writeVarInt(entry.value)
                }
            }
        }
        buffer.writeByte(END_OF_METADATA.toByte())
    }

    public companion object {
        public const val ID: Int = 0x63

        /** `ItemEntity.DATA_ITEM`: `Entity` claims 0-7, so the item's own entry lands here. */
        public const val ITEM_STACK_INDEX: Int = 8

        /** `ItemFrame.DATA_ITEM`: Entity 0-7, HangingEntity direction 8, then the frame item. */
        public const val ITEM_FRAME_STACK_INDEX: Int = 9

        /**
         * `Avatar.DATA_PLAYER_MODE_CUSTOMISATION`, the bitmask of which skin layers to draw.
         * Entity 0-7, LivingEntity 8-14, Avatar 15-16.
         */
        public const val PLAYER_SKIN_PARTS_INDEX: Int = 16

        /**
         * parity: `Player.DATA_PLAYER_ABSORPTION_ID`, the yellow hearts.
         *
         * 26.2 put `Avatar` between `LivingEntity` and `Player` (main hand and skin parts, 15 and
         * 16), so this moved: it is not the 15 older versions used.
         */
        public const val PLAYER_ABSORPTION_INDEX: Int = 17

        /** parity: `Entity.DATA_POSE`, which is what draws a player lying in a bed. */
        public const val POSE_INDEX: Int = 6

        /** parity: `LivingEntity.SLEEPING_POS_ID`, the bed they are lying in. */
        public const val SLEEPING_POS_INDEX: Int = 14

        /** parity: `Entity.DATA_SHARED_FLAGS_ID`, whose lowest bit is "this entity is on fire". */
        public const val SHARED_FLAGS_INDEX: Int = 0

        /** parity: `Entity.DATA_AIR_SUPPLY_ID`, the ticks of air left, which the client draws as bubbles. */
        public const val AIR_SUPPLY_INDEX: Int = 1

        public const val FLAG_ON_FIRE: Int = 0x01

        private const val BYTE_SERIALIZER = 0
        private const val INT_SERIALIZER = 1
        private const val FLOAT_SERIALIZER = 3
        private const val ITEM_STACK_SERIALIZER = 7
        private const val BOOLEAN_SERIALIZER = 8
        private const val OPTIONAL_BLOCK_POS_SERIALIZER = 11
        private const val POSE_SERIALIZER = 20
        private const val ROTATIONS_SERIALIZER = 9
        private const val VILLAGER_DATA_SERIALIZER = 18
        private const val CAT_VARIANT_SERIALIZER = 21
        private const val END_OF_METADATA = 0xFF

        /** The one entry a dropped item needs. */
        public fun item(
            entityId: Int,
            stack: WireItemStack?,
        ): SetEntityDataPacket = SetEntityDataPacket(entityId, listOf(Entry.Item(ITEM_STACK_INDEX, stack)))

        /** The Elytra or other item displayed by an item frame. */
        public fun itemFrame(
            entityId: Int,
            stack: WireItemStack?,
        ): SetEntityDataPacket = SetEntityDataPacket(entityId, listOf(Entry.Item(ITEM_FRAME_STACK_INDEX, stack)))

        /** The air left and whether this entity is burning, which is what the HUD draws. */
        public fun submersion(
            entityId: Int,
            airSupply: Int,
            onFire: Boolean,
        ): SetEntityDataPacket =
            SetEntityDataPacket(
                entityId,
                listOf(
                    Entry.Bytes(SHARED_FLAGS_INDEX, (if (onFire) FLAG_ON_FIRE else 0).toByte()),
                    Entry.Ints(AIR_SUPPLY_INDEX, airSupply),
                ),
            )

        /** Which skin layers a player has switched on, straight from their own client settings. */
        public fun skinParts(
            entityId: Int,
            skinParts: Byte,
        ): SetEntityDataPacket = SetEntityDataPacket(entityId, listOf(Entry.Bytes(PLAYER_SKIN_PARTS_INDEX, skinParts)))

        /** Lying in the bed at [bed], or standing again when it is null. */
        public fun sleeping(
            entityId: Int,
            bed: BlockPos?,
        ): SetEntityDataPacket =
            SetEntityDataPacket(
                entityId,
                listOf(
                    Entry.Poses(POSE_INDEX, if (bed == null) Pose.STANDING else Pose.SLEEPING),
                    Entry.OptionalBlockPos(SLEEPING_POS_INDEX, bed),
                ),
            )

        /** The yellow hearts on top of a player's own health bar. */
        public fun playerAbsorption(
            entityId: Int,
            amount: Float,
        ): SetEntityDataPacket = SetEntityDataPacket(entityId, listOf(Entry.Floats(PLAYER_ABSORPTION_INDEX, amount)))

        public fun read(buffer: ByteBuffer): SetEntityDataPacket {
            val entityId = buffer.readVarInt()
            val entries = mutableListOf<Entry>()
            while (true) {
                val index = buffer.readByte().toInt() and 0xFF
                if (index == END_OF_METADATA) break
                entries +=
                    when (val serializer = buffer.readVarInt()) {
                        BYTE_SERIALIZER -> Entry.Bytes(index, buffer.readByte())
                        INT_SERIALIZER -> Entry.Ints(index, buffer.readVarInt())
                        FLOAT_SERIALIZER -> Entry.Floats(index, buffer.readFloat())
                        ITEM_STACK_SERIALIZER -> Entry.Item(index, WireItemStack.read(buffer))
                        BOOLEAN_SERIALIZER -> Entry.Booleans(index, buffer.readBoolean())
                        OPTIONAL_BLOCK_POS_SERIALIZER ->
                            Entry.OptionalBlockPos(
                                index,
                                if (buffer.readBoolean()) BlockPos.unpack(buffer.readLong()) else null,
                            )
                        POSE_SERIALIZER -> Entry.Poses(index, Pose.entries[buffer.readVarInt()])
                        ROTATIONS_SERIALIZER ->
                            Entry.Rotations(index, buffer.readFloat(), buffer.readFloat(), buffer.readFloat())
                        VILLAGER_DATA_SERIALIZER ->
                            Entry.VillagerData(
                                index,
                                buffer.readVarInt(),
                                buffer.readVarInt(),
                                buffer.readVarInt(),
                            )
                        CAT_VARIANT_SERIALIZER -> Entry.RegistryId(index, buffer.readVarInt())
                        else -> throw ProtocolException(
                            "Unsupported entity metadata serializer $serializer at index $index",
                        )
                    }
            }
            return SetEntityDataPacket(entityId, entries)
        }
    }
}

/**
 * Clientbound, id 0x66: equipment changes. The high bit on every slot except the last one is the
 * continuation marker used by `ClientboundSetEquipmentPacket`.
 */
public data class SetEquipmentPacket(
    val entityId: Int,
    val slots: List<Slot>,
) {
    public data class Slot(
        val type: Type,
        val stack: WireItemStack?,
    )

    public enum class Type {
        MAINHAND,
        OFFHAND,
        FEET,
        LEGS,
        CHEST,
        HEAD,
        BODY,
        SADDLE,
    }

    init {
        require(slots.isNotEmpty()) { "An equipment packet must contain at least one slot" }
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        for ((index, slot) in slots.withIndex()) {
            val continuation = if (index < slots.lastIndex) CONTINUE_MASK else 0
            buffer.writeByte((slot.type.ordinal or continuation).toByte())
            WireItemStack.write(buffer, slot.stack)
        }
    }

    public companion object {
        public const val ID: Int = 0x66

        private const val CONTINUE_MASK = 0x80

        public fun read(buffer: ByteBuffer): SetEquipmentPacket {
            val entityId = buffer.readVarInt()
            val slots = mutableListOf<Slot>()
            do {
                val encoded = buffer.readByte().toInt() and 0xFF
                val type =
                    Type.entries.getOrNull(encoded and 0x7F)
                        ?: throw ProtocolException("Unknown equipment slot ${encoded and 0x7F}")
                slots += Slot(type, WireItemStack.read(buffer))
            } while ((encoded and CONTINUE_MASK) != 0)
            return SetEquipmentPacket(entityId, slots)
        }
    }
}

/**
 * Clientbound, id 0x7c: plays the "item flies to the player" pickup animation. Mirrors
 * `ClientboundTakeItemEntityPacket`. Purely cosmetic: the inventory change itself travels through
 * the normal container packets, sent alongside this one.
 */
public data class ClientboundTakeItemEntityPacket(
    val collectedEntityId: Int,
    val collectorEntityId: Int,
    val pickupItemCount: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(collectedEntityId)
        buffer.writeVarInt(collectorEntityId)
        buffer.writeVarInt(pickupItemCount)
    }

    public companion object {
        public const val ID: Int = 0x7c

        public fun read(buffer: ByteBuffer): ClientboundTakeItemEntityPacket =
            ClientboundTakeItemEntityPacket(buffer.readVarInt(), buffer.readVarInt(), buffer.readVarInt())
    }
}

/**
 * Clientbound, id 0x02: a one-off animation on an entity. Mirrors `ClientboundAnimatePacket`.
 *
 * The action is a byte with no varint, and the values are vanilla's own: swinging the off hand is
 * 3, not 1, because 1 was the old "hurt" animation and the numbering was never compacted.
 */
public data class AnimatePacket(
    val entityId: Int,
    val action: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeByte(action.toByte())
    }

    public companion object {
        public const val ID: Int = 0x02
        public const val SWING_MAIN_HAND: Int = 0
        public const val WAKE_UP: Int = 2
        public const val SWING_OFFHAND: Int = 3
        public const val CRITICAL_HIT: Int = 4
        public const val MAGIC_CRITICAL_HIT: Int = 5

        public fun read(buffer: ByteBuffer): AnimatePacket =
            AnimatePacket(buffer.readVarInt(), buffer.readByte().toInt() and 0xFF)
    }
}

/**
 * Clientbound, id 0x64: who is leashed to (or riding) what. Mirrors `ClientboundSetEntityLinkPacket`.
 *
 * Both ids are plain ints rather than varints, which is unusual enough to be worth stating: a
 * varint here writes a shorter frame and desynchronizes the client on the next packet.
 */
public data class SetEntityLinkPacket(
    val sourceId: Int,
    val destinationId: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeInt(sourceId)
        buffer.writeInt(destinationId)
    }

    public companion object {
        public const val ID: Int = 0x64

        /** What vanilla writes to say "nothing": -1, which is how a dismount is expressed. */
        public const val NO_VEHICLE: Int = -1

        public fun read(buffer: ByteBuffer): SetEntityLinkPacket =
            SetEntityLinkPacket(buffer.readInt(), buffer.readInt())
    }
}

/**
 * Clientbound, id 0x2f: particles at a position. Mirrors `ClientboundLevelParticlesPacket`.
 *
 * The particle *type* is written last, after the counts, and carries its own options for the types
 * that have them. Only option-free types are written here; the ones with options are refused
 * before they reach this packet, because a missing option block desynchronizes the stream.
 */
public data class LevelParticlesPacket(
    val particleTypeId: Int,
    val x: Double,
    val y: Double,
    val z: Double,
    val offsetX: Float = 0f,
    val offsetY: Float = 0f,
    val offsetZ: Float = 0f,
    val maxSpeed: Float = 0f,
    val count: Int = 1,
    /** parity: `force`, which shows the particle past the client's own particle-count setting. */
    val overrideLimiter: Boolean = false,
    val alwaysShow: Boolean = false,
    /** The type's own extra data, written straight after its id. Null for the types that carry none. */
    val options: ParticleOptions? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeBoolean(overrideLimiter)
        buffer.writeBoolean(alwaysShow)
        buffer.writeDouble(x)
        buffer.writeDouble(y)
        buffer.writeDouble(z)
        buffer.writeFloat(offsetX)
        buffer.writeFloat(offsetY)
        buffer.writeFloat(offsetZ)
        buffer.writeFloat(maxSpeed)
        buffer.writeInt(count)
        buffer.writeVarInt(particleTypeId)
        options?.write(buffer)
    }

    public companion object {
        public const val ID: Int = 0x2f

        public fun read(buffer: ByteBuffer): LevelParticlesPacket {
            val overrideLimiter = buffer.readBoolean()
            val alwaysShow = buffer.readBoolean()
            val x = buffer.readDouble()
            val y = buffer.readDouble()
            val z = buffer.readDouble()
            val offsetX = buffer.readFloat()
            val offsetY = buffer.readFloat()
            val offsetZ = buffer.readFloat()
            val maxSpeed = buffer.readFloat()
            val count = buffer.readInt()
            return LevelParticlesPacket(
                particleTypeId = buffer.readVarInt(),
                x = x,
                y = y,
                z = z,
                offsetX = offsetX,
                offsetY = offsetY,
                offsetZ = offsetZ,
                maxSpeed = maxSpeed,
                count = count,
                overrideLimiter = overrideLimiter,
                alwaysShow = alwaysShow,
            )
        }
    }
}

/**
 * Clientbound, id 0x8a: one entry of the locator bar. Mirrors `ClientboundTrackedWaypointPacket`.
 *
 * The identifier is an `Either<UUID, String>`, written as a boolean and then the chosen side, and
 * the payload depends on the waypoint's own kind: a position is three varints, which is the kind a
 * command can point at. The icon is a style id plus an optional packed RGB colour.
 */
@OptIn(ExperimentalUuidApi::class)
/**
 * parity: `ClientboundEntityEventPacket`.
 *
 * Most events are animations, but 24 to 28 are the player's own permission level, and the client
 * needs them: its local command parser refuses selectors (`@a`, `@s`) until it has been told the
 * player is at least a gamemaster, so without this a perfectly valid command is drawn in red and
 * only the server accepts it.
 */
public data class EntityEventPacket(
    val entityId: Int,
    val eventId: Byte,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeInt(entityId)
        buffer.writeByte(eventId)
    }

    public companion object {
        public const val ID: Int = 0x22

        /** parity: `EntityEvent.PERMISSION_LEVEL_ALL` and the four above it. */
        public const val PERMISSION_LEVEL_BASE: Int = 24

        public fun permissionLevel(
            entityId: Int,
            level: Int,
        ): EntityEventPacket = EntityEventPacket(entityId, (PERMISSION_LEVEL_BASE + level.coerceIn(0, 4)).toByte())

        public fun read(buffer: ByteBuffer): EntityEventPacket = EntityEventPacket(buffer.readInt(), buffer.readByte())
    }
}

public data class TrackedWaypointPacket(
    val operation: Operation,
    val entityUuid: Uuid,
    val style: String,
    val color: Int?,
    val x: Int,
    val y: Int,
    val z: Int,
) {
    /** parity: `ClientboundTrackedWaypointPacket.Operation`, written as its ordinal. */
    public enum class Operation {
        TRACK,
        UNTRACK,
        UPDATE,
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(operation.ordinal)
        // The identifier: `true` selects the UUID side of the either.
        buffer.writeBoolean(true)
        buffer.writeUuid(entityUuid)
        buffer.writeProtocolString(style)
        buffer.writeBoolean(color != null)
        // parity: `ByteBufCodecs.RGB_COLOR`, three bytes and not an int. Writing four disconnects
        // the client: it reads the waypoint type out of the leftover byte.
        color?.let {
            buffer.writeByte(((it shr 16) and 0xFF).toByte())
            buffer.writeByte(((it shr 8) and 0xFF).toByte())
            buffer.writeByte((it and 0xFF).toByte())
        }
        buffer.writeVarInt(WAYPOINT_TYPE_VEC3I)
        buffer.writeVarInt(x)
        buffer.writeVarInt(y)
        buffer.writeVarInt(z)
    }

    public companion object {
        public const val ID: Int = 0x8a

        /** parity: `TrackedWaypoint.Type`: EMPTY 0, VEC3I 1, CHUNK 2, AZIMUTH 3. */
        public const val WAYPOINT_TYPE_VEC3I: Int = 1

        public fun read(buffer: ByteBuffer): TrackedWaypointPacket {
            val operation = Operation.entries[buffer.readVarInt()]
            check(buffer.readBoolean()) { "only the uuid side of the identifier is written here" }
            val uuid = buffer.readUuid()
            val style = buffer.readProtocolString()
            val color =
                if (buffer.readBoolean()) {
                    ((buffer.readByte().toInt() and 0xFF) shl 16) or
                        ((buffer.readByte().toInt() and 0xFF) shl 8) or
                        (buffer.readByte().toInt() and 0xFF)
                } else {
                    null
                }
            check(buffer.readVarInt() == WAYPOINT_TYPE_VEC3I) { "only positioned waypoints are written here" }
            return TrackedWaypointPacket(
                operation,
                uuid,
                style,
                color,
                buffer.readVarInt(),
                buffer.readVarInt(),
                buffer.readVarInt(),
            )
        }
    }
}

/**
 * Clientbound, id 0x83: an entity's attributes and the modifiers on them. Mirrors
 * `ClientboundUpdateAttributesPacket`.
 *
 * This is how a client learns about mining efficiency: it reads `minecraft:mining_efficiency` off
 * its own attribute map, which only this packet fills, and not off the item in its hand.
 */
public data class UpdateAttributesPacket(
    val entityId: Int,
    val attributes: List<AttributeSnapshot>,
) {
    public data class AttributeSnapshot(
        /** The `minecraft:attribute` registry id. */
        val attribute: Int,
        val base: Double,
        val modifiers: List<Modifier> = emptyList(),
    )

    public data class Modifier(
        val id: Identifier,
        val amount: Double,
        val operation: Operation,
    )

    /** parity: `AttributeModifier.Operation`, in its own declared order. */
    public enum class Operation {
        ADD_VALUE,
        ADD_MULTIPLIED_BASE,
        ADD_MULTIPLIED_TOTAL,
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeVarInt(attributes.size)
        for (attribute in attributes) {
            buffer.writeVarInt(attribute.attribute)
            buffer.writeDouble(attribute.base)
            buffer.writeVarInt(attribute.modifiers.size)
            for (modifier in attribute.modifiers) {
                buffer.writeProtocolString(modifier.id.toString())
                buffer.writeDouble(modifier.amount)
                buffer.writeVarInt(modifier.operation.ordinal)
            }
        }
    }

    public companion object {
        public const val ID: Int = 0x83

        public fun read(buffer: ByteBuffer): UpdateAttributesPacket {
            val entityId = buffer.readVarInt()
            val attributes =
                List(buffer.readVarInt()) {
                    val id = buffer.readVarInt()
                    val base = buffer.readDouble()
                    val modifiers =
                        List(buffer.readVarInt()) {
                            Modifier(
                                Identifier.parse(buffer.readProtocolString()),
                                buffer.readDouble(),
                                Operation.entries[buffer.readVarInt()],
                            )
                        }
                    AttributeSnapshot(id, base, modifiers)
                }
            return UpdateAttributesPacket(entityId, attributes)
        }
    }
}

/**
 * What made an entity flash red, and where the hit came from.
 *
 * parity: `ClientboundDamageEventPacket`. The damage type travels as a holder of the synced
 * `minecraft:damage_type` registry, and the two entity ids as "id plus one", so zero means nobody.
 */
public data class DamageEventPacket(
    val entityId: Int,
    /** The index of the damage type in the `minecraft:damage_type` registry, as sent to the client. */
    val damageType: Int,
    /** Whoever is blamed for the hit, or -1 for a hit nobody dealt. */
    val causeEntityId: Int = -1,
    /** What actually touched them (an arrow, say), or -1. */
    val directEntityId: Int = -1,
    val sourceX: Double? = null,
    val sourceY: Double? = null,
    val sourceZ: Double? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        // A holder: 0 would mean the type follows inline, so a registry id is written as id + 1.
        buffer.writeVarInt(damageType + 1)
        buffer.writeVarInt(causeEntityId + 1)
        buffer.writeVarInt(directEntityId + 1)
        val hasPosition = sourceX != null && sourceY != null && sourceZ != null
        buffer.writeBoolean(hasPosition)
        if (hasPosition) {
            buffer.writeDouble(sourceX)
            buffer.writeDouble(sourceY)
            buffer.writeDouble(sourceZ)
        }
    }

    public companion object {
        public const val ID: Int = 0x19

        public fun read(buffer: ByteBuffer): DamageEventPacket {
            val entityId = buffer.readVarInt()
            val damageType = buffer.readVarInt() - 1
            val causeEntityId = buffer.readVarInt() - 1
            val directEntityId = buffer.readVarInt() - 1
            if (!buffer.readBoolean()) return DamageEventPacket(entityId, damageType, causeEntityId, directEntityId)
            return DamageEventPacket(
                entityId,
                damageType,
                causeEntityId,
                directEntityId,
                buffer.readDouble(),
                buffer.readDouble(),
                buffer.readDouble(),
            )
        }
    }
}

/**
 * Clientbound, id 0x84: one status effect on an entity. Mirrors `ClientboundUpdateMobEffectPacket`.
 *
 * The effect id travels bare: `MobEffect.STREAM_CODEC` is `holderRegistry`, not `holder`, so there
 * is no `+1` here. The two are easy to confuse and the wrong one silently shifts every effect.
 */
public data class UpdateMobEffectPacket(
    val entityId: Int,
    /** The `minecraft:mob_effect` registry id. */
    val effect: Int,
    val amplifier: Int,
    val durationTicks: Int,
    val ambient: Boolean = false,
    val visible: Boolean = true,
    val showIcon: Boolean = true,
    val blend: Boolean = false,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeVarInt(effect)
        buffer.writeVarInt(amplifier)
        buffer.writeVarInt(durationTicks)
        var flags = 0
        if (ambient) flags = flags or FLAG_AMBIENT
        if (visible) flags = flags or FLAG_VISIBLE
        if (showIcon) flags = flags or FLAG_SHOW_ICON
        if (blend) flags = flags or FLAG_BLEND
        buffer.writeByte(flags.toByte())
    }

    public companion object {
        public const val ID: Int = 0x84

        private const val FLAG_AMBIENT = 1
        private const val FLAG_VISIBLE = 2
        private const val FLAG_SHOW_ICON = 4
        private const val FLAG_BLEND = 8

        public fun read(buffer: ByteBuffer): UpdateMobEffectPacket {
            val entityId = buffer.readVarInt()
            val effect = buffer.readVarInt()
            val amplifier = buffer.readVarInt()
            val duration = buffer.readVarInt()
            val flags = buffer.readByte().toInt()
            return UpdateMobEffectPacket(
                entityId,
                effect,
                amplifier,
                duration,
                ambient = flags and FLAG_AMBIENT != 0,
                visible = flags and FLAG_VISIBLE != 0,
                showIcon = flags and FLAG_SHOW_ICON != 0,
                blend = flags and FLAG_BLEND != 0,
            )
        }
    }
}

/** Clientbound, id 0x4e: one status effect gone. Mirrors `ClientboundRemoveMobEffectPacket`. */
public data class RemoveMobEffectPacket(
    val entityId: Int,
    val effect: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeVarInt(effect)
    }

    public companion object {
        public const val ID: Int = 0x4e

        public fun read(buffer: ByteBuffer): RemoveMobEffectPacket =
            RemoveMobEffectPacket(buffer.readVarInt(), buffer.readVarInt())
    }
}
