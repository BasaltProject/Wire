package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.ComposedTables
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.NameIdMap
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.deepCopy
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.SectionBlocksUpdatePacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.readVarLong
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.16.2 to 1.16.1, where the world stops being described to the client.
 *
 * The dimension goes out as a name, the biomes are translated through theirs against the list the
 * client has compiled in, hardcore goes back to being a bit of the gamemode, and a line meant for
 * above the hotbar is sent as a title, which is the only way this client shows one (MC-119145).
 */
internal class Link736(
    tables: VersionTables,
    private val target: ComposedTables,
) : RenumberingLink(tables) {
    /** This server's biome ids to 1.16.1's, filled in from the codec when the player joins. */
    private var biomes = IntArray(0)

    /** The dimensions of the last login, so that a respawn can be named without one going past. */
    private var known: List<NbtCompound> = emptyList()

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            LoginPlayPacket.ID -> rewriteLogin(packet)
            RespawnPacket.ID -> rewriteRespawn(packet)
            SystemChatPacket.ID -> return aboveTheHotbar(packet)
            MapChunkPacket.ID -> rewriteChunk(packet)
            SectionBlocksUpdatePacket.ID -> rewriteSectionUpdate(packet)
        }
        return true
    }

    private fun rewriteLogin(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readInt()
        val hardcore = input.readByte().toInt() != 0
        val gamemode = input.readByte().toInt() and 0xFF
        val previousGamemode = input.readByte()
        val worldNames = List(input.readVarInt()) { input.readProtocolString() }
        val codec = input.readAnonymousNbt() as? NbtCompound ?: NbtCompound()
        val dimension = input.readAnonymousNbt()
        val worldName = input.readProtocolString()
        val hashedSeed = input.readLong()
        val maxPlayers = input.readVarInt()
        val rest = input.readBytes(input.readableBytes)
        rememberBiomes(codec)
        known = registryEntries(codec, DIMENSION_REGISTRY).orEmpty()
        encode(packet) { output ->
            output.writeInt(entityId)
            output.writeByte((if (hardcore) gamemode or HARDCORE else gamemode).toByte())
            output.writeByte(previousGamemode)
            output.writeVarInt(worldNames.size)
            for (name in worldNames) output.writeProtocolString(name)
            output.writeAnonymousNbt(dimensionRegistry(codec))
            output.writeProtocolString(dimensionName(codec, dimension))
            output.writeProtocolString(worldName)
            output.writeLong(hashedSeed)
            output.writeByte(minOf(maxPlayers, MAX_PLAYERS_SHOWN).toByte())
            output.writeBytes(rest)
        }
    }

    private fun rewriteRespawn(packet: MutablePacket) {
        val input = decode(packet) { it }
        val dimension = input.readAnonymousNbt()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeProtocolString(dimensionName(null, dimension))
            output.writeBytes(rest)
        }
    }

    /** parity: MC-119145, where a client this old drops chat aimed above the hotbar on the floor. */
    private fun aboveTheHotbar(packet: MutablePacket): Boolean {
        val input = decode(packet) { it }
        val json = input.readProtocolString(MAX_TEXT)
        if (input.readByte().toInt() != ABOVE_HOTBAR) return true
        packet.overrideId(TITLE)
        encode(packet) { output ->
            output.writeVarInt(SET_ACTION_BAR)
            output.writeProtocolString(json, MAX_TEXT)
        }
        return true
    }

    /** The biomes go out as this client numbers them, and the light flag it still reads comes back. */
    private fun rewriteChunk(packet: MutablePacket) {
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val fullChunk = input.readByte()
        val mask = input.readVarInt()
        val heightmaps = input.readAnonymousNbt()
        val biomeIds = IntArray(input.readVarInt()) { input.readVarInt() }
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeByte(fullChunk)
            output.writeBoolean(true)
            output.writeVarInt(mask)
            output.writeAnonymousNbt(heightmaps)
            for (index in 0 until BIOMES_IN_CHUNK) output.writeInt(biomeAt(biomeIds, index))
            output.writeBytes(rest)
        }
    }

    /** 1.16.2 packed a whole section into one long; 1.16.1 names the chunk and each block in it. */
    private fun rewriteSectionUpdate(packet: MutablePacket) {
        val input = decode(packet) { it }
        val packed = input.readLong()
        input.readByte()
        val changes = LongArray(input.readVarInt()) { input.readVarLong() }
        val sectionX = ((packed shl 0) shr 42).toInt()
        val sectionY = ((packed shl 44) shr 44).toInt()
        val sectionZ = ((packed shl 22) shr 42).toInt()
        encode(packet) { output ->
            output.writeInt(sectionX)
            output.writeInt(sectionZ)
            output.writeVarInt(changes.size)
            for (change in changes) {
                val position = (change and 0xFFFL).toInt()
                output.writeByte((((position shr 8) and 15) shl 4 or ((position shr 4) and 15)).toByte())
                output.writeByte((sectionY * SECTION_HEIGHT + (position and 15)).toByte())
                output.writeVarInt((change ushr 12).toInt())
            }
        }
    }

    private fun rememberBiomes(codec: NbtCompound) {
        val entries = registryEntries(codec, BIOME_REGISTRY) ?: return
        var highest = 0
        for (entry in entries) highest = maxOf(highest, entry.getInt("id"))
        val fallback = target.staticBiomes.id(FALLBACK_BIOME)
        val mapped = IntArray(highest + 1) { if (fallback == NameIdMap.NONE) 0 else fallback }
        for (entry in entries) {
            val id = entry.getInt("id")
            if (id !in mapped.indices) continue
            val theirs = target.staticBiomes.id(entry.getString("name"))
            if (theirs != NameIdMap.NONE) mapped[id] = theirs
        }
        biomes = mapped
    }

    private fun biomeAt(
        ids: IntArray,
        index: Int,
    ): Int {
        val id = ids.getOrElse(index) { 0 }
        return if (id in biomes.indices) biomes[id] else 0
    }

    /** Everything 1.16.1 is told about the world: the dimensions, and nothing else. */
    private fun dimensionRegistry(codec: NbtCompound): NbtCompound {
        val entries = registryEntries(codec, DIMENSION_REGISTRY).orEmpty()
        val dimensions =
            entries.map { entry ->
                val element = (entry["element"] as? NbtCompound)?.deepCopy() ?: NbtCompound()
                element.putString("name", entry.getString("name"))
                // 1.16.2 turned the nether's halved coordinates into a scale; before it, a flag.
                element.putBoolean("shrunk", element.getDouble("coordinate_scale", 1.0) != 1.0)
                element
            }
        val registry = NbtCompound()
        registry["dimension"] = NbtList(NbtType.COMPOUND, dimensions)
        return registry
    }

    /**
     * Which dimension the settings that just went past belong to.
     *
     * Matched against the registry rather than guessed from its contents: the settings sent in the
     * login packet are one of the entries, so the entry that equals them is the answer.
     */
    private fun dimensionName(
        codec: NbtCompound?,
        dimension: NbtTag,
    ): String {
        val entries = codec?.let { registryEntries(it, DIMENSION_REGISTRY) } ?: known
        for (entry in entries) {
            if (entry["element"] == dimension) return entry.getString("name")
        }
        return OVERWORLD
    }

    private fun registryEntries(
        codec: NbtCompound,
        registry: String,
    ): List<NbtCompound>? {
        val holder = codec[registry] as? NbtCompound ?: return null
        val values = holder.getList("value") ?: return null
        return values.toList().filterIsInstance<NbtCompound>()
    }

    private companion object {
        const val DIMENSION_REGISTRY = "minecraft:dimension_type"
        const val BIOME_REGISTRY = "minecraft:worldgen/biome"
        const val OVERWORLD = "minecraft:overworld"
        const val FALLBACK_BIOME = "minecraft:plains"

        /** parity: `GameType` packed hardcore into the high bit before 1.16.2 gave it a field. */
        const val HARDCORE = 0x08
        const val MAX_PLAYERS_SHOWN = 255
        const val BIOMES_IN_CHUNK = 1024
        const val SECTION_HEIGHT = 16

        const val TITLE = 0x4F
        const val SET_ACTION_BAR = 2
        const val ABOVE_HOTBAR = 2
        const val MAX_TEXT = 262144
    }
}
