package org.basaltmc.wire.nbt

/** A root NBT tag together with the name it was stored under (usually empty in modern usage). */
public data class NamedTag(
    val name: String,
    val tag: NbtTag,
)

/** Result of [NbtIO.readAnonymous]: the parsed tag and how many bytes of the input it consumed. */
public data class AnonymousNbtResult(
    val tag: NbtTag,
    val bytesConsumed: Int,
)

/**
 * Binary NBT reader/writer. Deliberately has no notion of gzip/zlib: compression is a transport
 * concern, layered on top by callers that already have a compressor (`basalt-io`'s zlib
 * cinterop, once it exists) via [writeCompressed]/[readCompressed]. This keeps `basalt-nbt`
 * dependency-free.
 */
public object NbtIO {
    public fun write(
        root: NbtTag,
        name: String = "",
    ): ByteArray {
        val writer = GrowableByteWriter()
        writeNamedTag(writer, name, root)
        return writer.toByteArray()
    }

    public fun read(bytes: ByteArray): NamedTag {
        val reader = ArrayByteReader(bytes)
        return readNamedTag(reader)
    }

    public fun writeCompressed(
        root: NbtTag,
        name: String = "",
        compress: (ByteArray) -> ByteArray,
    ): ByteArray = compress(write(root, name))

    public fun readCompressed(
        bytes: ByteArray,
        decompress: (ByteArray) -> ByteArray,
    ): NamedTag = read(decompress(bytes))

    /** The root tag has no name field, unlike [write]/[read]: the shape modern network packets use. */
    public fun writeAnonymous(root: NbtTag): ByteArray {
        val writer = GrowableByteWriter()
        writer.writeByte(root.type.id)
        if (root.type != NbtType.END) writePayload(writer, root)
        return writer.toByteArray()
    }

    /** Reads one anonymous root tag starting at the front of [bytes], reporting how many bytes it consumed. */
    public fun readAnonymous(bytes: ByteArray): AnonymousNbtResult {
        val reader = ArrayByteReader(bytes)
        val type = NbtType.fromId(reader.readByte())
        val tag = if (type == NbtType.END) NbtEnd else readPayload(reader, type)
        return AnonymousNbtResult(tag, reader.position)
    }

    private fun writeNamedTag(
        writer: ByteWriter,
        name: String,
        tag: NbtTag,
    ) {
        writer.writeByte(tag.type.id)
        if (tag.type != NbtType.END) {
            writeString(writer, name)
            writePayload(writer, tag)
        }
    }

    private fun readNamedTag(reader: ByteReader): NamedTag {
        val type = NbtType.fromId(reader.readByte())
        if (type == NbtType.END) return NamedTag("", NbtEnd)
        val name = readString(reader)
        val tag = readPayload(reader, type)
        return NamedTag(name, tag)
    }

    private fun writeString(
        writer: ByteWriter,
        value: String,
    ) {
        val length = ModifiedUtf8.encodedLength(value)
        require(length <= 0xFFFF) { "NBT string too long: $length encoded bytes" }
        writer.writeShort(length.toShort())
        ModifiedUtf8.encode(value, writer)
    }

    private fun readString(reader: ByteReader): String {
        val length = reader.readShort().toInt() and 0xFFFF
        return ModifiedUtf8.decode(reader, length)
    }

    private fun writePayload(
        writer: ByteWriter,
        tag: NbtTag,
    ) {
        when (tag) {
            is NbtEnd -> Unit
            is NbtByte -> writer.writeByte(tag.value)
            is NbtShort -> writer.writeShort(tag.value)
            is NbtInt -> writer.writeInt(tag.value)
            is NbtLong -> writer.writeLong(tag.value)
            is NbtFloat -> writer.writeFloat(tag.value)
            is NbtDouble -> writer.writeDouble(tag.value)
            is NbtByteArray -> {
                writer.writeInt(tag.value.size)
                for (b in tag.value) writer.writeByte(b)
            }
            is NbtString -> writeString(writer, tag.value)
            is NbtList -> {
                writer.writeByte(tag.elementType.id)
                writer.writeInt(tag.size)
                for (element in tag) writePayload(writer, element)
            }
            is NbtCompound -> {
                for ((key, value) in tag) writeNamedTag(writer, key, value)
                writer.writeByte(NbtType.END.id)
            }
            is NbtIntArray -> {
                writer.writeInt(tag.value.size)
                for (i in tag.value) writer.writeInt(i)
            }
            is NbtLongArray -> {
                writer.writeInt(tag.value.size)
                for (l in tag.value) writer.writeLong(l)
            }
        }
    }

    private fun readPayload(
        reader: ByteReader,
        type: NbtType,
    ): NbtTag =
        when (type) {
            NbtType.END -> NbtEnd
            NbtType.BYTE -> NbtByte(reader.readByte())
            NbtType.SHORT -> NbtShort(reader.readShort())
            NbtType.INT -> NbtInt(reader.readInt())
            NbtType.LONG -> NbtLong(reader.readLong())
            NbtType.FLOAT -> NbtFloat(reader.readFloat())
            NbtType.DOUBLE -> NbtDouble(reader.readDouble())
            NbtType.BYTE_ARRAY -> {
                val size = requirePositiveArraySize(reader.readInt())
                NbtByteArray(ByteArray(size) { reader.readByte() })
            }
            NbtType.STRING -> NbtString(readString(reader))
            NbtType.LIST -> {
                val elementType = NbtType.fromId(reader.readByte())
                val size = requirePositiveArraySize(reader.readInt())
                val elements = List(size) { readPayload(reader, elementType) }
                NbtList(elementType, elements)
            }
            NbtType.COMPOUND -> {
                val compound = NbtCompound()
                while (true) {
                    val entryType = NbtType.fromId(reader.readByte())
                    if (entryType == NbtType.END) break
                    val key = readString(reader)
                    compound[key] = readPayload(reader, entryType)
                }
                compound
            }
            NbtType.INT_ARRAY -> {
                val size = requirePositiveArraySize(reader.readInt())
                NbtIntArray(IntArray(size) { reader.readInt() })
            }
            NbtType.LONG_ARRAY -> {
                val size = requirePositiveArraySize(reader.readInt())
                NbtLongArray(LongArray(size) { reader.readLong() })
            }
        }

    private fun requirePositiveArraySize(size: Int): Int {
        if (size < 0) throw NbtFormatException("Negative NBT array/list size: $size")
        return size
    }
}
