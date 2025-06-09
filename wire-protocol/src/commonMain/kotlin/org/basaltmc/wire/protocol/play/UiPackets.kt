package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.ProtocolException
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

/**
 * The header and footer of the player list, vanilla's `ClientboundTabListPacket`.
 *
 * Java only. Bedrock has no player list to decorate, so a Bedrock client behind the future
 * translation layer will simply never be sent one; that is a fact about the other game's UI, not a
 * gap here.
 */
public data class TabListPacket(
    val header: Component,
    val footer: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeAnonymousNbt(ComponentNbt.serialize(header))
        buffer.writeAnonymousNbt(ComponentNbt.serialize(footer))
    }

    public companion object {
        public const val ID: Int = 0x7a

        public fun read(buffer: ByteBuffer): TabListPacket =
            TabListPacket(
                ComponentNbt.deserialize(buffer.readAnonymousNbt()),
                ComponentNbt.deserialize(buffer.readAnonymousNbt()),
            )
    }
}

/**
 * How a score's number is drawn, vanilla's `NumberFormat` registry.
 *
 * [Blank] is the one that matters for a sidebar: it removes the red number on the right, which is
 * what turns a scoreboard objective into the panel every server actually wants. Registry ids come
 * from `NumberFormatTypes.bootstrap`, in its registration order.
 */
public sealed interface NumberFormat {
    public val typeId: Int

    public data object Blank : NumberFormat {
        override val typeId: Int get() = 0
    }

    public data class Fixed(
        val value: Component,
    ) : NumberFormat {
        override val typeId: Int get() = 2
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(typeId)
        if (this is Fixed) buffer.writeAnonymousNbt(ComponentNbt.serialize(value))
    }

    public companion object {
        // `styled` (1) is deliberately not implemented: it carries a Style, and nothing here needs a
        // number that is coloured rather than absent or replaced.
        public fun read(buffer: ByteBuffer): NumberFormat =
            when (val id = buffer.readVarInt()) {
                0 -> Blank
                2 -> Fixed(ComponentNbt.deserialize(buffer.readAnonymousNbt()))
                else -> throw ProtocolException("Unsupported number format type: $id")
            }
    }
}

/** parity: `DisplaySlot`, by its own numeric id. */
public enum class DisplaySlot(
    public val id: Int,
) {
    LIST(0),
    SIDEBAR(1),
    BELOW_NAME(2),
}

/** parity: `ObjectiveCriteria.RenderType`, written as an enum ordinal. */
public enum class ObjectiveRenderType {
    INTEGER,
    HEARTS,
}

/**
 * Creates, changes or removes a scoreboard objective, vanilla's `ClientboundSetObjectivePacket`.
 *
 * The display name and render type are only on the wire for add (0) and change (2), which is why
 * they are nullable here: a remove that wrote them would be a different packet length than the
 * client expects.
 */
public data class SetObjectivePacket(
    val objectiveName: String,
    val method: Int,
    val displayName: Component? = null,
    val renderType: ObjectiveRenderType = ObjectiveRenderType.INTEGER,
    val numberFormat: NumberFormat? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(objectiveName)
        buffer.writeByte(method.toByte())
        if (method != METHOD_ADD && method != METHOD_CHANGE) return
        buffer.writeAnonymousNbt(ComponentNbt.serialize(displayName ?: Component.empty()))
        buffer.writeVarInt(renderType.ordinal)
        buffer.writeBoolean(numberFormat != null)
        numberFormat?.write(buffer)
    }

    public companion object {
        public const val ID: Int = 0x6a
        public const val METHOD_ADD: Int = 0
        public const val METHOD_REMOVE: Int = 1
        public const val METHOD_CHANGE: Int = 2

        public fun read(buffer: ByteBuffer): SetObjectivePacket {
            val name = buffer.readProtocolString()
            val method = buffer.readByte().toInt()
            if (method != METHOD_ADD && method != METHOD_CHANGE) return SetObjectivePacket(name, method)
            val displayName = ComponentNbt.deserialize(buffer.readAnonymousNbt())
            val renderType = ObjectiveRenderType.entries[buffer.readVarInt()]
            val format = if (buffer.readBoolean()) NumberFormat.read(buffer) else null
            return SetObjectivePacket(name, method, displayName, renderType, format)
        }
    }
}

/** Puts an objective in a slot, or clears the slot with an empty name. */
public data class SetDisplayObjectivePacket(
    val slot: DisplaySlot,
    val objectiveName: String,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(slot.id)
        buffer.writeProtocolString(objectiveName)
    }

    public companion object {
        public const val ID: Int = 0x62

        public fun read(buffer: ByteBuffer): SetDisplayObjectivePacket {
            val id = buffer.readVarInt()
            val slot = DisplaySlot.entries.firstOrNull { it.id == id } ?: DisplaySlot.LIST
            return SetDisplayObjectivePacket(slot, buffer.readProtocolString())
        }
    }
}

/**
 * One line of a scoreboard.
 *
 * [display] is what makes a modern sidebar bearable: the line's text is carried by the score itself
 * instead of being smuggled in through a team's prefix and suffix around a fake player name, which
 * is what every server had to do before this field existed.
 */
public data class SetScorePacket(
    val owner: String,
    val objectiveName: String,
    val score: Int,
    val display: Component? = null,
    val numberFormat: NumberFormat? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(owner)
        buffer.writeProtocolString(objectiveName)
        buffer.writeVarInt(score)
        buffer.writeBoolean(display != null)
        display?.let { buffer.writeAnonymousNbt(ComponentNbt.serialize(it)) }
        buffer.writeBoolean(numberFormat != null)
        numberFormat?.write(buffer)
    }

    public companion object {
        public const val ID: Int = 0x6e

        public fun read(buffer: ByteBuffer): SetScorePacket =
            SetScorePacket(
                owner = buffer.readProtocolString(),
                objectiveName = buffer.readProtocolString(),
                score = buffer.readVarInt(),
                display = if (buffer.readBoolean()) ComponentNbt.deserialize(buffer.readAnonymousNbt()) else null,
                numberFormat = if (buffer.readBoolean()) NumberFormat.read(buffer) else null,
            )
    }
}

/** Removes one line. A null objective removes that owner from every objective at once. */
public data class ResetScorePacket(
    val owner: String,
    val objectiveName: String? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(owner)
        buffer.writeBoolean(objectiveName != null)
        objectiveName?.let { buffer.writeProtocolString(it) }
    }

    public companion object {
        public const val ID: Int = 0x4f

        public fun read(buffer: ByteBuffer): ResetScorePacket =
            ResetScorePacket(
                owner = buffer.readProtocolString(),
                objectiveName = if (buffer.readBoolean()) buffer.readProtocolString() else null,
            )
    }
}

/** parity: `BossEvent.BossBarColor`, written as an enum ordinal. */
public enum class BossBarColor {
    PINK,
    BLUE,
    RED,
    GREEN,
    YELLOW,
    PURPLE,
    WHITE,
}

/** parity: `BossEvent.BossBarOverlay`: a plain bar, or one notched into segments. */
public enum class BossBarOverlay {
    PROGRESS,
    NOTCHED_6,
    NOTCHED_10,
    NOTCHED_12,
    NOTCHED_20,
    ;

    /** What `/bossbar set <id> style` takes, which is `BossBarOverlay.getSerializedName`. */
    public val commandName: String get() = name.lowercase()
}

/**
 * The bar across the top of the screen, vanilla's `ClientboundBossEventPacket`.
 *
 * One packet with an operation inside it rather than six packets, matching the wire exactly: a bar
 * is added once and then nudged, and re-adding it on every change makes the client's own fade
 * animation restart, which reads as a flicker.
 */
@OptIn(ExperimentalUuidApi::class)
public data class BossEventPacket(
    val id: Uuid,
    val operation: Operation,
) {
    public sealed interface Operation {
        public val type: Int

        public fun write(buffer: ByteBuffer)

        public data class Add(
            val name: Component,
            val progress: Float,
            val color: BossBarColor,
            val overlay: BossBarOverlay,
            val darkenScreen: Boolean = false,
            val playMusic: Boolean = false,
            val createWorldFog: Boolean = false,
        ) : Operation {
            override val type: Int get() = 0

            override fun write(buffer: ByteBuffer) {
                buffer.writeAnonymousNbt(ComponentNbt.serialize(name))
                buffer.writeFloat(progress)
                buffer.writeVarInt(color.ordinal)
                buffer.writeVarInt(overlay.ordinal)
                buffer.writeByte(flags(darkenScreen, playMusic, createWorldFog))
            }
        }

        public data object Remove : Operation {
            override val type: Int get() = 1

            override fun write(buffer: ByteBuffer) {
                // Nothing follows: the id in the packet is the whole message.
            }
        }

        public data class UpdateProgress(
            val progress: Float,
        ) : Operation {
            override val type: Int get() = 2

            override fun write(buffer: ByteBuffer) {
                buffer.writeFloat(progress)
            }
        }

        public data class UpdateName(
            val name: Component,
        ) : Operation {
            override val type: Int get() = 3

            override fun write(buffer: ByteBuffer) {
                buffer.writeAnonymousNbt(ComponentNbt.serialize(name))
            }
        }

        public data class UpdateStyle(
            val color: BossBarColor,
            val overlay: BossBarOverlay,
        ) : Operation {
            override val type: Int get() = 4

            override fun write(buffer: ByteBuffer) {
                buffer.writeVarInt(color.ordinal)
                buffer.writeVarInt(overlay.ordinal)
            }
        }

        public data class UpdateProperties(
            val darkenScreen: Boolean,
            val playMusic: Boolean,
            val createWorldFog: Boolean,
        ) : Operation {
            override val type: Int get() = 5

            override fun write(buffer: ByteBuffer) {
                buffer.writeByte(flags(darkenScreen, playMusic, createWorldFog))
            }
        }
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeUuid(id)
        buffer.writeVarInt(operation.type)
        operation.write(buffer)
    }

    public companion object {
        public const val ID: Int = 0x09

        private fun flags(
            darkenScreen: Boolean,
            playMusic: Boolean,
            createWorldFog: Boolean,
        ): Byte {
            var value = 0
            if (darkenScreen) value = value or 1
            if (playMusic) value = value or 2
            if (createWorldFog) value = value or 4
            return value.toByte()
        }

        public fun read(buffer: ByteBuffer): BossEventPacket {
            val id = buffer.readUuid()
            return BossEventPacket(
                id,
                when (val type = buffer.readVarInt()) {
                    0 -> {
                        val name = ComponentNbt.deserialize(buffer.readAnonymousNbt())
                        val progress = buffer.readFloat()
                        val color = BossBarColor.entries[buffer.readVarInt()]
                        val overlay = BossBarOverlay.entries[buffer.readVarInt()]
                        val flags = buffer.readByte().toInt() and 0xFF
                        Operation.Add(
                            name,
                            progress,
                            color,
                            overlay,
                            darkenScreen = flags and 1 != 0,
                            playMusic = flags and 2 != 0,
                            createWorldFog = flags and 4 != 0,
                        )
                    }
                    1 -> Operation.Remove
                    2 -> Operation.UpdateProgress(buffer.readFloat())
                    3 -> Operation.UpdateName(ComponentNbt.deserialize(buffer.readAnonymousNbt()))
                    4 ->
                        Operation.UpdateStyle(
                            BossBarColor.entries[buffer.readVarInt()],
                            BossBarOverlay.entries[buffer.readVarInt()],
                        )
                    5 -> {
                        val flags = buffer.readByte().toInt() and 0xFF
                        Operation.UpdateProperties(flags and 1 != 0, flags and 2 != 0, flags and 4 != 0)
                    }
                    else -> throw ProtocolException("Unknown boss event operation: $type")
                },
            )
        }
    }
}

/** parity: `Team.Visibility`, whose ids are what the wire carries. */
public enum class NameTagVisibility(
    public val serializedName: String,
) {
    ALWAYS("always"),
    NEVER("never"),
    HIDE_FOR_OTHER_TEAMS("hideForOtherTeams"),
    HIDE_FOR_OWN_TEAM("hideForOwnTeam"),
    ;

    public companion object {
        public fun byName(name: String): NameTagVisibility? = entries.firstOrNull { it.serializedName == name }
    }
}

/** parity: `Team.CollisionRule`. */
public enum class CollisionRule(
    public val serializedName: String,
) {
    ALWAYS("always"),
    NEVER("never"),
    PUSH_OTHER_TEAMS("pushOtherTeams"),
    PUSH_OWN_TEAM("pushOwnTeam"),
    ;

    public companion object {
        public fun byName(name: String): CollisionRule? = entries.firstOrNull { it.serializedName == name }
    }
}

/** parity: `TeamColor`, which is the sixteen chat colours in their own order. */
public enum class TeamColor(
    /** parity: `TeamColor`'s own packed RGB, which is what a waypoint icon carries. */
    public val rgb: Int,
) {
    BLACK(0x000000),
    DARK_BLUE(0x0000AA),
    DARK_GREEN(0x00AA00),
    DARK_AQUA(0x00AAAA),
    DARK_RED(0xAA0000),
    DARK_PURPLE(0xAA00AA),
    GOLD(0xFFAA00),
    GRAY(0xAAAAAA),
    DARK_GRAY(0x555555),
    BLUE(0x5555FF),
    GREEN(0x55FF55),
    AQUA(0x55FFFF),
    RED(0xFF5555),
    LIGHT_PURPLE(0xFF55FF),
    YELLOW(0xFFFF55),
    WHITE(0xFFFFFF),
    ;

    public companion object {
        public fun byName(name: String): TeamColor? = entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
    }
}

/**
 * Creates, changes, removes or populates a team. Mirrors `ClientboundSetPlayerTeamPacket`.
 *
 * The method decides what follows the name: parameters for add and change, a member list for add,
 * join and leave, and nothing at all for remove.
 */
public data class SetPlayerTeamPacket(
    val teamName: String,
    val method: Int,
    val parameters: Parameters? = null,
    val members: List<String> = emptyList(),
) {
    public data class Parameters(
        val displayName: Component,
        val prefix: Component = Component.empty(),
        val suffix: Component = Component.empty(),
        val nameTagVisibility: NameTagVisibility = NameTagVisibility.ALWAYS,
        val collisionRule: CollisionRule = CollisionRule.ALWAYS,
        val color: TeamColor? = null,
        val friendlyFire: Boolean = true,
        val seeFriendlyInvisibles: Boolean = true,
    ) {
        /** parity: `PlayerTeam.packOptions`: bit 0 friendly fire, bit 1 seeing friendly invisibles. */
        public val options: Byte
            get() {
                var flags = 0
                if (friendlyFire) flags = flags or 1
                if (seeFriendlyInvisibles) flags = flags or 2
                return flags.toByte()
            }

        public fun write(buffer: ByteBuffer) {
            buffer.writeAnonymousNbt(ComponentNbt.serialize(displayName))
            buffer.writeAnonymousNbt(ComponentNbt.serialize(prefix))
            buffer.writeAnonymousNbt(ComponentNbt.serialize(suffix))
            buffer.writeVarInt(nameTagVisibility.ordinal)
            buffer.writeVarInt(collisionRule.ordinal)
            buffer.writeBoolean(color != null)
            color?.let { buffer.writeVarInt(it.ordinal) }
            buffer.writeByte(options)
        }

        public companion object {
            public fun read(buffer: ByteBuffer): Parameters {
                val displayName = ComponentNbt.deserialize(buffer.readAnonymousNbt())
                val prefix = ComponentNbt.deserialize(buffer.readAnonymousNbt())
                val suffix = ComponentNbt.deserialize(buffer.readAnonymousNbt())
                val visibility = NameTagVisibility.entries[buffer.readVarInt()]
                val collision = CollisionRule.entries[buffer.readVarInt()]
                val color = if (buffer.readBoolean()) TeamColor.entries[buffer.readVarInt()] else null
                val options = buffer.readByte().toInt()
                return Parameters(
                    displayName,
                    prefix,
                    suffix,
                    visibility,
                    collision,
                    color,
                    friendlyFire = options and 1 != 0,
                    seeFriendlyInvisibles = options and 2 != 0,
                )
            }
        }
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(teamName)
        buffer.writeByte(method.toByte())
        if (method == METHOD_ADD || method == METHOD_CHANGE) {
            requireNotNull(parameters) { "method $method needs parameters" }.write(buffer)
        }
        if (method == METHOD_ADD || method == METHOD_JOIN || method == METHOD_LEAVE) {
            buffer.writeVarInt(members.size)
            for (member in members) buffer.writeProtocolString(member)
        }
    }

    public companion object {
        public const val ID: Int = 0x6d
        public const val METHOD_ADD: Int = 0
        public const val METHOD_REMOVE: Int = 1
        public const val METHOD_CHANGE: Int = 2
        public const val METHOD_JOIN: Int = 3
        public const val METHOD_LEAVE: Int = 4

        public fun read(buffer: ByteBuffer): SetPlayerTeamPacket {
            val name = buffer.readProtocolString()
            val method = buffer.readByte().toInt()
            val parameters =
                if (method == METHOD_ADD || method == METHOD_CHANGE) Parameters.read(buffer) else null
            val members =
                if (method == METHOD_ADD || method == METHOD_JOIN || method == METHOD_LEAVE) {
                    List(buffer.readVarInt()) { buffer.readProtocolString() }
                } else {
                    emptyList()
                }
            return SetPlayerTeamPacket(name, method, parameters, members)
        }
    }
}

/**
 * Clientbound, id 0x8c: opens a dialog screen. Mirrors `ClientboundShowDialogPacket`.
 *
 * The dialog travels as a holder: a registry reference is its id plus one, which is the form used
 * here, since a dialog worth showing is one the client was sent in the `minecraft:dialog` registry.
 */
public data class ShowDialogPacket(
    val dialogId: Int,
) {
    public fun write(buffer: ByteBuffer) {
        // parity: `Dialog.STREAM_CODEC` is `ByteBufCodecs.holder`, not `holderRegistry`, so here the
        // id really does carry the plus one: zero is reserved for an inline dialog.
        buffer.writeVarInt(dialogId + 1)
    }

    public companion object {
        public const val ID: Int = 0x8c

        public fun read(buffer: ByteBuffer): ShowDialogPacket = ShowDialogPacket(buffer.readVarInt() - 1)
    }
}

/** Clientbound, id 0x8b: closes whatever dialog is open. Mirrors `ClientboundClearDialogPacket`. */
public object ClearDialogPacket {
    public const val ID: Int = 0x8b

    /** Nothing follows the id, so writing one is writing its frame. */
    public fun write(buffer: ByteBuffer) {
        // Deliberately empty: the packet is its own id.
    }
}
