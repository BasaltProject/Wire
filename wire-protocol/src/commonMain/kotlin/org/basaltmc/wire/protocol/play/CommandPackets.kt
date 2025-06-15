package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * Serverbound, id 0x07: a command the player typed, without its leading slash. Mirrors
 * `ServerboundChatCommandPacket`.
 */
public data class ChatCommandPacket(
    val command: String,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(command)
    }

    public companion object {
        public const val ID: Int = 0x07

        public fun read(buffer: ByteBuffer): ChatCommandPacket = ChatCommandPacket(buffer.readProtocolString(256))
    }
}

/**
 * Serverbound, id 0x08: a command carrying signed message arguments (`/msg`, `/say` and friends
 * when the client has a chat session). Mirrors `ServerboundChatCommandSignedPacket`.
 *
 * Only [command] is decoded. The signature block that follows it (timestamp, salt, per-argument
 * signatures, last-seen acknowledgements) is left unread: nothing verifies signatures yet, and the
 * frame is discarded whole afterwards, so reading past the command would only be ceremony. Same
 * documented gap as [SystemChatPacket]'s.
 */
public data class SignedChatCommandPacket(
    val command: String,
) {
    public companion object {
        public const val ID: Int = 0x08

        public fun read(buffer: ByteBuffer): SignedChatCommandPacket =
            SignedChatCommandPacket(buffer.readProtocolString(256))
    }
}

/**
 * One argument parser in the command tree, as the client needs to instantiate it.
 *
 * [registryId] is the entry's index in vanilla's `COMMAND_ARGUMENT_TYPE` registry, which is
 * registration order in `ArgumentTypeInfos.bootstrap` (`brigadier:bool` is 0). It is a protocol
 * fact, not content: the client resolves the parser by that number alone.
 */
public sealed class ArgumentParser(
    public val registryId: Int,
) {
    /** Extra parser configuration, after the registry id. Empty for vanilla's `SingletonArgumentInfo` types. */
    public open fun writeProperties(buffer: ByteBuffer) {}

    /** `brigadier:string`. [Kind] is `StringArgumentType.StringType`'s own order. */
    public data class BrigadierString(
        val kind: Kind,
    ) : ArgumentParser(5) {
        public enum class Kind {
            SINGLE_WORD,
            QUOTABLE_PHRASE,
            GREEDY_PHRASE,
        }

        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeVarInt(kind.ordinal)
        }
    }

    /** `brigadier:integer`, with the same "omit the bound when it is the type's own extreme" encoding vanilla uses. */
    public data class BrigadierInteger(
        val min: Int = Int.MIN_VALUE,
        val max: Int = Int.MAX_VALUE,
    ) : ArgumentParser(3) {
        override fun writeProperties(buffer: ByteBuffer) {
            val hasMin = min != Int.MIN_VALUE
            val hasMax = max != Int.MAX_VALUE
            var flags = 0
            if (hasMin) flags = flags or 1
            if (hasMax) flags = flags or 2
            buffer.writeByte(flags.toByte())
            if (hasMin) buffer.writeInt(min)
            if (hasMax) buffer.writeInt(max)
        }
    }

    /** `minecraft:entity`: a selector. The two flags decide how many entities and of what kind may match. */
    public data class Entity(
        val single: Boolean,
        val playersOnly: Boolean,
    ) : ArgumentParser(6) {
        override fun writeProperties(buffer: ByteBuffer) {
            var flags = 0
            if (single) flags = flags or 1
            if (playersOnly) flags = flags or 2
            buffer.writeByte(flags.toByte())
        }
    }

    /** `brigadier:bool`. */
    public data object BrigadierBool : ArgumentParser(0)

    /** `brigadier:double`, with vanilla's "omit an unbounded end" encoding. */
    public data class BrigadierDouble(
        val min: Double = -Double.MAX_VALUE,
        val max: Double = Double.MAX_VALUE,
    ) : ArgumentParser(2) {
        override fun writeProperties(buffer: ByteBuffer) {
            val hasMin = min != -Double.MAX_VALUE
            val hasMax = max != Double.MAX_VALUE
            var flags = 0
            if (hasMin) flags = flags or 1
            if (hasMax) flags = flags or 2
            buffer.writeByte(flags.toByte())
            if (hasMin) buffer.writeDouble(min)
            if (hasMax) buffer.writeDouble(max)
        }
    }

    /** `brigadier:float`, with the same "omit an unbounded end" encoding as [BrigadierDouble]. */
    public data class BrigadierFloat(
        val min: Float = -Float.MAX_VALUE,
        val max: Float = Float.MAX_VALUE,
    ) : ArgumentParser(1) {
        override fun writeProperties(buffer: ByteBuffer) {
            val hasMin = min != -Float.MAX_VALUE
            val hasMax = max != Float.MAX_VALUE
            var flags = 0
            if (hasMin) flags = flags or 1
            if (hasMax) flags = flags or 2
            buffer.writeByte(flags.toByte())
            if (hasMin) buffer.writeFloat(min)
            if (hasMax) buffer.writeFloat(max)
        }
    }

    /** `minecraft:game_profile`: a player by name, online or not. */
    public data object GameProfile : ArgumentParser(7)

    /** `minecraft:block_pos`. */
    public data object BlockPosition : ArgumentParser(8)

    /** `minecraft:column_pos`: an `x z` pair of block coordinates. */
    public data object ColumnPosition : ArgumentParser(9)

    /** `minecraft:vec3`. */
    public data object Vec3 : ArgumentParser(10)

    /** `minecraft:vec2`. */
    public data object Vec2 : ArgumentParser(11)

    /** `minecraft:block_state`: a block plus its properties, as `/setblock` takes one. */
    public data object BlockState : ArgumentParser(12)

    /** `minecraft:block_predicate`: a block, a block tag, or either with properties. */
    public data object BlockPredicate : ArgumentParser(13)

    /** `minecraft:item_stack`: an item plus its components. */
    public data object ItemStack : ArgumentParser(14)

    /** `minecraft:item_predicate`: an item or item tag to match against. */
    public data object ItemPredicate : ArgumentParser(15)

    /** `minecraft:team_color`: one of the sixteen chat colours a team can be dyed. */
    public data object TeamColor : ArgumentParser(16)

    /** `minecraft:component`: a text component, as JSON or SNBT. */
    public data object TextComponent : ArgumentParser(18)

    /** `minecraft:style`: a component style without its text. */
    public data object Style : ArgumentParser(19)

    /** `minecraft:message`: the rest of the line, chat-decorated. */
    public data object Message : ArgumentParser(20)

    /** `minecraft:nbt_compound_tag`: an SNBT compound. */
    public data object NbtCompound : ArgumentParser(21)

    /** `minecraft:nbt_tag`: any SNBT value, compound or not. */
    public data object NbtTag : ArgumentParser(22)

    /** `minecraft:nbt_path`: a path into a compound, as `/data` takes one. */
    public data object NbtPath : ArgumentParser(23)

    /** `minecraft:objective`: a scoreboard objective's name. */
    public data object Objective : ArgumentParser(24)

    /** `minecraft:objective_criteria`: what an objective counts. */
    public data object ObjectiveCriteria : ArgumentParser(25)

    /** `minecraft:operation`: the `+=`, `<`, `><` family `/scoreboard players operation` takes. */
    public data object Operation : ArgumentParser(26)

    /** `minecraft:particle`. */
    public data object Particle : ArgumentParser(27)

    /** `minecraft:angle`: a single yaw, absolute or relative. */
    public data object Angle : ArgumentParser(28)

    /** `minecraft:rotation`: a `yaw pitch` pair. */
    public data object Rotation : ArgumentParser(29)

    /** `minecraft:scoreboard_slot`: where an objective is displayed. */
    public data object ScoreboardSlot : ArgumentParser(30)

    /** `minecraft:score_holder`: a selector or a bare name, which is how fake players are scored. */
    public data class ScoreHolder(
        val multiple: Boolean,
    ) : ArgumentParser(31) {
        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeByte(if (multiple) 1 else 0)
        }
    }

    /** `minecraft:swizzle`: any combination of `x`, `y` and `z`. */
    public data object Swizzle : ArgumentParser(32)

    /** `minecraft:team`: a team's name. */
    public data object Team : ArgumentParser(33)

    /** `minecraft:item_slot`: one slot name, as `/item replace` takes. */
    public data object ItemSlot : ArgumentParser(34)

    /** `minecraft:item_slots`: a slot name that may cover several slots. */
    public data object ItemSlots : ArgumentParser(35)

    /** `minecraft:resource_location`: a namespaced id, unchecked against any registry. */
    public data object ResourceLocation : ArgumentParser(36)

    /** `minecraft:entity_anchor`: `eyes` or `feet`. */
    public data object EntityAnchor : ArgumentParser(38)

    /** `minecraft:int_range`: `3`, `3..`, `..7` or `3..7`. */
    public data object IntRange : ArgumentParser(39)

    /** `minecraft:float_range`. */
    public data object FloatRange : ArgumentParser(40)

    /** `minecraft:dimension`: a level's id, resolved by the client against its own dimension list. */
    public data object Dimension : ArgumentParser(41)

    /** `minecraft:gamemode`. */
    public data object GameMode : ArgumentParser(42)

    /** `minecraft:time`: a tick count with an optional `d`/`s`/`t` unit, never below [min]. */
    public data class Time(
        val min: Int = 0,
    ) : ArgumentParser(43) {
        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeInt(min)
        }
    }

    /** `minecraft:resource`: an entry of [registry], which the client checks it knows. */
    public data class Resource(
        val registry: String,
    ) : ArgumentParser(46) {
        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeProtocolString(registry)
        }
    }

    /** `minecraft:resource_key`: like [Resource], but the entry need not be loaded. */
    public data class ResourceKey(
        val registry: String,
    ) : ArgumentParser(47) {
        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeProtocolString(registry)
        }
    }

    /** `minecraft:function`: a function or `#tag` of functions. */
    public data object Function : ArgumentParser(37)

    /** `minecraft:hex_color`: `#rrggbb`, which is what a waypoint's own colour takes. */
    public data object HexColor : ArgumentParser(17)

    /** `minecraft:resource_or_tag`: an entry of [registry] or a `#tag` of them. */
    public data class ResourceOrTag(
        val registry: String,
    ) : ArgumentParser(44) {
        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeProtocolString(registry)
        }
    }

    /** `minecraft:resource_or_tag_key`: like [ResourceOrTag], but the entry need not be loaded. */
    public data class ResourceOrTagKey(
        val registry: String,
    ) : ArgumentParser(45) {
        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeProtocolString(registry)
        }
    }

    /** `minecraft:resource_selector`: one or more entries of [registry], by name or by pattern. */
    public data class ResourceSelector(
        val registry: String,
    ) : ArgumentParser(48) {
        override fun writeProperties(buffer: ByteBuffer) {
            buffer.writeProtocolString(registry)
        }
    }

    /** `minecraft:template_mirror`: the three mirror modes `/place template` takes. */
    public data object TemplateMirror : ArgumentParser(49)

    /** `minecraft:template_rotation`: the four rotations `/place template` takes. */
    public data object TemplateRotation : ArgumentParser(50)

    /** `minecraft:heightmap`: one of the heightmap types `/execute positioned over` accepts. */
    public data object Heightmap : ArgumentParser(51)

    /** `minecraft:loot_table`: a loot table id or an inline table. */
    public data object LootTable : ArgumentParser(52)

    /** `minecraft:loot_predicate`: a predicate id or an inline predicate. */
    public data object LootPredicate : ArgumentParser(53)

    /** `minecraft:dialog`: a dialog id or an inline dialog. */
    public data object Dialog : ArgumentParser(55)

    /** `minecraft:uuid`. */
    public data object Uuid : ArgumentParser(56)

    public companion object {
        /**
         * Reads back a parser Basalt itself writes. The full registry has 57 entries; only the ones
         * used by the commands this server declares are decodable here, because nothing ever
         * receives this packet: it exists so round-trip tests cover the properties too, not just
         * the framing. An id outside that set is a bug in the writer, not client input.
         */
        public fun read(buffer: ByteBuffer): ArgumentParser =
            when (val registryId = buffer.readVarInt()) {
                0 -> BrigadierBool
                1 -> {
                    val flags = buffer.readByte().toInt()
                    BrigadierFloat(
                        min = if (flags and 1 != 0) buffer.readFloat() else -Float.MAX_VALUE,
                        max = if (flags and 2 != 0) buffer.readFloat() else Float.MAX_VALUE,
                    )
                }
                2 -> {
                    val flags = buffer.readByte().toInt()
                    BrigadierDouble(
                        min = if (flags and 1 != 0) buffer.readDouble() else -Double.MAX_VALUE,
                        max = if (flags and 2 != 0) buffer.readDouble() else Double.MAX_VALUE,
                    )
                }
                3 -> {
                    val flags = buffer.readByte().toInt()
                    BrigadierInteger(
                        min = if (flags and 1 != 0) buffer.readInt() else Int.MIN_VALUE,
                        max = if (flags and 2 != 0) buffer.readInt() else Int.MAX_VALUE,
                    )
                }
                5 -> BrigadierString(BrigadierString.Kind.entries[buffer.readVarInt()])
                6 -> {
                    val flags = buffer.readByte().toInt()
                    Entity(single = flags and 1 != 0, playersOnly = flags and 2 != 0)
                }
                7 -> GameProfile
                8 -> BlockPosition
                9 -> ColumnPosition
                10 -> Vec3
                11 -> Vec2
                12 -> BlockState
                13 -> BlockPredicate
                14 -> ItemStack
                15 -> ItemPredicate
                16 -> TeamColor
                17 -> HexColor
                18 -> TextComponent
                19 -> Style
                20 -> Message
                21 -> NbtCompound
                22 -> NbtTag
                23 -> NbtPath
                24 -> Objective
                25 -> ObjectiveCriteria
                26 -> Operation
                27 -> Particle
                28 -> Angle
                29 -> Rotation
                30 -> ScoreboardSlot
                31 -> ScoreHolder(multiple = buffer.readByte().toInt() and 1 != 0)
                32 -> Swizzle
                33 -> Team
                34 -> ItemSlot
                35 -> ItemSlots
                36 -> ResourceLocation
                37 -> Function
                38 -> EntityAnchor
                39 -> IntRange
                40 -> FloatRange
                41 -> Dimension
                42 -> GameMode
                43 -> Time(min = buffer.readInt())
                44 -> ResourceOrTag(buffer.readProtocolString())
                45 -> ResourceOrTagKey(buffer.readProtocolString())
                46 -> Resource(buffer.readProtocolString())
                47 -> ResourceKey(buffer.readProtocolString())
                48 -> ResourceSelector(buffer.readProtocolString())
                49 -> TemplateMirror
                50 -> TemplateRotation
                51 -> Heightmap
                52 -> LootTable
                53 -> LootPredicate
                55 -> Dialog
                56 -> Uuid
                else -> error("no argument parser registered for id $registryId")
            }
    }
}

/**
 * One node of the command tree on the wire, already flattened to indices. Mirrors
 * `ClientboundCommandsPacket.Entry`: the flag byte carries both the node kind (low two bits) and
 * the executable/redirect/custom-suggestion bits.
 */
public data class CommandNodeData(
    val kind: Kind,
    val executable: Boolean,
    val children: IntArray,
    val redirect: Int? = null,
    val name: String? = null,
    val parser: ArgumentParser? = null,
    val suggestionId: String? = null,
) {
    public enum class Kind {
        ROOT,
        LITERAL,
        ARGUMENT,
    }

    public fun write(buffer: ByteBuffer) {
        var flags = kind.ordinal
        if (executable) flags = flags or FLAG_EXECUTABLE
        if (redirect != null) flags = flags or FLAG_REDIRECT
        if (suggestionId != null) flags = flags or FLAG_CUSTOM_SUGGESTIONS
        buffer.writeByte(flags.toByte())
        buffer.writeVarInt(children.size)
        for (child in children) buffer.writeVarInt(child)
        if (redirect != null) buffer.writeVarInt(redirect)
        if (kind != Kind.ROOT) buffer.writeProtocolString(requireNotNull(name) { "a non-root node needs a name" })
        if (kind == Kind.ARGUMENT) {
            val argumentParser = requireNotNull(parser) { "an argument node needs a parser" }
            buffer.writeVarInt(argumentParser.registryId)
            argumentParser.writeProperties(buffer)
            if (suggestionId != null) buffer.writeProtocolString(suggestionId)
        }
    }

    override fun equals(other: Any?): Boolean =
        other is CommandNodeData &&
            kind == other.kind &&
            executable == other.executable &&
            children.contentEquals(other.children) &&
            redirect == other.redirect &&
            name == other.name &&
            parser == other.parser &&
            suggestionId == other.suggestionId

    override fun hashCode(): Int {
        var result = kind.hashCode()
        result = 31 * result + executable.hashCode()
        result = 31 * result + children.contentHashCode()
        result = 31 * result + (redirect ?: 0)
        result = 31 * result + (name?.hashCode() ?: 0)
        result = 31 * result + (parser?.hashCode() ?: 0)
        result = 31 * result + (suggestionId?.hashCode() ?: 0)
        return result
    }

    public companion object {
        private const val MASK_KIND = 3
        private const val FLAG_EXECUTABLE = 4
        private const val FLAG_REDIRECT = 8
        private const val FLAG_CUSTOM_SUGGESTIONS = 16

        public fun read(buffer: ByteBuffer): CommandNodeData {
            val flags = buffer.readByte().toInt()
            val kind = Kind.entries[flags and MASK_KIND]
            val children = IntArray(buffer.readVarInt()) { buffer.readVarInt() }
            val redirect = if (flags and FLAG_REDIRECT != 0) buffer.readVarInt() else null
            val name = if (kind != Kind.ROOT) buffer.readProtocolString() else null
            val parser = if (kind == Kind.ARGUMENT) ArgumentParser.read(buffer) else null
            val suggestionId =
                if (kind == Kind.ARGUMENT &&
                    flags and FLAG_CUSTOM_SUGGESTIONS != 0
                ) {
                    buffer.readProtocolString()
                } else {
                    null
                }
            return CommandNodeData(
                kind = kind,
                executable = flags and FLAG_EXECUTABLE != 0,
                children = children,
                redirect = redirect,
                name = name,
                parser = parser,
                suggestionId = suggestionId,
            )
        }
    }
}

/**
 * Clientbound, id 0x10: the command tree, sent once the player is in Play. Mirrors
 * `ClientboundCommandsPacket`.
 *
 * The client parses commands locally against this tree before sending anything, so it is what
 * makes tab completion, the red "unknown command" highlight and argument colouring work. Vanilla
 * sends a per-player tree, already filtered to the commands that player may use.
 */
public data class CommandsPacket(
    val nodes: List<CommandNodeData>,
    val rootIndex: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(nodes.size)
        for (node in nodes) node.write(buffer)
        buffer.writeVarInt(rootIndex)
    }

    public companion object {
        public const val ID: Int = 0x10

        public fun read(buffer: ByteBuffer): CommandsPacket {
            val nodes = List(buffer.readVarInt()) { CommandNodeData.read(buffer) }
            return CommandsPacket(nodes, buffer.readVarInt())
        }
    }
}
