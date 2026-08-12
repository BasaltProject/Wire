package org.basaltmc.wire.nbt

/** Base type for every NBT value. See `NbtType` for the 13 concrete tag kinds. */
public sealed class NbtTag {
    public abstract val type: NbtType
}

public data object NbtEnd : NbtTag() {
    override val type: NbtType = NbtType.END
}

public data class NbtByte(
    val value: Byte,
) : NbtTag() {
    override val type: NbtType = NbtType.BYTE

    public companion object {
        public val TRUE: NbtByte = NbtByte(1)
        public val FALSE: NbtByte = NbtByte(0)

        public fun of(value: Boolean): NbtByte = if (value) TRUE else FALSE
    }
}

public data class NbtShort(
    val value: Short,
) : NbtTag() {
    override val type: NbtType = NbtType.SHORT
}

public data class NbtInt(
    val value: Int,
) : NbtTag() {
    override val type: NbtType = NbtType.INT
}

public data class NbtLong(
    val value: Long,
) : NbtTag() {
    override val type: NbtType = NbtType.LONG
}

public data class NbtFloat(
    val value: Float,
) : NbtTag() {
    override val type: NbtType = NbtType.FLOAT
}

public data class NbtDouble(
    val value: Double,
) : NbtTag() {
    override val type: NbtType = NbtType.DOUBLE
}

public class NbtByteArray(
    public val value: ByteArray,
) : NbtTag() {
    override val type: NbtType = NbtType.BYTE_ARRAY

    override fun equals(other: Any?): Boolean = other is NbtByteArray && value.contentEquals(other.value)

    override fun hashCode(): Int = value.contentHashCode()

    override fun toString(): String = "NbtByteArray(size=${value.size})"
}

public data class NbtString(
    val value: String,
) : NbtTag() {
    override val type: NbtType = NbtType.STRING
}

public class NbtList(
    public val elementType: NbtType,
    elements: List<NbtTag> = emptyList(),
) : NbtTag(),
    Iterable<NbtTag> {
    override val type: NbtType = NbtType.LIST

    private val backing: MutableList<NbtTag> = elements.toMutableList()

    init {
        require(elementType == NbtType.END || backing.all { it.type == elementType }) {
            "All elements of an NbtList must have type $elementType"
        }
    }

    public val size: Int get() = backing.size

    public operator fun get(index: Int): NbtTag = backing[index]

    override fun iterator(): Iterator<NbtTag> = backing.iterator()

    public fun toList(): List<NbtTag> = backing.toList()

    override fun equals(other: Any?): Boolean =
        other is NbtList && elementType == other.elementType && backing == other.backing

    override fun hashCode(): Int = 31 * elementType.hashCode() + backing.hashCode()

    override fun toString(): String = "NbtList(elementType=$elementType, size=${backing.size})"
}

public class NbtCompound(
    entries: Map<String, NbtTag> = emptyMap(),
) : NbtTag(),
    Iterable<Map.Entry<String, NbtTag>> {
    override val type: NbtType = NbtType.COMPOUND

    private val backing: LinkedHashMap<String, NbtTag> = LinkedHashMap(entries)

    public val size: Int get() = backing.size

    public val keys: Set<String> get() = backing.keys

    public operator fun contains(key: String): Boolean = backing.containsKey(key)

    public operator fun get(key: String): NbtTag? = backing[key]

    public operator fun set(
        key: String,
        tag: NbtTag,
    ) {
        backing[key] = tag
    }

    public fun remove(key: String): NbtTag? = backing.remove(key)

    public fun putByte(
        key: String,
        value: Byte,
    ): Unit = set(key, NbtByte(value))

    public fun putBoolean(
        key: String,
        value: Boolean,
    ): Unit = set(key, NbtByte.of(value))

    public fun putShort(
        key: String,
        value: Short,
    ): Unit = set(key, NbtShort(value))

    public fun putInt(
        key: String,
        value: Int,
    ): Unit = set(key, NbtInt(value))

    public fun putLong(
        key: String,
        value: Long,
    ): Unit = set(key, NbtLong(value))

    public fun putFloat(
        key: String,
        value: Float,
    ): Unit = set(key, NbtFloat(value))

    public fun putDouble(
        key: String,
        value: Double,
    ): Unit = set(key, NbtDouble(value))

    public fun putString(
        key: String,
        value: String,
    ): Unit = set(key, NbtString(value))

    public fun putByteArray(
        key: String,
        value: ByteArray,
    ): Unit = set(key, NbtByteArray(value))

    public fun putIntArray(
        key: String,
        value: IntArray,
    ): Unit = set(key, NbtIntArray(value))

    public fun putLongArray(
        key: String,
        value: LongArray,
    ): Unit = set(key, NbtLongArray(value))

    public fun putCompound(
        key: String,
        value: NbtCompound,
    ): Unit = set(key, value)

    public fun getByte(
        key: String,
        default: Byte = 0,
    ): Byte = (backing[key] as? NbtByte)?.value ?: default

    public fun getBoolean(
        key: String,
        default: Boolean = false,
    ): Boolean = (backing[key] as? NbtByte)?.let { it.value != 0.toByte() } ?: default

    public fun getShort(
        key: String,
        default: Short = 0,
    ): Short = (backing[key] as? NbtShort)?.value ?: default

    public fun getInt(
        key: String,
        default: Int = 0,
    ): Int = (backing[key] as? NbtInt)?.value ?: default

    public fun getLong(
        key: String,
        default: Long = 0L,
    ): Long = (backing[key] as? NbtLong)?.value ?: default

    public fun getFloat(
        key: String,
        default: Float = 0f,
    ): Float = (backing[key] as? NbtFloat)?.value ?: default

    public fun getDouble(
        key: String,
        default: Double = 0.0,
    ): Double = (backing[key] as? NbtDouble)?.value ?: default

    public fun getString(
        key: String,
        default: String = "",
    ): String = (backing[key] as? NbtString)?.value ?: default

    public fun getByteArray(key: String): ByteArray = (backing[key] as? NbtByteArray)?.value ?: ByteArray(0)

    public fun getIntArray(key: String): IntArray = (backing[key] as? NbtIntArray)?.value ?: IntArray(0)

    public fun getLongArray(key: String): LongArray = (backing[key] as? NbtLongArray)?.value ?: LongArray(0)

    public fun getCompound(key: String): NbtCompound = (backing[key] as? NbtCompound) ?: NbtCompound()

    public fun getList(key: String): NbtList? = backing[key] as? NbtList

    override fun iterator(): Iterator<Map.Entry<String, NbtTag>> = backing.entries.iterator()

    override fun equals(other: Any?): Boolean = other is NbtCompound && backing == other.backing

    override fun hashCode(): Int = backing.hashCode()

    override fun toString(): String = "NbtCompound(size=${backing.size})"
}

public class NbtIntArray(
    public val value: IntArray,
) : NbtTag() {
    override val type: NbtType = NbtType.INT_ARRAY

    override fun equals(other: Any?): Boolean = other is NbtIntArray && value.contentEquals(other.value)

    override fun hashCode(): Int = value.contentHashCode()

    override fun toString(): String = "NbtIntArray(size=${value.size})"
}

public class NbtLongArray(
    public val value: LongArray,
) : NbtTag() {
    override val type: NbtType = NbtType.LONG_ARRAY

    override fun equals(other: Any?): Boolean = other is NbtLongArray && value.contentEquals(other.value)

    override fun hashCode(): Int = value.contentHashCode()

    override fun toString(): String = "NbtLongArray(size=${value.size})"
}

/** Returns an independent tree, matching vanilla NBT's copy semantics for mutable containers and arrays. */
public fun NbtTag.deepCopy(): NbtTag =
    when (this) {
        is NbtCompound -> deepCopy()
        is NbtList -> NbtList(elementType, map(NbtTag::deepCopy))
        is NbtByteArray -> NbtByteArray(value.copyOf())
        is NbtIntArray -> NbtIntArray(value.copyOf())
        is NbtLongArray -> NbtLongArray(value.copyOf())
        else -> this
    }

/** Type-preserving overload for the most common mutable NBT root. */
public fun NbtCompound.deepCopy(): NbtCompound = NbtCompound(associate { (name, value) -> name to value.deepCopy() })
