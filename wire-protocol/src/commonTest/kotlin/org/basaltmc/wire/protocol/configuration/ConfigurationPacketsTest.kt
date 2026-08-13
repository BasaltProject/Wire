package org.basaltmc.wire.protocol.configuration

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class ConfigurationPacketsTest {
    @Test
    fun clientInformationRoundTrips() {
        val buffer = ByteBuffer()
        buffer.writeProtocolString("en_us")
        buffer.writeByte(10.toByte())
        buffer.writeVarInt(0)
        buffer.writeBoolean(true)
        buffer.writeByte(127.toByte())
        buffer.writeVarInt(0)
        buffer.writeBoolean(false)
        buffer.writeBoolean(true)
        buffer.writeVarInt(0)

        val result = ClientInformationPacket.read(buffer)
        assertEquals("en_us", result.locale)
        assertEquals(10.toByte(), result.viewDistance)
        assertEquals(true, result.chatColors)
        assertEquals(0, result.mainHand)
        assertEquals(false, result.enableTextFiltering)
        assertEquals(true, result.enableServerListing)
    }

    @Test
    fun selectKnownPacksRoundTripsWithEntries() {
        val buffer = ByteBuffer()
        val packet = SelectKnownPacksPacket(listOf(KnownPack("minecraft", "core", "1.0")))
        packet.write(buffer)
        assertEquals(packet, SelectKnownPacksPacket.read(buffer))
    }

    @Test
    fun selectKnownPacksRoundTripsEmpty() {
        val buffer = ByteBuffer()
        val packet = SelectKnownPacksPacket(emptyList())
        packet.write(buffer)
        assertEquals(packet, SelectKnownPacksPacket.read(buffer))
    }

    @Test
    fun registryDataRoundTripsWithAndWithoutValues() {
        val buffer = ByteBuffer()
        val nbtValue = NbtCompound().also { it.putBoolean("piglin_safe", false) }
        val packet =
            RegistryDataPacket(
                "minecraft:dimension_type",
                listOf(
                    RegistryEntry("minecraft:overworld", nbtValue),
                    RegistryEntry("minecraft:the_nether", null),
                ),
            )
        packet.write(buffer)
        val result = RegistryDataPacket.read(buffer)
        assertEquals(packet.registryId, result.registryId)
        assertEquals(packet.entries, result.entries)
    }

    @Test
    fun registryEntryWithoutAValueReadsBackNull() {
        val buffer = ByteBuffer()
        RegistryEntry("minecraft:plains", null).write(buffer)
        val result = RegistryEntry.read(buffer)
        assertNull(result.value)
        assertEquals("minecraft:plains", result.key)
    }

    @Test
    fun configurationKeepAliveRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ConfigurationKeepAlivePacket(123456789L)
        packet.write(buffer)
        assertEquals(packet, ConfigurationKeepAlivePacket.read(buffer))
    }

    @Test
    fun tagsRoundTripsWithEntriesAndEmptyIds() {
        val buffer = ByteBuffer()
        val packet =
            TagsPacket(
                listOf(
                    TagGroup("minecraft:damage_type", listOf(TagEntry("minecraft:is_fire", emptyList()))),
                    TagGroup("minecraft:block", listOf(TagEntry("minecraft:logs", listOf(1, 2, 3)))),
                ),
            )
        packet.write(buffer)
        assertEquals(packet, TagsPacket.read(buffer))
    }

    @Test
    fun tagsRoundTripsEmpty() {
        val buffer = ByteBuffer()
        val packet = TagsPacket(emptyList())
        packet.write(buffer)
        assertEquals(packet, TagsPacket.read(buffer))
    }

    @Test
    fun brandRoundTrips() {
        val buffer = ByteBuffer()
        val packet = BrandPacket("Basalt 26.2")
        packet.write(buffer)
        assertEquals(packet, BrandPacket.read(buffer))
    }
}
