package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals

class HudPacketsTest {
    @Test
    fun titleTextRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SetTitleTextPacket(Component.text("Welcome"))
        packet.write(buffer)
        assertEquals(packet, SetTitleTextPacket.read(buffer))
    }

    @Test
    fun subtitleAndActionBarRoundTrip() {
        val subtitleBuffer = ByteBuffer()
        val subtitle = SetSubtitleTextPacket(Component.text("to the server"))
        subtitle.write(subtitleBuffer)
        assertEquals(subtitle, SetSubtitleTextPacket.read(subtitleBuffer))

        val actionBarBuffer = ByteBuffer()
        val actionBar = SetActionBarTextPacket(Component.text("watch out"))
        actionBar.write(actionBarBuffer)
        assertEquals(actionBar, SetActionBarTextPacket.read(actionBarBuffer))
    }

    @Test
    fun titleTimingsAndClearRoundTrip() {
        val timingsBuffer = ByteBuffer()
        val timings = SetTitlesAnimationPacket(fadeIn = 10, stay = 70, fadeOut = 20)
        timings.write(timingsBuffer)
        assertEquals(timings, SetTitlesAnimationPacket.read(timingsBuffer))

        val clearBuffer = ByteBuffer()
        val clear = ClearTitlesPacket(resetTimes = true)
        clear.write(clearBuffer)
        assertEquals(clear, ClearTitlesPacket.read(clearBuffer))
    }

    @Test
    fun soundRoundTripsThroughItsEighthOfABlockPositions() {
        val buffer = ByteBuffer()
        val packet =
            SoundPacket(
                sound = "minecraft:entity.experience_orb.pickup",
                source = SoundSource.PLAYERS,
                x = 10.5,
                y = 64.0,
                z = -20.25,
                volume = 1f,
                pitch = 1.5f,
                seed = 42L,
            )
        packet.write(buffer)
        assertEquals(packet, SoundPacket.read(buffer))
    }

    @Test
    fun stopSoundCarriesEitherHalfOrNeither() {
        for (
        packet in
        listOf(
            StopSoundPacket(sound = null, source = null),
            StopSoundPacket(sound = "minecraft:music.game", source = null),
            StopSoundPacket(sound = null, source = SoundSource.MUSIC),
            StopSoundPacket(sound = "minecraft:music.game", source = SoundSource.MUSIC),
        )
        ) {
            val buffer = ByteBuffer()
            packet.write(buffer)
            assertEquals(packet, StopSoundPacket.read(buffer))
        }
    }

    @Test
    fun soundSourcesUseVanillasOwnCommandNames() {
        assertEquals(SoundSource.BLOCKS, SoundSource.byCommandName("block"))
        assertEquals(SoundSource.RECORDS, SoundSource.byCommandName("record"))
        assertEquals("player", SoundSource.PLAYERS.commandName)
    }
}
