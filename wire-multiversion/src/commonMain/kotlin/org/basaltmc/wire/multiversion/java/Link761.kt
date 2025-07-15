package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.ComposedTables
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.DamageEventPacket
import org.basaltmc.wire.protocol.play.HurtAnimationPacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.UpdateMobEffectPacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.19.4 to 1.19.3.
 *
 * 1.19.4 folded the packet that spawned a player into the one that spawns anything else, and split
 * being hurt out of the entity events into two packets of its own. Both go back here: a player
 * spawns with its own packet again, and a hit is an entity event and an animation.
 */
internal class Link761(
    tables: VersionTables,
    private val target: ComposedTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            SpawnEntityPacket.ID -> {
                // The shape here is the one the link to 1.21.8 left: the velocity is three shorts
                // at the end and the angles are bytes.
                val input = decode(packet) { it }
                val entityId = input.readVarInt()
                val mostSignificant = input.readLong()
                val leastSignificant = input.readLong()
                val type = input.readVarInt()
                if (!target.isPlayer(type)) return true
                val x = input.readDouble()
                val y = input.readDouble()
                val z = input.readDouble()
                val pitch = input.readByte()
                val yaw = input.readByte()
                packet.overrideId(ADD_PLAYER_761)
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeLong(mostSignificant)
                    output.writeLong(leastSignificant)
                    output.writeDouble(x)
                    output.writeDouble(y)
                    output.writeDouble(z)
                    output.writeByte(yaw)
                    output.writeByte(pitch)
                }
            }
            PlayerPositionPacket.ID ->
                encode(packet) { output ->
                    output.writeBytes(packet.body)
                    // 1.19.3 is told whether this teleport takes the player off whatever they rode.
                    output.writeBoolean(false)
                }
            DamageEventPacket.ID -> {
                val damage = decode(packet) { DamageEventPacket.read(it) }
                packet.overrideId(ENTITY_EVENT_761)
                encode(packet) { output ->
                    output.writeInt(damage.entityId)
                    output.writeByte(HURT_EVENT)
                }
            }
            HurtAnimationPacket.ID -> {
                val hurt = decode(packet) { HurtAnimationPacket.read(it) }
                packet.overrideId(ANIMATE_761)
                encode(packet) { output ->
                    output.writeVarInt(hurt.entityId)
                    output.writeByte(HURT_ANIMATION)
                }
            }
            UpdateMobEffectPacket.ID -> {
                val effect = decode(packet) { UpdateMobEffectPacket.read(it) }
                if (effect.durationTicks >= 0) return true
                // An effect that never ends is new in 1.19.4. A number this big is the same thing
                // to anyone watching it count down.
                encode(packet) { effect.copy(durationTicks = PRACTICALLY_FOREVER).write(it) }
            }
        }
        return true
    }

    private companion object {
        /**
         * A player, as 1.19.4 numbers entity types.
         *
         * Not this server's number: by the time a link sees a packet the ids in it are already the
         * ones of the version above this link, which is 1.19.4 here.
         */
        const val PLAYER_IN_1_19_4 = 122

        const val ADD_PLAYER_761 = 2
        const val ENTITY_EVENT_761 = 25
        const val ANIMATE_761 = 3

        /** parity: `EntityEvent.HURT` and `AnimateAction.HURT`, the two halves of being hit. */
        const val HURT_EVENT: Byte = 2
        const val HURT_ANIMATION: Byte = 1

        const val PRACTICALLY_FOREVER = 20 * 60 * 60 * 24
    }
}
