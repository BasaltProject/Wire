package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ExplodePacket
import org.basaltmc.wire.protocol.play.SetEntityMotionPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.SpawnPositionPacket
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.math.roundToInt

/**
 * 1.21.9 to 1.21.8. Three packets changed shape, all of them about where something is or how fast
 * it is going.
 *
 * 1.21.9 replaced the three shorts a velocity used to be with a packed vector, and moved it in
 * front of the angles when an entity spawns. And the spawn point grew a dimension and a pitch,
 * which a 1.21.8 client has nowhere to put.
 */
internal class Link772(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        // Renumbering first, shape second: the numbers are read out of the packet as this server
        // writes it, and what is written here is no longer that packet.
        if (!super.clientbound(state, packet)) return false
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            SetEntityMotionPacket.ID ->
                rewrite(packet) { input, output ->
                    val motion = SetEntityMotionPacket.read(input)
                    output.writeVarInt(motion.entityId)
                    writeShortVelocity(output, motion.velocityX, motion.velocityY, motion.velocityZ)
                }
            SpawnEntityPacket.ID ->
                rewrite(packet) { input, output ->
                    val spawn = SpawnEntityPacket.read(input)
                    output.writeVarInt(spawn.entityId)
                    output.writeLong(spawn.uuid.toLongs { most, _ -> most })
                    output.writeLong(spawn.uuid.toLongs { _, least -> least })
                    output.writeVarInt(spawn.entityType)
                    output.writeDouble(spawn.x)
                    output.writeDouble(spawn.y)
                    output.writeDouble(spawn.z)
                    output.writeByte(angleOf(spawn.pitch))
                    output.writeByte(angleOf(spawn.yaw))
                    output.writeByte(angleOf(spawn.headYaw))
                    output.writeVarInt(spawn.objectData)
                    writeShortVelocity(output, spawn.velocityX, spawn.velocityY, spawn.velocityZ)
                }
            ExplodePacket.ID ->
                rewrite(packet) { input, output ->
                    val explode = ExplodePacket.read(input)
                    output.writeDouble(explode.x)
                    output.writeDouble(explode.y)
                    output.writeDouble(explode.z)
                    val knockback = explode.knockbackX
                    output.writeBoolean(knockback != null)
                    if (knockback != null) {
                        output.writeDouble(knockback)
                        output.writeDouble(explode.knockbackY)
                        output.writeDouble(explode.knockbackZ)
                    }
                    output.writeVarInt(explode.particleId)
                    // The sound as a direct holder, which is how this server always writes one.
                    output.writeVarInt(0)
                    output.writeProtocolString(explode.sound)
                    output.writeBoolean(false)
                }
            SpawnPositionPacket.ID ->
                rewrite(packet) { input, output ->
                    val spawn = SpawnPositionPacket.read(input)
                    // The dimension and the pitch have nowhere to go: 1.21.8 has one spawn point,
                    // in the world the player is in, and one angle to face it with.
                    output.writeLong(spawn.globalPos.location.pack())
                    output.writeFloat(spawn.yaw)
                }
        }
        return true
    }

    private fun writeShortVelocity(
        output: org.basaltmc.wire.io.ByteBuffer,
        x: Double,
        y: Double,
        z: Double,
    ) {
        output.writeShort(quantize(x))
        output.writeShort(quantize(y))
        output.writeShort(quantize(z))
    }

    private companion object {
        /** parity: `ClientboundSetEntityMotionPacket` before 1.21.9, which stored blocks per tick times 8000. */
        const val VELOCITY_SCALE = 8000.0
        const val VELOCITY_LIMIT = 3.9

        fun quantize(value: Double): Short =
            (value.coerceIn(-VELOCITY_LIMIT, VELOCITY_LIMIT) * VELOCITY_SCALE).roundToInt().toShort()

        /** The 256ths of a turn an angle travels as. */
        fun angleOf(degrees: Float): Byte = (degrees * 256f / 360f).roundToInt().toByte()
    }
}
