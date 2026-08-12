package org.basaltmc.wire.text

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import kotlinx.serialization.json.putJsonObject

/**
 * JSON serialization matching vanilla's modern component format (nested `clickEvent`/`hoverEvent`
 * objects, `extra` for children). Built on `kotlinx.serialization.json`'s [JsonElement] tree
 * rather than `@Serializable` data classes: the shape is too irregular (polymorphic by which key
 * is present, tri-state style fields) for automatic derivation to fit cleanly.
 */
public object ComponentJson {
    public fun serialize(component: Component): JsonElement =
        buildJsonObject {
            when (component) {
                is Component.Text -> put("text", component.content)
                is Component.Translatable -> {
                    put("translate", component.key)
                    component.fallback?.let { put("fallback", it) }
                    if (component.args.isNotEmpty()) {
                        putJsonArray("with") { component.args.forEach { add(serialize(it)) } }
                    }
                }
                is Component.Keybind -> put("keybind", component.key)
                is Component.Score ->
                    putJsonObject("score") {
                        put("name", component.name)
                        put("objective", component.objective)
                    }
                is Component.Selector -> {
                    put("selector", component.pattern)
                    component.separator?.let { put("separator", serialize(it)) }
                }
            }
            writeStyle(this, component.style)
            if (component.children.isNotEmpty()) {
                putJsonArray("extra") { component.children.forEach { add(serialize(it)) } }
            }
        }

    private fun writeStyle(
        builder: kotlinx.serialization.json.JsonObjectBuilder,
        style: Style,
    ) {
        style.color?.let { color ->
            builder.put("color", TextColor.nameOf(color) ?: color.toHexString())
        }
        style.bold?.let { builder.put("bold", it) }
        style.italic?.let { builder.put("italic", it) }
        style.underlined?.let { builder.put("underlined", it) }
        style.strikethrough?.let { builder.put("strikethrough", it) }
        style.obfuscated?.let { builder.put("obfuscated", it) }
        style.font?.let { builder.put("font", it) }
        style.insertion?.let { builder.put("insertion", it) }
        style.clickEvent?.let { event ->
            builder.putJsonObject("clickEvent") {
                put("action", event.action.id)
                put("value", event.value)
            }
        }
        style.hoverEvent?.let { event ->
            builder.putJsonObject("hoverEvent") {
                put("action", event.action)
                when (event) {
                    is HoverEvent.ShowText -> put("contents", serialize(event.value))
                    is HoverEvent.ShowItem ->
                        putJsonObject("contents") {
                            put("id", event.itemId)
                            put("count", event.count)
                        }
                    is HoverEvent.ShowEntity ->
                        putJsonObject("contents") {
                            put("type", event.entityType)
                            put("id", event.uuid)
                            event.name?.let { put("name", serialize(it)) }
                        }
                }
            }
        }
    }

    public fun deserialize(element: JsonElement): Component =
        when (element) {
            is JsonPrimitive -> Component.Text(element.content)
            is JsonArray -> deserializeArrayAsSiblingChain(element)
            is JsonObject -> deserializeObject(element)
            else -> Component.empty()
        }

    private fun deserializeArrayAsSiblingChain(array: JsonArray): Component {
        require(array.isNotEmpty()) { "Component JSON array must not be empty" }
        val parts = array.map { deserialize(it) }
        return parts.first().let { first ->
            var result = first
            for (i in 1 until parts.size) result = result.append(parts[i])
            result
        }
    }

    private fun deserializeObject(obj: JsonObject): Component {
        val style = readStyle(obj)
        val children = (obj["extra"] as? JsonArray)?.map { deserialize(it) } ?: emptyList()

        val base: Component =
            when {
                "text" in obj -> Component.Text((obj["text"] as JsonPrimitive).content)
                "translate" in obj ->
                    Component.Translatable(
                        key = (obj["translate"] as JsonPrimitive).content,
                        args = (obj["with"] as? JsonArray)?.map { deserialize(it) } ?: emptyList(),
                        fallback = (obj["fallback"] as? JsonPrimitive)?.contentOrNull,
                    )
                "keybind" in obj -> Component.Keybind((obj["keybind"] as JsonPrimitive).content)
                "score" in obj -> {
                    val score = obj["score"] as JsonObject
                    Component.Score(
                        name = (score["name"] as JsonPrimitive).content,
                        objective = (score["objective"] as JsonPrimitive).content,
                    )
                }
                "selector" in obj ->
                    Component.Selector(
                        pattern = (obj["selector"] as JsonPrimitive).content,
                        separator = (obj["separator"] as? JsonElement)?.let { deserialize(it) },
                    )
                else -> Component.empty()
            }
        return base.withStyle(style).let { withChildren(it, children) }
    }

    private fun withChildren(
        component: Component,
        children: List<Component>,
    ): Component {
        var result = component
        for (child in children) result = result.append(child)
        return result
    }

    private fun readStyle(obj: JsonObject): Style {
        val color = (obj["color"] as? JsonPrimitive)?.content?.let { TextColor.parse(it) }
        val clickEvent = (obj["clickEvent"] as? JsonObject)?.let { readClickEvent(it) }
        val hoverEvent = (obj["hoverEvent"] as? JsonObject)?.let { readHoverEvent(it) }
        return Style(
            color = color,
            bold = readBoolean(obj["bold"]),
            italic = readBoolean(obj["italic"]),
            underlined = readBoolean(obj["underlined"]),
            strikethrough = readBoolean(obj["strikethrough"]),
            obfuscated = readBoolean(obj["obfuscated"]),
            font = (obj["font"] as? JsonPrimitive)?.content,
            insertion = (obj["insertion"] as? JsonPrimitive)?.content,
            clickEvent = clickEvent,
            hoverEvent = hoverEvent,
        )
    }

    private fun readBoolean(element: JsonElement?): Boolean? =
        (element as? JsonPrimitive)?.content?.toBooleanStrictOrNull()

    private fun readClickEvent(obj: JsonObject): ClickEvent? {
        val action = (obj["action"] as? JsonPrimitive)?.content?.let { ClickEvent.Action.fromId(it) } ?: return null
        val value = (obj["value"] as? JsonPrimitive)?.content ?: return null
        return ClickEvent(action, value)
    }

    private fun readHoverEvent(obj: JsonObject): HoverEvent? {
        val action = (obj["action"] as? JsonPrimitive)?.content ?: return null
        val contents = obj["contents"] ?: return null
        return when (action) {
            "show_text" -> HoverEvent.ShowText(deserialize(contents))
            "show_item" -> {
                val contentsObj = contents as? JsonObject ?: return null
                HoverEvent.ShowItem(
                    itemId = (contentsObj["id"] as JsonPrimitive).content,
                    count = (contentsObj["count"] as? JsonPrimitive)?.content?.toIntOrNull() ?: 1,
                )
            }
            "show_entity" -> {
                val contentsObj = contents as? JsonObject ?: return null
                HoverEvent.ShowEntity(
                    entityType = (contentsObj["type"] as JsonPrimitive).content,
                    uuid = (contentsObj["id"] as JsonPrimitive).content,
                    name = (contentsObj["name"] as? JsonElement)?.let { deserialize(it) },
                )
            }
            else -> null
        }
    }
}
