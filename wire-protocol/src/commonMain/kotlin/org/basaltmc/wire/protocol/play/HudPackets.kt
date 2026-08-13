package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentNbt

/** Clientbound, id 0x72: the big line of a title. Mirrors `ClientboundSetTitleTextPacket`. */
public data class SetTitleTextPacket(
    val text: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeAnonymousNbt(ComponentNbt.serialize(text))
    }

    public companion object {
        public const val ID: Int = 0x72

        public fun read(buffer: ByteBuffer): SetTitleTextPacket =
            SetTitleTextPacket(ComponentNbt.deserialize(buffer.readAnonymousNbt()))
    }
}

/** Clientbound, id 0x70: the smaller line under a title. Mirrors `ClientboundSetSubtitleTextPacket`. */
public data class SetSubtitleTextPacket(
    val text: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeAnonymousNbt(ComponentNbt.serialize(text))
    }

    public companion object {
        public const val ID: Int = 0x70

        public fun read(buffer: ByteBuffer): SetSubtitleTextPacket =
            SetSubtitleTextPacket(ComponentNbt.deserialize(buffer.readAnonymousNbt()))
    }
}

/**
 * Clientbound, id 0x57: the line above the hotbar. Mirrors `ClientboundSetActionBarTextPacket`.
 *
 * Distinct from [SystemChatPacket] with its action-bar flag: this one is the *title* system's
 * action bar, which `/title ... actionbar` sets and `/title ... clear` removes.
 */
public data class SetActionBarTextPacket(
    val text: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeAnonymousNbt(ComponentNbt.serialize(text))
    }

    public companion object {
        public const val ID: Int = 0x57

        public fun read(buffer: ByteBuffer): SetActionBarTextPacket =
            SetActionBarTextPacket(ComponentNbt.deserialize(buffer.readAnonymousNbt()))
    }
}

/** Clientbound, id 0x73: how long a title fades in, stays and fades out, in ticks. */
public data class SetTitlesAnimationPacket(
    val fadeIn: Int,
    val stay: Int,
    val fadeOut: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeInt(fadeIn)
        buffer.writeInt(stay)
        buffer.writeInt(fadeOut)
    }

    public companion object {
        public const val ID: Int = 0x73

        public fun read(buffer: ByteBuffer): SetTitlesAnimationPacket =
            SetTitlesAnimationPacket(buffer.readInt(), buffer.readInt(), buffer.readInt())
    }
}

/** Clientbound, id 0x0e: hides whatever title is showing, optionally resetting its timings too. */
public data class ClearTitlesPacket(
    val resetTimes: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeBoolean(resetTimes)
    }

    public companion object {
        public const val ID: Int = 0x0e

        public fun read(buffer: ByteBuffer): ClearTitlesPacket = ClearTitlesPacket(buffer.readBoolean())
    }
}

/** The mixer a sound plays through. Ordinals are what the wire carries. parity: `SoundSource`. */
public enum class SoundSource {
    MASTER,
    MUSIC,
    RECORDS,
    WEATHER,
    BLOCKS,
    HOSTILE,
    NEUTRAL,
    PLAYERS,
    AMBIENT,
    VOICE,
    UI,
    ;

    /** The name `/playsound` and `/stopsound` take, which is not always the constant's own. */
    public val commandName: String
        get() =
            when (this) {
                RECORDS -> "record"
                BLOCKS -> "block"
                PLAYERS -> "player"
                else -> name.lowercase()
            }

    public companion object {
        public fun byCommandName(name: String): SoundSource? = entries.firstOrNull { it.commandName == name }
    }
}

/**
 * Clientbound, id 0x75: play a sound at a position. Mirrors `ClientboundSoundPacket`.
 *
 * The sound travels as a *direct* holder (a leading 0, then the id) rather than as a registry
 * index: the client resolves the name itself that way, so a server that has not built the
 * `sound_event` registry table can still name any sound. Coordinates are eighths of a block, which
 * is vanilla's own `LOCATION_ACCURACY`.
 */
public data class SoundPacket(
    val sound: String,
    val source: SoundSource,
    val x: Double,
    val y: Double,
    val z: Double,
    val volume: Float,
    val pitch: Float,
    val seed: Long,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(0)
        buffer.writeProtocolString(sound)
        // The optional fixed range of a direct sound event: absent, so the client uses volume.
        buffer.writeBoolean(false)
        buffer.writeVarInt(source.ordinal)
        buffer.writeInt((x * LOCATION_ACCURACY).toInt())
        buffer.writeInt((y * LOCATION_ACCURACY).toInt())
        buffer.writeInt((z * LOCATION_ACCURACY).toInt())
        buffer.writeFloat(volume)
        buffer.writeFloat(pitch)
        buffer.writeLong(seed)
    }

    public companion object {
        public const val ID: Int = 0x75

        /** parity: `ClientboundSoundPacket.LOCATION_ACCURACY`. */
        public const val LOCATION_ACCURACY: Double = 8.0

        public fun read(buffer: ByteBuffer): SoundPacket {
            check(buffer.readVarInt() == 0) { "only direct sound holders are written here" }
            val sound = buffer.readProtocolString()
            check(!buffer.readBoolean()) { "a fixed range is never written here" }
            val source = SoundSource.entries[buffer.readVarInt()]
            val x = buffer.readInt() / LOCATION_ACCURACY
            val y = buffer.readInt() / LOCATION_ACCURACY
            val z = buffer.readInt() / LOCATION_ACCURACY
            return SoundPacket(sound, source, x, y, z, buffer.readFloat(), buffer.readFloat(), buffer.readLong())
        }
    }
}

/**
 * Clientbound, id 0x77: stop one sound, every sound from one source, or all of them. Mirrors
 * `ClientboundStopSoundPacket`, flag byte included: 1 means a source follows, 2 a name.
 */
public data class StopSoundPacket(
    val sound: String?,
    val source: SoundSource?,
) {
    public fun write(buffer: ByteBuffer) {
        var flags = 0
        if (source != null) flags = flags or 1
        if (sound != null) flags = flags or 2
        buffer.writeByte(flags.toByte())
        if (source != null) buffer.writeVarInt(source.ordinal)
        if (sound != null) buffer.writeProtocolString(sound)
    }

    public companion object {
        public const val ID: Int = 0x77

        public fun read(buffer: ByteBuffer): StopSoundPacket {
            val flags = buffer.readByte().toInt()
            val source = if (flags and 1 != 0) SoundSource.entries[buffer.readVarInt()] else null
            val sound = if (flags and 2 != 0) buffer.readProtocolString() else null
            return StopSoundPacket(sound, source)
        }
    }
}
