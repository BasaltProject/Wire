package org.basaltmc.text

import org.basaltmc.nbt.NbtIO
import kotlin.test.Test
import kotlin.test.assertEquals

class ComponentNbtTest {
    private fun roundTrip(component: Component): Component {
        val tag = ComponentNbt.serialize(component)
        val bytes = NbtIO.write(tag)
        val (_, readBack) = NbtIO.read(bytes)
        return ComponentNbt.deserialize(readBack)
    }

    @Test
    fun roundTripsSimpleText() {
        val component = Component.text("hello", TextColor.RED).bold().underlined(false)
        assertEquals(component, roundTrip(component))
    }

    @Test
    fun roundTripsNestedChildrenAndEvents() {
        val component =
            Component
                .translatable("chat.type.text", Component.text("Steve"), Component.text("hi"))
                .clickEvent(ClickEvent.openUrl("https://basaltmc.org"))
                .hoverEvent(HoverEvent.ShowEntity("minecraft:pig", "00000000-0000-0000-0000-000000000000"))
                .append(Component.text(" more").italic())
        assertEquals(component, roundTrip(component))
    }

    @Test
    fun roundTripsSelectorAndScore() {
        val selector = Component.Selector("@a", separator = Component.text(", "))
        assertEquals(selector, roundTrip(selector))
        assertEquals(Component.score("Steve", "kills"), roundTrip(Component.score("Steve", "kills")))
    }
}
