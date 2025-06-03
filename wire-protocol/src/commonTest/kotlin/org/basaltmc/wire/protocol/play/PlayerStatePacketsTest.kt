package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals

class PlayerStatePacketsTest {
    @Test
    fun setHealthRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SetHealthPacket(health = 17.5f, food = 12, saturation = 3.25f)
        packet.write(buffer)
        assertEquals(packet, SetHealthPacket.read(buffer))
    }

    @Test
    fun setExperienceRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SetExperiencePacket(progress = 0.4f, level = 7, total = 62)
        packet.write(buffer)
        assertEquals(packet, SetExperiencePacket.read(buffer))
    }

    @Test
    fun playerAbilitiesRoundTripEveryFlagCombination() {
        for (flags in 0 until 16) {
            val buffer = ByteBuffer()
            val packet =
                ClientboundPlayerAbilitiesPacket(
                    invulnerable = flags and 1 != 0,
                    flying = flags and 2 != 0,
                    mayFly = flags and 4 != 0,
                    instabuild = flags and 8 != 0,
                )
            packet.write(buffer)
            assertEquals(packet, ClientboundPlayerAbilitiesPacket.read(buffer))
        }
    }

    @Test
    fun serverboundAbilitiesCarryOnlyTheFlyingBit() {
        val buffer = ByteBuffer()
        ServerboundPlayerAbilitiesPacket(flying = true).write(buffer)
        assertEquals(0x02, buffer.readByte().toInt())
    }

    @Test
    fun respawnRoundTrips() {
        val buffer = ByteBuffer()
        val info = SpawnInfo(0, "minecraft:overworld", 42L, Gamemode.SURVIVAL, 255, false, true, null, 0, 63)
        val packet = RespawnPacket(info, RespawnPacket.KEEP_ALL_DATA)
        packet.write(buffer)
        assertEquals(packet, RespawnPacket.read(buffer))
    }

    @Test
    fun clientCommandRoundTripsEveryAction() {
        for (action in ClientCommandPacket.Action.entries) {
            val buffer = ByteBuffer()
            val packet = ClientCommandPacket(action)
            packet.write(buffer)
            assertEquals(packet, ClientCommandPacket.read(buffer))
        }
    }

    @Test
    fun combatKillRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            PlayerCombatKillPacket(
                playerId = 7,
                message = Component.translatable("death.attack.outOfWorld"),
            )
        packet.write(buffer)
        assertEquals(packet, PlayerCombatKillPacket.read(buffer))
    }
}
