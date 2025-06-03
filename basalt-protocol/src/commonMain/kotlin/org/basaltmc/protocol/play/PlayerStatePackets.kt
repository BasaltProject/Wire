package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.readAnonymousNbt
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeAnonymousNbt
import org.basaltmc.protocol.writeVarInt
import org.basaltmc.text.Component
import org.basaltmc.text.ComponentNbt

/**
 * Clientbound, id 0x68: the health, food and saturation bars. Mirrors `ClientboundSetHealthPacket`.
 *
 * [saturation] is invisible in the HUD but not unobservable: the client uses it to decide whether
 * the food bar shakes, so sending a wrong value is a visible divergence.
 */
public data class SetHealthPacket(
    val health: Float,
    val food: Int,
    val saturation: Float,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeFloat(health)
        buffer.writeVarInt(food)
        buffer.writeFloat(saturation)
    }

    public companion object {
        public const val ID: Int = 0x68

        public fun read(buffer: ByteBuffer): SetHealthPacket =
            SetHealthPacket(buffer.readFloat(), buffer.readVarInt(), buffer.readFloat())
    }
}

/**
 * Clientbound, id 0x67: the experience bar. Mirrors `ClientboundSetExperiencePacket`.
 *
 * The wire order is progress, level, total, which is not the order vanilla's own constructor takes
 * its arguments in: kept as declared here to match the packet, not the constructor.
 */
public data class SetExperiencePacket(
    val progress: Float,
    val level: Int,
    val total: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeFloat(progress)
        buffer.writeVarInt(level)
        buffer.writeVarInt(total)
    }

    public companion object {
        public const val ID: Int = 0x67

        public fun read(buffer: ByteBuffer): SetExperiencePacket =
            SetExperiencePacket(buffer.readFloat(), buffer.readVarInt(), buffer.readVarInt())
    }
}

/**
 * Clientbound, id 0x40: what the player is allowed to do and how fast they move. Mirrors
 * `ClientboundPlayerAbilitiesPacket`; the defaults are vanilla's own `Abilities`.
 */
public data class ClientboundPlayerAbilitiesPacket(
    val invulnerable: Boolean,
    val flying: Boolean,
    val mayFly: Boolean,
    val instabuild: Boolean,
    val flyingSpeed: Float = DEFAULT_FLYING_SPEED,
    val walkingSpeed: Float = DEFAULT_WALKING_SPEED,
) {
    public fun write(buffer: ByteBuffer) {
        var flags = 0
        if (invulnerable) flags = flags or FLAG_INVULNERABLE
        if (flying) flags = flags or FLAG_FLYING
        if (mayFly) flags = flags or FLAG_MAY_FLY
        if (instabuild) flags = flags or FLAG_INSTABUILD
        buffer.writeByte(flags.toByte())
        buffer.writeFloat(flyingSpeed)
        buffer.writeFloat(walkingSpeed)
    }

    public companion object {
        public const val ID: Int = 0x40

        public const val DEFAULT_FLYING_SPEED: Float = 0.05f
        public const val DEFAULT_WALKING_SPEED: Float = 0.1f

        private const val FLAG_INVULNERABLE = 1
        private const val FLAG_FLYING = 2
        private const val FLAG_MAY_FLY = 4
        private const val FLAG_INSTABUILD = 8

        public fun read(buffer: ByteBuffer): ClientboundPlayerAbilitiesPacket {
            val flags = buffer.readByte().toInt()
            return ClientboundPlayerAbilitiesPacket(
                invulnerable = flags and FLAG_INVULNERABLE != 0,
                flying = flags and FLAG_FLYING != 0,
                mayFly = flags and FLAG_MAY_FLY != 0,
                instabuild = flags and FLAG_INSTABUILD != 0,
                flyingSpeed = buffer.readFloat(),
                walkingSpeed = buffer.readFloat(),
            )
        }
    }
}

/**
 * Serverbound, id 0x28: the client toggling flight. Mirrors `ServerboundPlayerAbilitiesPacket`,
 * which sends the same bitfield as its clientbound counterpart but only reads the flying bit.
 */
public data class ServerboundPlayerAbilitiesPacket(
    val flying: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeByte(if (flying) FLAG_FLYING.toByte() else 0)
    }

    public companion object {
        public const val ID: Int = 0x28

        private const val FLAG_FLYING = 2

        public fun read(buffer: ByteBuffer): ServerboundPlayerAbilitiesPacket =
            ServerboundPlayerAbilitiesPacket(buffer.readByte().toInt() and FLAG_FLYING != 0)
    }
}

/**
 * Clientbound, id 0x52: re-enters the same or another dimension without a full reconnect, which is
 * what a respawn is. Mirrors `ClientboundRespawnPacket`; [dataToKeep] is the same bitmask vanilla
 * declares ([KEEP_ATTRIBUTE_MODIFIERS], [KEEP_ENTITY_DATA]).
 */
public data class RespawnPacket(
    val spawnInfo: SpawnInfo,
    val dataToKeep: Byte,
) {
    public fun write(buffer: ByteBuffer) {
        spawnInfo.write(buffer)
        buffer.writeByte(dataToKeep)
    }

    public companion object {
        public const val ID: Int = 0x52

        public const val KEEP_ATTRIBUTE_MODIFIERS: Byte = 1
        public const val KEEP_ENTITY_DATA: Byte = 2
        public const val KEEP_ALL_DATA: Byte = 3

        public fun read(buffer: ByteBuffer): RespawnPacket = RespawnPacket(SpawnInfo.read(buffer), buffer.readByte())
    }
}

/**
 * Serverbound, id 0x0c: the client asking for something only the server can start. Mirrors
 * `ServerboundClientCommandPacket`; [Action.PERFORM_RESPAWN] is the "Respawn" button.
 */
public data class ClientCommandPacket(
    val action: Action,
) {
    public enum class Action {
        PERFORM_RESPAWN,
        REQUEST_STATS,
        REQUEST_GAMERULE_VALUES,
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(action.ordinal)
    }

    public companion object {
        public const val ID: Int = 0x0c

        public fun read(buffer: ByteBuffer): ClientCommandPacket =
            ClientCommandPacket(Action.entries[buffer.readVarInt()])
    }
}

/**
 * Serverbound, id 0x2a: a state the client entered on its own. Mirrors
 * `ServerboundPlayerCommandPacket`.
 *
 * Sneaking is not in this list: it moved to `ServerboundPlayerInputPacket`, which is the *next* id
 * (0x2b) and carries a one-byte bitfield rather than these three varints. Sprinting is here, and
 * the server needs it because sprinting is the only movement that costs hunger in 26.2.
 */
public data class PlayerCommandPacket(
    val entityId: Int,
    val action: Action,
    val data: Int,
) {
    public enum class Action {
        STOP_SLEEPING,
        START_SPRINTING,
        STOP_SPRINTING,
        START_RIDING_JUMP,
        STOP_RIDING_JUMP,
        OPEN_INVENTORY,
        START_FALL_FLYING,
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeVarInt(action.ordinal)
        buffer.writeVarInt(data)
    }

    public companion object {
        public const val ID: Int = 0x2a

        public fun read(buffer: ByteBuffer): PlayerCommandPacket =
            PlayerCommandPacket(buffer.readVarInt(), Action.entries[buffer.readVarInt()], buffer.readVarInt())
    }
}

/**
 * Clientbound, id 0x44: the death screen, carrying the message shown on it. Mirrors
 * `ClientboundPlayerCombatKillPacket`. Without this the client never learns it died, so it keeps
 * playing while the server considers it dead.
 */
public data class PlayerCombatKillPacket(
    val playerId: Int,
    val message: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(playerId)
        buffer.writeAnonymousNbt(ComponentNbt.serialize(message))
    }

    public companion object {
        public const val ID: Int = 0x44

        public fun read(buffer: ByteBuffer): PlayerCombatKillPacket =
            PlayerCombatKillPacket(buffer.readVarInt(), ComponentNbt.deserialize(buffer.readAnonymousNbt()))
    }
}

/**
 * Clientbound, id 0x4d: stop tracking these entities. Mirrors `ClientboundRemoveEntitiesPacket`.
 *
 * Needed the moment a server has more than one world: a player who leaves for another world is
 * still an entity everyone in the old one is tracking, and without this they stay frozen in place
 * there forever.
 */
public data class RemoveEntitiesPacket(
    val entityIds: List<Int>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityIds.size)
        for (id in entityIds) buffer.writeVarInt(id)
    }

    public companion object {
        public const val ID: Int = 0x4d

        public fun read(buffer: ByteBuffer): RemoveEntitiesPacket =
            RemoveEntitiesPacket(List(buffer.readVarInt()) { buffer.readVarInt() })
    }
}
