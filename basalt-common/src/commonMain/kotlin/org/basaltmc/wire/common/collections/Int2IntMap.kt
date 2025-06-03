package org.basaltmc.wire.common.collections

/**
 * Open-addressing primitive int-to-int map: no boxing, unlike a `MutableMap<Int, Int>`.
 * Two `Int` values are reserved as slot sentinels and cannot be used as keys; storing either
 * throws [IllegalArgumentException]. [get] returns [NO_VALUE] for a missing key, so callers who
 * need to distinguish "absent" from "stored value happens to equal NO_VALUE" should use
 * [containsKey] or [getOrDefault], the same caveat fastutil's primitive maps document.
 */
public class Int2IntMap(
    initialCapacity: Int = 16,
) {
    private var keys: IntArray
    private var values: IntArray

    /** Live entries plus tombstones from [remove]; drives when to [grow]. */
    private var occupied: Int = 0

    public var size: Int = 0
        private set

    init {
        val capacity = nextPowerOfTwo(initialCapacity.coerceAtLeast(4))
        keys = IntArray(capacity) { EMPTY }
        values = IntArray(capacity)
    }

    public fun isEmpty(): Boolean = size == 0

    public fun containsKey(key: Int): Boolean {
        checkKey(key)
        return findSlot(key) >= 0
    }

    public fun get(key: Int): Int = getOrDefault(key, NO_VALUE)

    public fun getOrDefault(
        key: Int,
        default: Int,
    ): Int {
        checkKey(key)
        val slot = findSlot(key)
        return if (slot >= 0) values[slot] else default
    }

    /** Returns the previous value, or [NO_VALUE] if [key] was not present. */
    public fun put(
        key: Int,
        value: Int,
    ): Int {
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
                    return NO_VALUE
                }
                TOMBSTONE -> if (firstTombstone < 0) firstTombstone = index
                key -> {
                    val previous = values[index]
                    values[index] = value
                    return previous
                }
                else -> Unit
            }
            index = (index + 1) and (keys.size - 1)
        }
    }

    /** Returns the removed value, or [NO_VALUE] if [key] was not present. */
    public fun remove(key: Int): Int {
        checkKey(key)
        val slot = findSlot(key)
        if (slot < 0) return NO_VALUE
        val previous = values[slot]
        keys[slot] = TOMBSTONE
        size--
        return previous
    }

    private fun findSlot(key: Int): Int {
        var index = indexFor(key)
        while (true) {
            val existing = keys[index]
            if (existing == EMPTY) return -1
            if (existing == key) return index
            index = (index + 1) and (keys.size - 1)
        }
    }

    private fun indexFor(key: Int): Int = mix(key) and (keys.size - 1)

    private fun grow() {
        val oldKeys = keys
        val oldValues = values
        keys = IntArray(oldKeys.size * 2) { EMPTY }
        values = IntArray(oldKeys.size * 2)
        occupied = 0
        size = 0
        for (i in oldKeys.indices) {
            val k = oldKeys[i]
            if (k != EMPTY && k != TOMBSTONE) put(k, oldValues[i])
        }
    }

    private fun checkKey(key: Int) {
        require(key != EMPTY && key != TOMBSTONE) { "key $key is reserved and cannot be stored" }
    }

    public companion object {
        public const val NO_VALUE: Int = Int.MIN_VALUE + 2
        private const val EMPTY: Int = Int.MIN_VALUE
        private const val TOMBSTONE: Int = Int.MIN_VALUE + 1

        // parity: not vanilla-related; this is Murmur3's 32-bit finalizer, used only to spread
        // keys evenly across buckets.
        private fun mix(x: Int): Int {
            var h = x
            h = h xor (h ushr 16)
            h *= -0x7ee3623b
            h = h xor (h ushr 13)
            h *= -0x3d4d51cb
            h = h xor (h ushr 16)
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
