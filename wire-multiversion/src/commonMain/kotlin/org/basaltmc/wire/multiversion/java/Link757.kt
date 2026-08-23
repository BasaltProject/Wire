package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.18.2 to 1.18 and 1.18.1, where the world a player is in is described rather than named.
 *
 * 1.18.2 started sending the name of the dimension type and letting the client look it up in the
 * registry it was just sent. Before that the whole description travels again in the packet, so it
 * is copied back out of the codec.
 */
internal class Link757(
    tables: VersionTables,
) : RenumberingLink(tables) {
    /** The last codec seen, which is where the description of a dimension comes from. */
    private var dimensions: Map<String, NbtTag> = emptyMap()

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            LoginPlayPacket.ID -> {
                val input = decode(packet) { it }
                val entityId = input.readInt()
                val hardcore = input.readByte()
                val gamemode = input.readByte()
                val previous = input.readByte()
                val worlds = List(input.readVarInt()) { input.readProtocolString() }
                val codec = input.readAnonymousNbt()
                remember(codec)
                val dimensionType = input.readProtocolString()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeInt(entityId)
                    output.writeByte(hardcore)
                    output.writeByte(gamemode)
                    output.writeByte(previous)
                    output.writeVarInt(worlds.size)
                    for (world in worlds) output.writeProtocolString(world)
                    output.writeAnonymousNbt(codec)
                    writeDimension(output, dimensionType)
                    output.writeBytes(rest)
                }
            }
            RespawnPacket.ID -> {
                val input = decode(packet) { it }
                val dimensionType = input.readProtocolString()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    writeDimension(output, dimensionType)
                    output.writeBytes(rest)
                }
            }
        }
        return true
    }

    /** Keeps the description of every dimension the codec carries, by name. */
    private fun remember(codec: NbtTag) {
        val root = codec as? NbtCompound ?: return
        val registry = root["minecraft:dimension_type"] as? NbtCompound ?: return
        val values = registry["value"] as? NbtList ?: return
        dimensions =
            buildMap {
                for (entry in values) {
                    val wrapped = entry as? NbtCompound ?: continue
                    val name = (wrapped["name"] as? org.basaltmc.wire.nbt.NbtString)?.value ?: continue
                    val element = wrapped["element"] ?: continue
                    put(name, element)
                }
            }
    }

    /**
     * The description of that dimension, or an empty one.
     *
     * An empty compound rather than nothing at all: a client this old reads a tag here whatever
     * happens, and one that is merely empty leaves it with defaults instead of a broken stream.
     */
    private fun writeDimension(
        output: ByteBuffer,
        name: String,
    ) {
        output.writeAnonymousNbt(dimensions[name] ?: NbtCompound())
    }
}
