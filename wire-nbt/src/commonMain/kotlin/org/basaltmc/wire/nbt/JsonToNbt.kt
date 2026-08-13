package org.basaltmc.wire.nbt

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive

/**
 * Datapack JSON as NBT, which is the form a registry entry travels in.
 *
 * Mojang's codecs read numbers through `NumericTag`, so the numeric type is forgiving; the shape
 * is not. A list's elements must all share a type, the one rule JSON does not enforce and NBT does.
 * A boolean becomes a byte.
 */
public object JsonToNbt {
    /** Converts, or throws when the JSON has a shape NBT cannot hold (a mixed array, a null). */
    public fun convert(element: JsonElement): NbtTag =
        when (element) {
            is JsonObject -> convertObject(element)
            is JsonArray -> convertArray(element)
            is JsonPrimitive -> convertPrimitive(element)
            JsonNull -> throw NbtConversionException("null has no NBT equivalent")
            else -> throw NbtConversionException("unsupported JSON element")
        }

    private fun convertObject(element: JsonObject): NbtCompound {
        val compound = NbtCompound()
        for ((key, value) in element) {
            // A null field is simply absent in NBT, which is what an optional codec expects anyway.
            if (value is JsonNull) continue
            compound[key] = convert(value)
        }
        return compound
    }

    private fun convertArray(element: JsonArray): NbtTag {
        if (element.isEmpty()) return NbtList(NbtType.END)
        val converted = element.map { convert(it) }
        val type = converted.first().type
        if (converted.any { it.type != type }) {
            // A mixed array cannot be an NbtList, and silently coercing would change the data.
            throw NbtConversionException("a JSON array of mixed types has no NBT equivalent")
        }
        return NbtList(type, converted)
    }

    private fun convertPrimitive(element: JsonPrimitive): NbtTag {
        if (element.isString) return NbtString(element.content)
        element.content.toBooleanStrictOrNull()?.let { return NbtByte(if (it) 1 else 0) }
        val text = element.content
        if ('.' in text || 'e' in text || 'E' in text) {
            return NbtDouble(text.toDoubleOrNull() ?: throw NbtConversionException("'$text' is not a number"))
        }
        text.toIntOrNull()?.let { return NbtInt(it) }
        text.toLongOrNull()?.let { return NbtLong(it) }
        return NbtDouble(text.toDoubleOrNull() ?: throw NbtConversionException("'$text' is not a number"))
    }
}

/** Thrown when JSON cannot be represented as NBT without changing what it says. */
public class NbtConversionException(
    message: String,
) : Exception(message)
