package org.basaltmc.wire.common.collections

/**
 * Open-addressing primitive long-to-object map: keys avoid boxing, unlike a
 * `MutableMap<Long, V>`. Meant for keys like packed chunk coordinates. Two `Long` values are
 * reserved as slot sentinels and cannot be used as keys; storing either throws
 * [IllegalArgumentException]. `null` is not a valid stored value, it doubles as the "absent"
 * result of [get], so lookups need no separate `containsKey` check.
 */
public class Long2ObjectMap<V : Any>(
    initialCapacity: Int = 16,
) {
    private var keys: LongArray
    private var values: Array<Any?>

    /** Live entries plus tombstones from [remove]; drives when to [grow]. */
    private var occupied: Int = 0

    public var size: Int = 0
        private set

    init {
        val capacity = nextPowerOfTwo(initialCapacity.coerceAtLeast(4))
        keys = LongArray(capacity) { EMPTY }
        values = arrayOfNulls(capacity)
    }

    public fun isEmpty(): Boolean = size == 0

    public fun containsKey(key: Long): Boolean {
        checkKey(key)
        return findSlot(key) >= 0
    }

    @Suppress("UNCHECKED_CAST")
    public fun get(key: Long): V? {
        checkKey(key)
        val slot = findSlot(key)
        return if (slot >= 0) values[slot] as V else null
    }

    /** Returns the previous value, or `null` if [key] was not present. */
    @Suppress("UNCHECKED_CAST")
    public fun put(
        key: Long,
        value: V,
    ): V? {
        checkKey(key)
        if (occupied * 4 >= keys.size * 3) grow()

        var index = indexFor(key)
        var firstTombstone = -1
        while (true) {
            when (keys[index]) {
                EMPTY -> {
                    val target = if (firstTombstone >= 0) firstTombstone else index
                    if (target == index) occupied++
                    keys[target] = key
                    values[target] = value
                    size++
                    return null
                }
                TOMBSTONE -> if (firstTombstone < 0) firstTombstone = index
                key -> {
                    val previous = values[index]
                    values[index] = value
                    return previous as V
                }
                else -> Unit
            }
            index = (index + 1) and (keys.size - 1)
        }
    }

    /** Returns the removed value, or `null` if [key] was not present. */
    @Suppress("UNCHECKED_CAST")
    public fun remove(key: Long): V? {
        checkKey(key)
        val slot = findSlot(key)
        if (slot < 0) return null
        val previous = values[slot]
        keys[slot] = TOMBSTONE
        values[slot] = null
        size--
        return previous as V
    }

    private fun findSlot(key: Long): Int {
        var index = indexFor(key)
        while (true) {
            val existing = keys[index]
            if (existing == EMPTY) return -1
            if (existing == key) return index
            index = (index + 1) and (keys.size - 1)
        }
    }

    private fun indexFor(key: Long): Int = (mix(key) and (keys.size - 1).toLong()).toInt()

    private fun grow() {
        val oldKeys = keys
        val oldValues = values
        keys = LongArray(oldKeys.size * 2) { EMPTY }
        values = arrayOfNulls(oldKeys.size * 2)
        occupied = 0
        size = 0
        for (i in oldKeys.indices) {
            val k = oldKeys[i]
            if (k != EMPTY && k != TOMBSTONE) {
                @Suppress("UNCHECKED_CAST")
                put(k, oldValues[i] as V)
            }
        }
    }

    private fun checkKey(key: Long) {
        require(key != EMPTY && key != TOMBSTONE) { "key $key is reserved and cannot be stored" }
    }

    public companion object {
        private const val EMPTY: Long = Long.MIN_VALUE
        private const val TOMBSTONE: Long = Long.MIN_VALUE + 1

        // parity: not vanilla-related; this is splitmix64's finalizer, used only to spread keys
        // evenly across buckets (packed chunk coordinates would otherwise cluster).
        private fun mix(x: Long): Long {
            var h = x
            h = (h xor (h ushr 30)) * -0x40a7b892e31b1a47L
            h = (h xor (h ushr 27)) * -0x6b2fb644ecceee15L
            h = h xor (h ushr 31)
            return h
        }

        private fun nextPowerOfTwo(x: Int): Int {
            var v = x - 1
            v = v or (v ushr 1)
            v = v or (v ushr 2)
            v = v or (v ushr 4)
            v = v or (v ushr 8)
            v = v or (v ushr 16)
            return v + 1
        }
    }
}
