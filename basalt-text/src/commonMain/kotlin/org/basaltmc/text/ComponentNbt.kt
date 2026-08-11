package org.basaltmc.text

import org.basaltmc.nbt.NbtByte
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtString
import org.basaltmc.nbt.NbtTag
import org.basaltmc.nbt.NbtType

/**
 * NBT serialization matching the modern network component format: the same shape as
 * [ComponentJson], but booleans are [NbtByte] (0/1) rather than JSON booleans, since that is what
 * the play-protocol packets for chat and other text actually carry.
 */
public object ComponentNbt {
    public fun serialize(component: Component): NbtTag {
        val compound = NbtCompound()
        when (component) {
            is Component.Text -> compound.putString("text", component.content)
            is Component.Translatable -> {
                compound.putString("translate", component.key)
                component.fallback?.let { compound.putString("fallback", it) }
                if (component.args.isNotEmpty()) {
                    compound["with"] = NbtList(NbtType.COMPOUND, component.args.map { serialize(it) })
                }
            }
            is Component.Keybind -> compound.putString("keybind", component.key)
            is Component.Score -> {
                val score = NbtCompound()
                score.putString("name", component.name)
                score.putString("objective", component.objective)
                compound["score"] = score
            }
            is Component.Selector -> {
                compound.putString("selector", component.pattern)
                component.separator?.let { compound["separator"] = serialize(it) }
            }
        }
        writeStyle(compound, component.style)
        if (component.children.isNotEmpty()) {
            compound["extra"] = NbtList(NbtType.COMPOUND, component.children.map { serialize(it) })
        }
        return compound
    }

    private fun writeStyle(
        compound: NbtCompound,
        style: Style,
    ) {
        style.color?.let { color -> compound.putString("color", TextColor.nameOf(color) ?: color.toHexString()) }
        style.bold?.let { compound.putBoolean("bold", it) }
        style.italic?.let { compound.putBoolean("italic", it) }
        style.underlined?.let { compound.putBoolean("underlined", it) }
        style.strikethrough?.let { compound.putBoolean("strikethrough", it) }
        style.obfuscated?.let { compound.putBoolean("obfuscated", it) }
        style.font?.let { compound.putString("font", it) }
        style.insertion?.let { compound.putString("insertion", it) }
        style.clickEvent?.let { event ->
            val click = NbtCompound()
            click.putString("action", event.action.id)
            click.putString("value", event.value)
            compound["clickEvent"] = click
        }
        style.hoverEvent?.let { event ->
            val hover = NbtCompound()
            hover.putString("action", event.action)
            when (event) {
                is HoverEvent.ShowText -> hover["contents"] = serialize(event.value)
                is HoverEvent.ShowItem -> {
                    val contents = NbtCompound()
                    contents.putString("id", event.itemId)
                    contents.putInt("count", event.count)
                    hover["contents"] = contents
                }
                is HoverEvent.ShowEntity -> {
                    val contents = NbtCompound()
                    contents.putString("type", event.entityType)
                    contents.putString("id", event.uuid)
                    event.name?.let { contents["name"] = serialize(it) }
                    hover["contents"] = contents
                }
            }
            compound["hoverEvent"] = hover
        }
    }

    public fun deserialize(tag: NbtTag): Component =
        when (tag) {
            is NbtString -> Component.Text(tag.value)
            is NbtList -> deserializeListAsSiblingChain(tag)
            is NbtCompound -> deserializeCompound(tag)
            else -> Component.empty()
        }

    private fun deserializeListAsSiblingChain(list: NbtList): Component {
        require(list.size > 0) { "Component NBT list must not be empty" }
        val parts = list.toList().map { deserialize(it) }
        var result = parts.first()
        for (i in 1 until parts.size) result = result.append(parts[i])
        return result
    }

    private fun deserializeCompound(compound: NbtCompound): Component {
        val style = readStyle(compound)
        val children = compound.getList("extra")?.toList()?.map { deserialize(it) } ?: emptyList()

        val base: Component =
            when {
                "text" in compound -> Component.Text(compound.getString("text"))
                "translate" in compound ->
                    Component.Translatable(
                        key = compound.getString("translate"),
                        args = compound.getList("with")?.toList()?.map { deserialize(it) } ?: emptyList(),
                        fallback = if ("fallback" in compound) compound.getString("fallback") else null,
                    )
                "keybind" in compound -> Component.Keybind(compound.getString("keybind"))
                "score" in compound -> {
                    val score = compound.getCompound("score")
                    Component.Score(name = score.getString("name"), objective = score.getString("objective"))
                }
                "selector" in compound ->
                    Component.Selector(
                        pattern = compound.getString("selector"),
                        separator = compound["separator"]?.let { deserialize(it) },
                    )
                else -> Component.empty()
            }
        var result = base.withStyle(style)
        for (child in children) result = result.append(child)
        return result
    }

    private fun readStyle(compound: NbtCompound): Style {
        val color = if ("color" in compound) TextColor.parse(compound.getString("color")) else null
        val click = (compound["clickEvent"] as? NbtCompound)?.let { readClickEvent(it) }
        val hover = (compound["hoverEvent"] as? NbtCompound)?.let { readHoverEvent(it) }
        return Style(
            color = color,
            bold = readBoolean(compound, "bold"),
            italic = readBoolean(compound, "italic"),
            underlined = readBoolean(compound, "underlined"),
            strikethrough = readBoolean(compound, "strikethrough"),
            obfuscated = readBoolean(compound, "obfuscated"),
            font = if ("font" in compound) compound.getString("font") else null,
            insertion = if ("insertion" in compound) compound.getString("insertion") else null,
            clickEvent = click,
            hoverEvent = hover,
        )
    }

    private fun readBoolean(
        compound: NbtCompound,
        key: String,
    ): Boolean? = (compound[key] as? NbtByte)?.let { it.value != 0.toByte() }

    private fun readClickEvent(compound: NbtCompound): ClickEvent? {
        val action = ClickEvent.Action.fromId(compound.getString("action")) ?: return null
        return ClickEvent(action, compound.getString("value"))
    }

    private fun readHoverEvent(compound: NbtCompound): HoverEvent? {
        val action = compound.getString("action")
        val contents = compound["contents"] ?: return null
        return when (action) {
            "show_text" -> HoverEvent.ShowText(deserialize(contents))
            "show_item" -> {
                val contentsCompound = contents as? NbtCompound ?: return null
                HoverEvent.ShowItem(contentsCompound.getString("id"), contentsCompound.getInt("count", 1))
            }
            "show_entity" -> {
                val contentsCompound = contents as? NbtCompound ?: return null
                HoverEvent.ShowEntity(
                    entityType = contentsCompound.getString("type"),
                    uuid = contentsCompound.getString("id"),
                    name = contentsCompound["name"]?.let { deserialize(it) },
                )
            }
            else -> null
        }
    }
}
