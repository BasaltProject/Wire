package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/**
 * The extra data a particle type carries after its id. parity: each `ParticleOptions`' own stream
 * codec, which is written straight after the type in `ClientboundLevelParticlesPacket`.
 *
 * One case per *shape* rather than per particle: vanilla's twenty-three particles with options
 * share nine codecs between them (five write a block state, three a colour, two a geyser's water
 * blocks), and which shape a type uses is the registry's business, not this file's.
 */
public sealed interface ParticleOptions {
    public fun write(buffer: ByteBuffer)

    /** `block`, `block_marker`, `falling_dust`, `dust_pillar`, `block_crumble`: a block state id. */
    public data class BlockState(
        val stateId: Int,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeVarInt(stateId)
        }
    }

    /** `entity_effect`, `flash`, `tinted_leaves`: one packed colour, written as a plain int. */
    public data class Color(
        val color: Int,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeInt(color)
        }
    }

    /** `effect`, `instant_effect`: a colour and a power. */
    public data class Spell(
        val color: Int,
        val power: Float,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeInt(color)
            buffer.writeFloat(power)
        }
    }

    /** `dust`: a colour and a scale. */
    public data class Dust(
        val color: Int,
        val scale: Float,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeInt(color)
            buffer.writeFloat(scale)
        }
    }

    /** `dust_color_transition`: the two ends of the fade, and a scale. */
    public data class DustTransition(
        val fromColor: Int,
        val toColor: Int,
        val scale: Float,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeInt(fromColor)
            buffer.writeInt(toColor)
            buffer.writeFloat(scale)
        }
    }

    /**
     * `item`: the stack it is drawn as.
     *
     * parity: `ItemStackTemplate.STREAM_CODEC`, which is **not** the ordinary stack codec: the item
     * comes first and the count second, and there is no "empty" short form, because a template is
     * never empty.
     */
    public data class Item(
        val itemId: Int,
        val count: Int = 1,
        val components: List<ItemComponent> = emptyList(),
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeVarInt(itemId)
            buffer.writeVarInt(count)
            buffer.writeVarInt(components.size)
            buffer.writeVarInt(0)
            for (component in components) {
                buffer.writeVarInt(component.id)
                component.writePayload(buffer)
            }
        }
    }

    /** `trail`: where it is heading, its colour, and how long it takes. */
    public data class Trail(
        val targetX: Double,
        val targetY: Double,
        val targetZ: Double,
        val color: Int,
        val duration: Int,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeDouble(targetX)
            buffer.writeDouble(targetY)
            buffer.writeDouble(targetZ)
            buffer.writeInt(color)
            buffer.writeVarInt(duration)
        }
    }

    /** `sculk_charge` (a roll) and `dragon_breath` (a power): one float either way. */
    public data class Number(
        val value: Float,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeFloat(value)
        }
    }

    /** `shriek`: how long the client waits before drawing it. */
    public data class Delay(
        val ticks: Int,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeVarInt(ticks)
        }
    }

    /** `geyser`, `geyser_plume`: how many water blocks fed it. */
    public data class Geyser(
        val waterBlocks: Int,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeInt(waterBlocks)
        }
    }

    /** `geyser_base`, `geyser_poof`: the same, plus the impulse of the burst. */
    public data class GeyserBase(
        val waterBlocks: Int,
        val burstImpulseBase: Float,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeInt(waterBlocks)
            buffer.writeFloat(burstImpulseBase)
        }
    }

    /**
     * `vibration`: where the signal is going and when it lands.
     *
     * Only the block form of `PositionSource` is written (type 0), which is the one `/particle`
     * itself can name; the entity form needs an entity that the command has no way to give it.
     */
    public data class Vibration(
        val x: Int,
        val y: Int,
        val z: Int,
        val arrivalTicks: Int,
    ) : ParticleOptions {
        override fun write(buffer: ByteBuffer) {
            buffer.writeVarInt(BLOCK_POSITION_SOURCE)
            buffer.writeLong(BlockPos(x, y, z).pack())
            buffer.writeVarInt(arrivalTicks)
        }

        private companion object {
            /** parity: `minecraft:block`'s index in the `position_source_type` registry. */
            const val BLOCK_POSITION_SOURCE = 0
        }
    }

    public companion object {
        /** Reads the options of a type whose [shape] the caller knows: the proxy's side of this packet. */
        public fun read(
            buffer: ByteBuffer,
            shape: ParticleOptionShape,
        ): ParticleOptions? =
            when (shape) {
                ParticleOptionShape.NONE -> null
                ParticleOptionShape.BLOCK_STATE -> BlockState(buffer.readVarInt())
                ParticleOptionShape.COLOR -> Color(buffer.readInt())
                ParticleOptionShape.SPELL -> Spell(buffer.readInt(), buffer.readFloat())
                ParticleOptionShape.DUST -> Dust(buffer.readInt(), buffer.readFloat())
                ParticleOptionShape.DUST_TRANSITION ->
                    DustTransition(buffer.readInt(), buffer.readInt(), buffer.readFloat())
                ParticleOptionShape.ITEM -> {
                    val itemId = buffer.readVarInt()
                    val count = buffer.readVarInt()
                    val added = buffer.readVarInt()
                    val removed = buffer.readVarInt()
                    val components =
                        List(added) {
                            val id = buffer.readVarInt()
                            ItemComponent.readPayload(buffer, id, length = null)
                                ?: throw org.basaltmc.wire.protocol.ProtocolException(
                                    "Data component $id is not supported yet on a particle's item",
                                )
                        }
                    repeat(removed) { buffer.readVarInt() }
                    Item(itemId, count, components)
                }
                ParticleOptionShape.TRAIL ->
                    Trail(
                        buffer.readDouble(),
                        buffer.readDouble(),
                        buffer.readDouble(),
                        buffer.readInt(),
                        buffer.readVarInt(),
                    )
                ParticleOptionShape.NUMBER -> Number(buffer.readFloat())
                ParticleOptionShape.DELAY -> Delay(buffer.readVarInt())
                ParticleOptionShape.GEYSER -> Geyser(buffer.readInt())
                ParticleOptionShape.GEYSER_BASE -> GeyserBase(buffer.readInt(), buffer.readFloat())
                ParticleOptionShape.VIBRATION -> {
                    buffer.readVarInt()
                    val pos = BlockPos.unpack(buffer.readLong())
                    Vibration(pos.x, pos.y, pos.z, buffer.readVarInt())
                }
            }
    }
}

/** Which of the option shapes a particle type carries. The registry says which type uses which. */
public enum class ParticleOptionShape {
    NONE,
    BLOCK_STATE,
    COLOR,
    SPELL,
    DUST,
    DUST_TRANSITION,
    ITEM,
    TRAIL,
    NUMBER,
    DELAY,
    GEYSER,
    GEYSER_BASE,
    VIBRATION,
}
