package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.readProtocolString
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeProtocolString
import org.basaltmc.protocol.writeVarInt

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

    /** `minecraft:game_profile`: a player by name, online or not. */
    public data object GameProfile : ArgumentParser(7)

    /** `minecraft:block_pos`. */
    public data object BlockPosition : ArgumentParser(8)

    /** `minecraft:vec3`. */
    public data object Vec3 : ArgumentParser(10)

    /** `minecraft:message`: the rest of the line, chat-decorated. */
    public data object Message : ArgumentParser(20)

    /** `minecraft:gamemode`. */
    public data object GameMode : ArgumentParser(42)

    public companion object {
        /**
         * Reads back a parser Basalt itself writes. The full registry has 57 entries; only the ones
         * used by the commands this server declares are decodable here, because nothing ever
         * receives this packet: it exists so round-trip tests cover the properties too, not just
         * the framing. An id outside that set is a bug in the writer, not client input.
         */
        public fun read(buffer: ByteBuffer): ArgumentParser =
            when (val registryId = buffer.readVarInt()) {
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
                10 -> Vec3
                20 -> Message
                42 -> GameMode
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
