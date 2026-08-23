package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.ChunkRewriter
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ChunksBiomesPacket
import org.basaltmc.wire.protocol.play.Heightmap
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt

/**
 * 1.21.5 to 1.21.4, where the chunk changed shape twice over.
 *
 * A palette used to write how many longs it holds; from 1.21.5 it does not, because the width and
 * the size say it. And the heightmaps used to be an NBT compound keyed by name, where they are now
 * a list of numbered ones.
 */
internal class Link769(
    tables: VersionTables,
) : RenumberingLink(tables) {
    /**
     * Identity on the ids: by the time a link sees a chunk the numbering is already the far end's,
     * so this only changes how a palette is written.
     */
    private val olderChunks =
        ChunkRewriter(
            org.basaltmc.wire.multiversion.IdMap.IDENTITY,
            tables.blockStateDirectBits,
            lengthPrefixedData = true,
            // The fluid count is long gone by here: 26.1 to 1.21.11 took it out.
            fluidCount = false,
            readsFluidCount = false,
        )

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        // Not super first here: this link replaces the chunk rewriting the base class does rather
        // than adding to it, so it reads the packet as it arrives and writes the older form itself.
        if (state != ProtocolState.PLAY) return super.clientbound(state, packet)
        when (packet.nativeId) {
            MapChunkPacket.ID -> {
                val chunk = decode(packet) { MapChunkPacket.read(it) }
                encode(packet) { output -> writeOlderChunk(chunk, output) }
                return true
            }
            ChunksBiomesPacket.ID -> {
                val biomes = decode(packet) { ChunksBiomesPacket.read(it) }
                val rewritten = biomes.chunks.map { it.copy(biomes = olderChunks.rewriteBiomes(it.biomes)) }
                encode(packet) { biomes.copy(chunks = rewritten).write(it) }
                return true
            }
        }
        return super.clientbound(state, packet)
    }

    private fun writeOlderChunk(
        chunk: MapChunkPacket,
        output: ByteBuffer,
    ) {
        // Everything after the heightmaps is the same in both versions, so it is written once by
        // the packet itself and copied across rather than spelled out again here.
        val modern = ByteBuffer().also { chunk.copy(chunkData = olderChunks.rewrite(chunk.chunkData)).write(it) }
        modern.readInt()
        modern.readInt()
        repeat(modern.readVarInt()) {
            modern.readVarInt()
            repeat(modern.readVarInt()) { modern.readLong() }
        }
        output.writeInt(chunk.x)
        output.writeInt(chunk.z)
        output.writeAnonymousNbt(namedHeightmaps(chunk.heightmaps))
        output.writeBytes(modern.readBytes(modern.readableBytes))
    }

    /** The heightmaps as 1.21.4 wants them: a compound keyed by the name of each one. */
    private fun namedHeightmaps(heightmaps: List<Heightmap>): NbtCompound {
        val tag = NbtCompound()
        for (heightmap in heightmaps) tag.putLongArray(heightmap.type.name, heightmap.data)
        return tag
    }
}
