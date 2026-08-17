package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtEnd
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * Clientbound, id 0x06: one block entity's data, for a change that is not a change of block.
 *
 * Mirrors `ClientboundBlockEntityDataPacket`. A chunk carries its block entities with it, so this
 * is what keeps them up to date afterwards: the text written on a sign, a chest that was renamed.
 */
public data class BlockEntityDataPacket(
    val pos: BlockPos,
    val typeId: Int,
    val tag: NbtCompound?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(pos.pack())
        buffer.writeVarInt(typeId)
        buffer.writeAnonymousNbt(tag ?: NbtEnd)
    }

    public companion object {
        public const val ID: Int = 0x06

        public fun read(buffer: ByteBuffer): BlockEntityDataPacket {
            val pos = BlockPos.unpack(buffer.readLong())
            val typeId = buffer.readVarInt()
            return BlockEntityDataPacket(pos, typeId, buffer.readAnonymousNbt() as? NbtCompound)
        }
    }
}

/**
 * Clientbound, id 0x3c: open the sign editor. Mirrors `ClientboundOpenSignEditorPacket`.
 *
 * The client refuses to edit a sign it was not asked to edit, so placing one without this leaves a
 * blank sign and no way to write on it.
 */
public data class OpenSignEditorPacket(
    val pos: BlockPos,
    val isFrontText: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(pos.pack())
        buffer.writeBoolean(isFrontText)
    }

    public companion object {
        public const val ID: Int = 0x3c

        public fun read(buffer: ByteBuffer): OpenSignEditorPacket =
            OpenSignEditorPacket(BlockPos.unpack(buffer.readLong()), buffer.readBoolean())
    }
}

/**
 * Serverbound, id 0x3d: the four lines someone typed. Mirrors `ServerboundSignUpdatePacket`.
 *
 * The lines arrive as plain text, not as components: what the client sends is exactly what was
 * typed, and the server is what turns it into the text a sign shows.
 */
public data class SignUpdatePacket(
    val pos: BlockPos,
    val isFrontText: Boolean,
    val lines: List<String>,
) {
    init {
        require(lines.size == LINES) { "a sign has $LINES lines, not ${lines.size}" }
    }

    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(pos.pack())
        buffer.writeBoolean(isFrontText)
        for (line in lines) buffer.writeProtocolString(line, MAX_LINE_LENGTH)
    }

    public companion object {
        public const val ID: Int = 0x3d
        public const val LINES: Int = 4

        /** parity: `ServerboundSignUpdatePacket.MAX_STRING_LENGTH`, which is not the sign's own limit. */
        public const val MAX_LINE_LENGTH: Int = 384

        public fun read(buffer: ByteBuffer): SignUpdatePacket {
            val pos = BlockPos.unpack(buffer.readLong())
            val isFrontText = buffer.readBoolean()
            return SignUpdatePacket(pos, isFrontText, List(LINES) { buffer.readProtocolString(MAX_LINE_LENGTH) })
        }
    }
}
