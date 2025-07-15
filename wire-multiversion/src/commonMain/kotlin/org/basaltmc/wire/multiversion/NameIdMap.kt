package org.basaltmc.wire.multiversion

/**
 * A registry the older version has built into it, by name.
 *
 * Biomes below 1.16.2 are what this exists for. From 1.16.2 the server sends its own numbering in
 * the login codec and a chunk carries those numbers; a client older than that has the list compiled
 * in and reads the same numbers against its own. So the ids have to travel through the names, which
 * is the only thing the two sides agree on.
 */
public class NameIdMap(
    private val encoded: String,
) {
    private var names: Array<String> = EMPTY_NAMES
    private var ids: IntArray = EMPTY_IDS
    private var parsed = false

    public val isEmpty: Boolean
        get() {
            if (!parsed) parse()
            return names.isEmpty()
        }

    /** That version's id for [name], or [NONE] when it has no such entry. */
    public fun id(name: String): Int {
        if (!parsed) parse()
        var low = 0
        var high = names.size - 1
        while (low <= high) {
            val mid = (low + high) ushr 1
            val comparison = names[mid].compareTo(name)
            when {
                comparison < 0 -> low = mid + 1
                comparison > 0 -> high = mid - 1
                else -> return ids[mid]
            }
        }
        return NONE
    }

    /** The name of an id, for a version that reads by name what this one numbers. */
    public fun name(id: Int): String? {
        if (!parsed) parse()
        for (index in ids.indices) {
            if (ids[index] == id) return names[index]
        }
        return null
    }

    private fun parse() {
        val entries = if (encoded.isEmpty()) emptyList() else encoded.split(';')
        // A name has colons of its own, so the id is what follows the last one.
        names = Array(entries.size) { entries[it].substringBeforeLast(':') }
        ids = IntArray(entries.size) { entries[it].substringAfterLast(':').toInt() }
        parsed = true
    }

    public companion object {
        public const val NONE: Int = -1

        public val EMPTY: NameIdMap = NameIdMap("")

        private val EMPTY_NAMES = emptyArray<String>()
        private val EMPTY_IDS = IntArray(0)
    }
}
