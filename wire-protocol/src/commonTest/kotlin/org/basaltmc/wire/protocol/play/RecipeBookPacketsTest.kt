package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

class RecipeBookPacketsTest {
    @Test
    fun aShapedEntryRoundTrips() {
        val packet =
            RecipeBookAddPacket(
                entries =
                    listOf(
                        RecipeBookEntry(
                            recipeId = 7,
                            display =
                                RecipeDisplay.Shaped(
                                    width = 2,
                                    height = 2,
                                    ingredients =
                                        listOf(
                                            SlotDisplay.Item(5),
                                            SlotDisplay.Empty,
                                            SlotDisplay.Composite(
                                                listOf(SlotDisplay.Item(1), SlotDisplay.Item(2)),
                                            ),
                                            SlotDisplay.Item(9),
                                        ),
                                    result = SlotDisplay.Item(11),
                                    craftingStation = SlotDisplay.Item(3),
                                ),
                            category = RecipeBookEntry.CRAFTING_MISC_CATEGORY,
                            craftingRequirements = listOf(listOf(5), listOf(1, 2), listOf(9)),
                            notification = true,
                        ),
                    ),
                replace = false,
            )
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, RecipeBookAddPacket.read(buffer))
    }

    @Test
    fun aShapelessEntryWithNoRequirementsRoundTrips() {
        val packet =
            RecipeBookAddPacket(
                entries =
                    listOf(
                        RecipeBookEntry(
                            recipeId = 1,
                            display =
                                RecipeDisplay.Shapeless(
                                    ingredients = listOf(SlotDisplay.Item(4)),
                                    result = SlotDisplay.Item(8),
                                    craftingStation = SlotDisplay.Empty,
                                ),
                            category = RecipeBookEntry.CRAFTING_MISC_CATEGORY,
                            craftingRequirements = emptyList(),
                        ),
                    ),
                replace = true,
            )
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, RecipeBookAddPacket.read(buffer))
    }

    @Test
    fun removeRoundTrips() {
        val packet = RecipeBookRemovePacket(listOf(1, 2, 3))
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, RecipeBookRemovePacket.read(buffer))
    }
}
