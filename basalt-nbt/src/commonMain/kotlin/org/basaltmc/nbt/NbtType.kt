package org.basaltmc.nbt

/** The 13 NBT tag type IDs, unchanged since Minecraft Beta 1.3. */
public enum class NbtType(
    public val id: Byte,
) {
    END(0),
    BYTE(1),
    SHORT(2),
    INT(3),
    LONG(4),
    FLOAT(5),
    DOUBLE(6),
    BYTE_ARRAY(7),
    STRING(8),
    LIST(9),
    COMPOUND(10),
    INT_ARRAY(11),
    LONG_ARRAY(12),
    ;

    public companion object {
        public fun fromId(id: Byte): NbtType =
            entries.find { it.id == id } ?: throw NbtFormatException("Unknown NBT tag id: $id")
    }
}

/** Thrown when binary or SNBT input does not describe a well-formed NBT document. */
public class NbtFormatException(
    message: String,
) : Exception(message)
