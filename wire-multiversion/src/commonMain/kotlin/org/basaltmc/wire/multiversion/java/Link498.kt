package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.15 to 1.14.4, where a biome is still a column and an entity brings its own data.
 *
 * 1.14 has one biome per column and reads them after the sections, so 1.15's thousand and twenty
 * four become two hundred and fifty six, taken from the bottom of each.
 *
 * A spawn gets its empty set of entity data back, and the world seed goes.
 */
internal class Link498(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            MapChunkPacket.ID -> foldBiomesIn(packet)
            LoginPlayPacket.ID -> dropSeedAndRespawnScreen(packet)
            RespawnPacket.ID -> dropSeed(packet)
            LevelParticlesPacket.ID -> shortenCoordinates(packet)
            // A mob and a player are the same packet here as far as this server is concerned, so
            // which one it is has to be read off the number 1.15 gave it.
            SpawnEntityPacket.ID ->
                if (packet.id == ADD_MOB || packet.id == ADD_PLAYER) appendEmptyData(packet)
        }
        return true
    }

    /** The biomes go where 1.14 looks for them: after the sections, in the same buffer. */
    private fun foldBiomesIn(packet: MutablePacket) {
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val fullChunk = input.readByte()
        val mask = input.readVarInt()
        val heightmaps = input.readAnonymousNbt()
        val biomes = IntArray(BIOMES_IN_CHUNK) { input.readInt() }
        val sections = input.readBytes(input.readVarInt())
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeByte(fullChunk)
            output.writeVarInt(mask)
            output.writeAnonymousNbt(heightmaps)
            output.writeVarInt(sections.size + COLUMNS * Int.SIZE_BYTES)
            output.writeBytes(sections)
            for (index in 0 until COLUMNS) output.writeInt(biomeOf(biomes, index))
            output.writeBytes(rest)
        }
    }

    /** The biome of a column, which 1.15 has one of per four blocks of height and 1.14 one of. */
    private fun biomeOf(
        biomes: IntArray,
        column: Int,
    ): Int {
        val x = (column % COLUMN_SIDE) / BIOME_CELL
        val z = (column / COLUMN_SIDE) / BIOME_CELL
        return biomes[z * BIOME_SIDE + x]
    }

    private fun dropSeedAndRespawnScreen(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readInt()
        val gamemode = input.readByte()
        val dimension = input.readInt()
        input.readLong()
        val maxPlayers = input.readByte()
        val levelType = input.readProtocolString()
        val viewDistance = input.readVarInt()
        val reducedDebugInfo = input.readByte()
        encode(packet) { output ->
            output.writeInt(entityId)
            output.writeByte(gamemode)
            output.writeInt(dimension)
            output.writeByte(maxPlayers)
            output.writeProtocolString(levelType)
            output.writeVarInt(viewDistance)
            output.writeByte(reducedDebugInfo)
        }
    }

    private fun dropSeed(packet: MutablePacket) {
        val input = decode(packet) { it }
        val dimension = input.readInt()
        input.readLong()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeInt(dimension)
            output.writeBytes(rest)
        }
    }

    /** 1.15 made a particle's position a double; 1.14 reads three floats. */
    private fun shortenCoordinates(packet: MutablePacket) {
        val input = decode(packet) { it }
        val particle = input.readInt()
        val longDistance = input.readByte()
        val x = input.readDouble()
        val y = input.readDouble()
        val z = input.readDouble()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeInt(particle)
            output.writeByte(longDistance)
            output.writeFloat(x.toFloat())
            output.writeFloat(y.toFloat())
            output.writeFloat(z.toFloat())
            output.writeBytes(rest)
        }
    }

    /**
     * An empty set of entity data, which 1.14 expects at the end of a spawn.
     *
     * What the entity actually looks like arrives right after in its own packet, so nothing is
     * lost by starting it empty: the client reads the terminator and waits for that one.
     */
    private fun appendEmptyData(packet: MutablePacket) {
        val body = packet.body
        encode(packet) { output ->
            output.writeBytes(body)
            output.writeByte(END_OF_DATA)
        }
    }

    private companion object {
        const val BIOMES_IN_CHUNK = 1024
        const val COLUMNS = 16 * 16
        const val COLUMN_SIDE = 16
        const val BIOME_CELL = 4
        const val BIOME_SIDE = 4

        /** What 1.15 calls the two spawns that carry the entity's data with them. */
        const val ADD_MOB = 0x03
        const val ADD_PLAYER = 0x05

        /** parity: `EntityDataSerializer` ends a list with an index of 255. */
        const val END_OF_DATA = 0xFF.toByte()
    }
}
