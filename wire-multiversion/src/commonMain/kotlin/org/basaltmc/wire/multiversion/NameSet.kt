package org.basaltmc.wire.multiversion

/**
 * A set of names, generated and parsed on first use.
 *
 * What a version cannot be told: the registry entries the version above it added and the attribute
 * keys it invented. Neither is a number that can be remapped: an element a client has never heard
 * of is not a smaller world, it is a client that refuses to load the registries at all.
 */
public class NameSet(
    private val encoded: String,
) {
    private var names: Array<String> = EMPTY
    private var parsed = false

    public val isEmpty: Boolean
        get() {
            if (!parsed) parse()
            return names.isEmpty()
        }

    /** Every name in it, for the caller that needs the set itself and not one question about it. */
    public fun toList(): List<String> {
        if (!parsed) parse()
        return names.toList()
    }

    public operator fun contains(name: String): Boolean {
        if (!parsed) parse()
        var low = 0
        var high = names.size - 1
        while (low <= high) {
            val mid = (low + high) ushr 1
            val comparison = names[mid].compareTo(name)
            when {
                comparison < 0 -> low = mid + 1
                comparison > 0 -> high = mid - 1
                else -> return true
            }
        }
        return false
    }

    private fun parse() {
        names = if (encoded.isEmpty()) EMPTY else encoded.split(';').sortedArray()
        parsed = true
    }

    private companion object {
        val EMPTY = emptyArray<String>()

        fun List<String>.sortedArray(): Array<String> = toTypedArray().also { it.sort() }
    }
}
