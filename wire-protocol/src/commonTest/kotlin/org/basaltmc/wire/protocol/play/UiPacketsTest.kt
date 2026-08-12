package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

@OptIn(ExperimentalUuidApi::class)
class UiPacketsTest {
    private fun <T> roundTrip(
        value: T,
        write: (T, ByteBuffer) -> Unit,
        read: (ByteBuffer) -> T,
    ): T {
        val buffer = ByteBuffer()
        write(value, buffer)
        val decoded = read(buffer)
        // Nothing left over: a codec that reads less than it wrote desynchronises the whole stream.
        assertEquals(0, buffer.readableBytes, "trailing bytes after reading back $value")
        return decoded
    }

    @Test
    fun tabListHeaderAndFooterSurviveARoundTrip() {
        val packet =
            TabListPacket(
                header = Component.text("BASALT", TextColor.WHITE),
                footer = Component.text("20 TPS"),
            )

        assertEquals(packet, roundTrip(packet, { p, b -> p.write(b) }, TabListPacket::read))
    }

    @Test
    fun anObjectiveWritesItsDisplayOnlyWhenItIsAddedOrChanged() {
        val added =
            SetObjectivePacket(
                objectiveName = "basalt",
                method = SetObjectivePacket.METHOD_ADD,
                displayName = Component.text("Sidebar"),
                numberFormat = NumberFormat.Blank,
            )
        assertEquals(added, roundTrip(added, { p, b -> p.write(b) }, SetObjectivePacket::read))

        // A remove that wrote a display name would be the wrong length on the wire.
        val removed = SetObjectivePacket("basalt", SetObjectivePacket.METHOD_REMOVE)
        val decoded = roundTrip(removed, { p, b -> p.write(b) }, SetObjectivePacket::read)
        assertEquals("basalt", decoded.objectiveName)
        assertEquals(SetObjectivePacket.METHOD_REMOVE, decoded.method)
    }

    @Test
    fun aScoreCarriesItsOwnLineAndCanHideItsNumber() {
        val packet =
            SetScorePacket(
                owner = "line0",
                objectiveName = "basalt",
                score = 15,
                display = Component.text("Players: 3"),
                numberFormat = NumberFormat.Blank,
            )

        val decoded = roundTrip(packet, { p, b -> p.write(b) }, SetScorePacket::read)
        assertEquals(packet, decoded)
        assertTrue(decoded.numberFormat is NumberFormat.Blank)
    }

    @Test
    fun aScoreWithoutADisplayOrAFormatWritesNeither() {
        val packet = SetScorePacket("line0", "basalt", 3)

        assertEquals(packet, roundTrip(packet, { p, b -> p.write(b) }, SetScorePacket::read))
    }

    @Test
    fun aFixedNumberFormatKeepsItsComponent() {
        val packet =
            SetScorePacket("line0", "basalt", 1, numberFormat = NumberFormat.Fixed(Component.text("!")))

        assertEquals(packet, roundTrip(packet, { p, b -> p.write(b) }, SetScorePacket::read))
    }

    @Test
    fun theDisplaySlotAndItsObjectiveSurviveARoundTrip() {
        val packet = SetDisplayObjectivePacket(DisplaySlot.SIDEBAR, "basalt")

        assertEquals(packet, roundTrip(packet, { p, b -> p.write(b) }, SetDisplayObjectivePacket::read))
    }

    @Test
    fun resettingAScoreCanNameOneObjectiveOrNone() {
        val one = ResetScorePacket("line0", "basalt")
        val all = ResetScorePacket("line0")

        assertEquals(one, roundTrip(one, { p, b -> p.write(b) }, ResetScorePacket::read))
        assertEquals(all, roundTrip(all, { p, b -> p.write(b) }, ResetScorePacket::read))
    }

    @Test
    fun everyBossBarOperationSurvivesARoundTrip() {
        val id = Uuid.parse("11111111-2222-3333-4444-555555555555")
        val operations =
            listOf(
                BossEventPacket.Operation.Add(
                    name = Component.text("basaltmc.org"),
                    progress = 0.5f,
                    color = BossBarColor.PURPLE,
                    overlay = BossBarOverlay.NOTCHED_10,
                    darkenScreen = true,
                    createWorldFog = true,
                ),
                BossEventPacket.Operation.Remove,
                BossEventPacket.Operation.UpdateProgress(0.25f),
                BossEventPacket.Operation.UpdateName(Component.text("changed")),
                BossEventPacket.Operation.UpdateStyle(BossBarColor.GREEN, BossBarOverlay.PROGRESS),
                BossEventPacket.Operation.UpdateProperties(
                    darkenScreen = false,
                    playMusic = true,
                    createWorldFog = false,
                ),
            )

        for (operation in operations) {
            val packet = BossEventPacket(id, operation)
            assertEquals(packet, roundTrip(packet, { p, b -> p.write(b) }, BossEventPacket::read), "$operation")
        }
    }

    @Test
    fun theFlagByteKeepsEachPropertyApart() {
        val id = Uuid.parse("11111111-2222-3333-4444-555555555555")
        val packet =
            BossEventPacket(
                id,
                BossEventPacket.Operation.UpdateProperties(
                    darkenScreen = true,
                    playMusic = false,
                    createWorldFog = true,
                ),
            )

        val decoded = roundTrip(packet, { p, b -> p.write(b) }, BossEventPacket::read)
        val properties = decoded.operation as BossEventPacket.Operation.UpdateProperties
        assertTrue(properties.darkenScreen)
        assertTrue(!properties.playMusic)
        assertTrue(properties.createWorldFog)
    }
}
