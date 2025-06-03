package org.basaltmc.wire.protocol

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtIO
import org.basaltmc.wire.nbt.NbtTag
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Protocol-level read/write helpers on top of `wire-io`'s [ByteBuffer]. String encoding is
 * plain UTF-8 (VarInt-prefixed byte length), unlike NBT's modified UTF-8 — confirmed against the
 * decompiled `net.minecraft.network.Utf8String`, not assumed.
 */
public fun ByteBuffer.writeVarInt(value: Int): Unit = VarInt.write(this, value)

public fun ByteBuffer.readVarInt(): Int = VarInt.read(this)

public fun ByteBuffer.writeVarLong(value: Long): Unit = VarLong.write(this, value)

public fun ByteBuffer.readVarLong(): Long = VarLong.read(this)

public fun ByteBuffer.writeProtocolString(
    value: String,
    maxLength: Int = 32767,
) {
    require(value.length <= maxLength) { "String too long: ${value.length} > $maxLength characters" }
    val encoded = value.encodeToByteArray()
    writeVarInt(encoded.size)
    writeBytes(encoded)
}

public fun ByteBuffer.readProtocolString(maxLength: Int = 32767): String {
    val length = readVarInt()
    if (length < 0) throw ProtocolException("Negative string length: $length")
    val maxEncodedLength = maxLength * 3
    if (length > maxEncodedLength) {
        throw ProtocolException("String buffer length $length exceeds maximum allowed $maxEncodedLength")
    }
    val result = readBytes(length).decodeToString()
    if (result.length > maxLength) {
        throw ProtocolException("String length ${result.length} exceeds maximum allowed $maxLength")
    }
    return result
}

public fun ByteBuffer.writeByteArray(bytes: ByteArray) {
    writeVarInt(bytes.size)
    writeBytes(bytes)
}

/** [maxSize] mirrors `FriendlyByteBuf.readByteArray`, which bounds against the buffer's own readable bytes by default. */
public fun ByteBuffer.readByteArray(maxSize: Int = readableBytes): ByteArray {
    val length = readVarInt()
    if (length < 0) throw ProtocolException("Negative byte array length: $length")
    if (length > maxSize) throw ProtocolException("Byte array length $length exceeds maximum allowed $maxSize")
    return readBytes(length)
}

public fun ByteBuffer.writeBoolean(value: Boolean): Unit = writeByte(if (value) 1 else 0)

public fun ByteBuffer.readBoolean(): Boolean = readByte() != 0.toByte()

@OptIn(ExperimentalUuidApi::class)
public fun ByteBuffer.writeUuid(uuid: Uuid) {
    uuid.toLongs { mostSignificantBits, leastSignificantBits ->
        writeLong(mostSignificantBits)
        writeLong(leastSignificantBits)
    }
}

@OptIn(ExperimentalUuidApi::class)
public fun ByteBuffer.readUuid(): Uuid = Uuid.fromLongs(readLong(), readLong())

/** "Network NBT": a root tag with no name field, used by chat, registry data, and disconnect reasons. */
public fun ByteBuffer.writeAnonymousNbt(tag: NbtTag) {
    writeBytes(NbtIO.writeAnonymous(tag))
}

public fun ByteBuffer.readAnonymousNbt(): NbtTag {
    val result = NbtIO.readAnonymous(toByteArray())
    skip(result.bytesConsumed)
    return result.tag
}
