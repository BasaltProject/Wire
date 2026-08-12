package org.basaltmc.wire.text

import kotlinx.serialization.json.Json
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ComponentJsonTest {
    private fun roundTrip(component: Component): Component {
        val json = ComponentJson.serialize(component)
        val text = json.toString()
        val parsed = Json.parseToJsonElement(text)
        return ComponentJson.deserialize(parsed)
    }

    @Test
    fun roundTripsSimpleText() {
        val component = Component.text("hello", TextColor.RED).bold()
        assertEquals(component, roundTrip(component))
    }

    @Test
    fun roundTripsNestedChildrenAndTranslatable() {
        val component =
            Component
                .translatable("chat.type.text", Component.text("Steve"), Component.text("hi"))
                .append(Component.text(" (extra)").italic())
        assertEquals(component, roundTrip(component))
    }

    @Test
    fun roundTripsClickAndHoverEvents() {
        val component =
            Component
                .text("click me")
                .clickEvent(ClickEvent.runCommand("/help"))
                .hoverEvent(HoverEvent.ShowText(Component.text("a tooltip")))
        assertEquals(component, roundTrip(component))
    }

    @Test
    fun roundTripsKeybindAndScore() {
        assertEquals(Component.keybind("key.jump"), roundTrip(Component.keybind("key.jump")))
        assertEquals(Component.score("Steve", "kills"), roundTrip(Component.score("Steve", "kills")))
    }

    @Test
    fun serializesColorAsNamedWhenPossible() {
        val json = ComponentJson.serialize(Component.text("x", TextColor.RED))
        assertTrue("\"color\":\"red\"" in json.toString())
    }
}
