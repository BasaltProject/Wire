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

/**
 * The line being typed, asked while it is typed. parity: `ServerboundCommandSuggestionPacket`.
 *
 * [id] comes back untouched in the answer: the client drops a reply carrying an older id, so a slow
 * answer never lands on top of a newer keystroke.
 */
public data class CommandSuggestionRequestPacket(
    val id: Int,
    val command: String,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(id)
        buffer.writeProtocolString(command, MAX_COMMAND_LENGTH)
    }

    public companion object {
        public const val ID: Int = 0x0f

        /** parity: vanilla's own cap on the line it will ask about. */
        public const val MAX_COMMAND_LENGTH: Int = 32500

        public fun read(buffer: ByteBuffer): CommandSuggestionRequestPacket =
            CommandSuggestionRequestPacket(buffer.readVarInt(), buffer.readProtocolString(MAX_COMMAND_LENGTH))
    }
}

/** One completion: the text that replaces the range, and the grey note the client draws beside it. */
public data class CommandSuggestion(
    val text: String,
    val tooltip: Component? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(text)
        buffer.writeBoolean(tooltip != null)
        tooltip?.let { buffer.writeAnonymousNbt(ComponentNbt.serialize(it)) }
    }

    public companion object {
        public fun read(buffer: ByteBuffer): CommandSuggestion =
            CommandSuggestion(
                text = buffer.readProtocolString(),
                tooltip = if (buffer.readBoolean()) ComponentNbt.deserialize(buffer.readAnonymousNbt()) else null,
            )
    }
}

/**
 * The completions for a request. parity: `ClientboundCommandSuggestionsPacket`.
 *
 * [start] and [length] are the slice of the line, in characters, that the pick replaces.
 */
public data class CommandSuggestionsPacket(
    val id: Int,
    val start: Int,
    val length: Int,
    val suggestions: List<CommandSuggestion>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(id)
        buffer.writeVarInt(start)
        buffer.writeVarInt(length)
        buffer.writeVarInt(suggestions.size)
        for (suggestion in suggestions) suggestion.write(buffer)
    }

    public companion object {
        public const val ID: Int = 0x0f

        public fun read(buffer: ByteBuffer): CommandSuggestionsPacket =
            CommandSuggestionsPacket(
                id = buffer.readVarInt(),
                start = buffer.readVarInt(),
                length = buffer.readVarInt(),
                suggestions = List(buffer.readVarInt()) { CommandSuggestion.read(buffer) },
            )
    }
}
