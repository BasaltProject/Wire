package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.20.2 to 1.20 and 1.20.1, where there is no configuration phase at all.
 *
 * Everything the configuration would have said arrives inside the packet that starts the game, so
 * this link swallows the phase, keeps the registries out of it and puts them back there.
 *
 * Lost with the phase: the server brand and the block tags, which this build sends as configuration
 * packets. A client this old joins and plays without them.
 */
internal class Link763(
    tables: VersionTables,
) : RenumberingLink(tables) {
    private val registries = mutableMapOf<String, List<Pair<String, NbtCompound>>>()

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state == ProtocolState.CONFIGURATION) {
            if (packet.nativeId == RegistryDataPacket.ID) rememberRegistry(packet)
            // Nothing from the configuration reaches a client that has no such phase.
            return false
        }
        if (state != ProtocolState.PLAY) return true
        if (packet.nativeId == RespawnPacket.ID) {
            // The spawn arrives without its sea level (the link to 1.21 took it out) and has to
            // come out named rather than numbered, like the one inside the login packet.
            val spawn = decode(packet) { LoginFields.SpawnFields.read(it, withSeaLevel = false) }
            val keep =
                decode(packet) { input ->
                    LoginFields.SpawnFields.read(input, withSeaLevel = false)
                    input.readByte()
                }
            encode(packet) { output ->
                output.writeProtocolString(dimensionTypeName(spawn.dimension))
                output.writeProtocolString(spawn.dimensionName)
                output.writeLong(spawn.hashedSeed)
                output.writeByte(spawn.gamemode.toByte())
                output.writeByte(spawn.previousGamemode.toByte())
                output.writeBoolean(spawn.debug)
                output.writeBoolean(spawn.flat)
                output.writeByte(keep)
                val deathDimension = spawn.deathDimension
                output.writeBoolean(deathDimension != null)
                if (deathDimension != null) {
                    output.writeProtocolString(deathDimension)
                    output.writeLong(spawn.deathPosition)
                }
                output.writeVarInt(spawn.portalCooldown)
            }
            return true
        }
        if (packet.nativeId != LoginPlayPacket.ID) return true
        // By here the two links above have taken the online-mode flag and the sea level out of it.
        val login = decode(packet) { LoginFields.read(it, withOnlineMode = false, withSeaLevel = false) }
        encode(packet) { output ->
            output.writeInt(login.entityId)
            output.writeBoolean(login.hardcore)
            output.writeByte(login.spawn.gamemode.toByte())
            output.writeByte(login.spawn.previousGamemode.toByte())
            output.writeVarInt(login.worldNames.size)
            for (name in login.worldNames) output.writeProtocolString(name)
            output.writeAnonymousNbt(codec())
            output.writeProtocolString(dimensionTypeName(login.spawn.dimension))
            output.writeProtocolString(login.spawn.dimensionName)
            output.writeLong(login.spawn.hashedSeed)
            output.writeVarInt(login.maxPlayers)
            output.writeVarInt(login.viewDistance)
            output.writeVarInt(login.simulationDistance)
            output.writeBoolean(login.reducedDebugInfo)
            output.writeBoolean(login.respawnScreen)
            output.writeBoolean(login.spawn.debug)
            output.writeBoolean(login.spawn.flat)
            val deathDimension = login.spawn.deathDimension
            output.writeBoolean(deathDimension != null)
            if (deathDimension != null) {
                output.writeProtocolString(deathDimension)
                output.writeLong(login.spawn.deathPosition)
            }
            output.writeVarInt(login.spawn.portalCooldown)
        }
        return true
    }

    private fun rememberRegistry(packet: MutablePacket) {
        val data = decode(packet) { RegistryDataPacket.read(it) }
        if (data.registryId !in CODEC_REGISTRIES) return
        registries[data.registryId] =
            data.entries.mapNotNull { entry ->
                val value = entry.value as? NbtCompound ?: return@mapNotNull null
                entry.key to value
            }
    }

    /**
     * The one tag a 1.20.1 client reads its world out of.
     *
     * Chat types are in it because that client will not start without them, and this build has no
     * such registry to send: one entry, named the way vanilla names its own.
     */
    private fun codec(): NbtCompound {
        val codec = NbtCompound()
        for (registry in CODEC_REGISTRIES) {
            val entries = registries[registry] ?: if (registry == CHAT_TYPE) listOf(defaultChatType()) else continue
            val values =
                NbtList(
                    org.basaltmc.wire.nbt.NbtType.COMPOUND,
                    entries.mapIndexed { index, entry ->
                        NbtCompound().apply {
                            putString("name", entry.first)
                            putInt("id", index)
                            this["element"] = entry.second
                        }
                    },
                )
            val holder = NbtCompound()
            holder.putString("type", registry)
            holder["value"] = values
            codec[registry] = holder
        }
        return codec
    }

    private fun defaultChatType(): Pair<String, NbtCompound> {
        val decoration = NbtCompound()
        decoration.putString("translation_key", "chat.type.text")
        decoration["parameters"] =
            NbtList(org.basaltmc.wire.nbt.NbtType.STRING, listOf(NbtString("sender"), NbtString("content")))
        decoration["style"] = NbtCompound()
        val element = NbtCompound()
        element["chat"] = decoration
        element["narration"] = decoration
        return "minecraft:chat" to element
    }

    /** What the dimension this player spawned in is called, which is a name over there and a number here. */
    private fun dimensionTypeName(index: Int): String =
        registries[DIMENSION_TYPE]?.getOrNull(index)?.first ?: "minecraft:overworld"

    private companion object {
        const val DIMENSION_TYPE = "minecraft:dimension_type"
        const val CHAT_TYPE = "minecraft:chat_type"

        /** Everything a client of 1.20.1 looks for in the codec, in the order vanilla writes it. */
        val CODEC_REGISTRIES = listOf(DIMENSION_TYPE, "minecraft:worldgen/biome", CHAT_TYPE)
    }
}
