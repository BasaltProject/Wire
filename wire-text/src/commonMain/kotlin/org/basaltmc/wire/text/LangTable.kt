package org.basaltmc.wire.text

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonPrimitive

/** A flat translation-key to format-string table, e.g. parsed from `assets/minecraft/lang/en_us.json`. */
public class LangTable(
    private val entries: Map<String, String>,
) {
    /** Mirrors vanilla's `Language.getOrDefault`: a missing key falls back to [default]. */
    public fun getOrDefault(
        key: String,
        default: String,
    ): String = entries[key] ?: default

    public companion object {
        public val EMPTY: LangTable = LangTable(emptyMap())

        public fun parse(json: String): LangTable {
            val root = Json.parseToJsonElement(json) as JsonObject
            return LangTable(root.entries.associate { (key, value) -> key to value.jsonPrimitive.content })
        }
    }
}
