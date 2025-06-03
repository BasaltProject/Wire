package org.basaltmc.text

/**
 * What appears when a player hovers a piece of text. Mirrors Adventure's `HoverEvent` model.
 * `show_item`/`show_entity` use plain identifiers rather than richer item/entity types: those
 * live in `basalt-registry`/`basalt-world`, both of which sit above `basalt-text` in
 * the module dependency order, so this module cannot depend on them.
 */
public sealed class HoverEvent {
    public abstract val action: String

    public data class ShowText(
        val value: Component,
    ) : HoverEvent() {
        override val action: String = "show_text"
    }

    public data class ShowItem(
        val itemId: String,
        val count: Int = 1,
    ) : HoverEvent() {
        override val action: String = "show_item"
    }

    public data class ShowEntity(
        val entityType: String,
        val uuid: String,
        val name: Component? = null,
    ) : HoverEvent() {
        override val action: String = "show_entity"
    }
}
