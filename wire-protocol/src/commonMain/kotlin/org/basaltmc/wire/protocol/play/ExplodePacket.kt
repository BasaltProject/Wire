package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * One particle kind thrown out of an explosion, and how big and fast it goes.
 *
 * parity: `ExplosionParticleInfo`, carried in a weighted list so an explosion can throw a mix.
 */
public data class ExplosionParticle(
    val particleId: Int,
    val scaling: Float,
    val speed: Float,
    val weight: Int = 1,
)

/**
 * Clientbound, id 0x24: an explosion happened here. Mirrors `ClientboundExplodePacket`.
 *
 * The blocks it broke are *not* in this packet: those go out as ordinary block updates. What this
 * carries is everything the client cannot work out for itself, which is the whole reason an
 * explosion looks and sounds like one: the flash, the smoke thrown off the blocks, the bang, and
 * how hard whoever is standing nearby was pushed.
 */
public data class ExplodePacket(
    val x: Double,
    val y: Double,
    val z: Double,
    val radius: Float,
    val blockCount: Int,
    val knockbackX: Double? = null,
    val knockbackY: Double = 0.0,
    val knockbackZ: Double = 0.0,
    val particleId: Int,
    val sound: String,
    val blockParticles: List<ExplosionParticle> = emptyList(),
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeDouble(x)
        buffer.writeDouble(y)
        buffer.writeDouble(z)
        buffer.writeFloat(radius)
        // parity: a plain int, not a varint: `ByteBufCodecs.INT`.
        buffer.writeInt(blockCount)
        val knockback = knockbackX
        buffer.writeBoolean(knockback != null)
        if (knockback != null) {
            buffer.writeDouble(knockback)
            buffer.writeDouble(knockbackY)
            buffer.writeDouble(knockbackZ)
        }
        buffer.writeVarInt(particleId)
        // The sound travels as a direct holder, the same way [SoundPacket] carries one.
        buffer.writeVarInt(0)
        buffer.writeProtocolString(sound)
        buffer.writeBoolean(false)
        buffer.writeVarInt(blockParticles.size)
        for (particle in blockParticles) {
            buffer.writeVarInt(particle.particleId)
            buffer.writeFloat(particle.scaling)
            buffer.writeFloat(particle.speed)
            buffer.writeVarInt(particle.weight)
        }
    }

    public companion object {
        public const val ID: Int = 0x24

        /** Only the sound holder written by [write] is understood: a direct name, never a registry id. */
        public fun read(buffer: ByteBuffer): ExplodePacket {
            val x = buffer.readDouble()
            val y = buffer.readDouble()
            val z = buffer.readDouble()
            val radius = buffer.readFloat()
            val blockCount = buffer.readInt()
            val hasKnockback = buffer.readBoolean()
            val knockbackX = if (hasKnockback) buffer.readDouble() else null
            val knockbackY = if (hasKnockback) buffer.readDouble() else 0.0
            val knockbackZ = if (hasKnockback) buffer.readDouble() else 0.0
            val particleId = buffer.readVarInt()
            buffer.readVarInt()
            val sound = buffer.readProtocolString()
            buffer.readBoolean()
            val blockParticles =
                List(buffer.readVarInt()) {
                    ExplosionParticle(
                        particleId = buffer.readVarInt(),
                        scaling = buffer.readFloat(),
                        speed = buffer.readFloat(),
                        weight = buffer.readVarInt(),
                    )
                }
            return ExplodePacket(
                x = x,
                y = y,
                z = z,
                radius = radius,
                blockCount = blockCount,
                knockbackX = knockbackX,
                knockbackY = knockbackY,
                knockbackZ = knockbackZ,
                particleId = particleId,
                sound = sound,
                blockParticles = blockParticles,
            )
        }

        /** parity: `Level.DEFAULT_EXPLOSION_BLOCK_PARTICLES`, a poof and some smoke, evenly weighted. */
        public const val POOF_SCALING: Float = 0.5f
        public const val SMOKE_SCALING: Float = 1.0f
    }
}
