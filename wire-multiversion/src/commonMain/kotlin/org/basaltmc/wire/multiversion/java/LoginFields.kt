package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * The packet that starts the game, read and written field by field.
 *
 * It cannot go through the packet class: by the time a link halfway down the chain sees this
 * packet, the links above have already taken fields out of it, so the reader that matches this
 * server no longer matches the bytes. Each link says what the shape it is looking at still has.
 *
 * What each version dropped, newest first: 26.2 added [onlineMode], and 1.21.2 added the sea level
 * to the spawn.
 */
internal class LoginFields(
    val entityId: Int,
    val hardcore: Boolean,
    val worldNames: List<String>,
    val maxPlayers: Int,
    val viewDistance: Int,
    val simulationDistance: Int,
    val reducedDebugInfo: Boolean,
    val respawnScreen: Boolean,
    val limitedCrafting: Boolean,
    val spawn: SpawnFields,
    val onlineMode: Boolean,
    val enforcesSecureChat: Boolean,
) {
    /** The half of it that describes where the player is, which the respawn packet carries alone. */
    class SpawnFields(
        val dimension: Int,
        val dimensionName: String,
        val hashedSeed: Long,
        val gamemode: Int,
        val previousGamemode: Int,
        val debug: Boolean,
        val flat: Boolean,
        val deathDimension: String?,
        val deathPosition: Long,
        val portalCooldown: Int,
        val seaLevel: Int,
    ) {
        fun write(
            output: ByteBuffer,
            withSeaLevel: Boolean,
        ) {
            output.writeVarInt(dimension)
            output.writeProtocolString(dimensionName)
            output.writeLong(hashedSeed)
            output.writeByte(gamemode.toByte())
            output.writeByte(previousGamemode.toByte())
            output.writeBoolean(debug)
            output.writeBoolean(flat)
            output.writeBoolean(deathDimension != null)
            deathDimension?.let {
                output.writeProtocolString(it)
                output.writeLong(deathPosition)
            }
            output.writeVarInt(portalCooldown)
            if (withSeaLevel) output.writeVarInt(seaLevel)
        }

        companion object {
            fun read(
                input: ByteBuffer,
                withSeaLevel: Boolean,
            ): SpawnFields {
                val dimension = input.readVarInt()
                val dimensionName = input.readProtocolString()
                val hashedSeed = input.readLong()
                val gamemode = input.readByte().toInt()
                val previousGamemode = input.readByte().toInt()
                val debug = input.readByte().toInt() != 0
                val flat = input.readByte().toInt() != 0
                val hasDeath = input.readByte().toInt() != 0
                val deathDimension = if (hasDeath) input.readProtocolString() else null
                val deathPosition = if (hasDeath) input.readLong() else 0L
                val portalCooldown = input.readVarInt()
                val seaLevel = if (withSeaLevel) input.readVarInt() else 0
                return SpawnFields(
                    dimension,
                    dimensionName,
                    hashedSeed,
                    gamemode,
                    previousGamemode,
                    debug,
                    flat,
                    deathDimension,
                    deathPosition,
                    portalCooldown,
                    seaLevel,
                )
            }
        }
    }

    fun write(
        output: ByteBuffer,
        withOnlineMode: Boolean,
        withSeaLevel: Boolean,
    ) {
        output.writeInt(entityId)
        output.writeBoolean(hardcore)
        output.writeVarInt(worldNames.size)
        for (name in worldNames) output.writeProtocolString(name)
        output.writeVarInt(maxPlayers)
        output.writeVarInt(viewDistance)
        output.writeVarInt(simulationDistance)
        output.writeBoolean(reducedDebugInfo)
        output.writeBoolean(respawnScreen)
        output.writeBoolean(limitedCrafting)
        spawn.write(output, withSeaLevel)
        if (withOnlineMode) output.writeBoolean(onlineMode)
        output.writeBoolean(enforcesSecureChat)
    }

    /** Where the player died, as a block position, for the versions that want it unpacked. */
    fun deathBlock(): BlockPos? = deathDimension()?.let { BlockPos.unpack(spawn.deathPosition) }

    fun deathDimension(): String? = spawn.deathDimension

    companion object {
        fun read(
            input: ByteBuffer,
            withOnlineMode: Boolean,
            withSeaLevel: Boolean,
        ): LoginFields {
            val entityId = input.readInt()
            val hardcore = input.readByte().toInt() != 0
            val worldNames = List(input.readVarInt()) { input.readProtocolString() }
            val maxPlayers = input.readVarInt()
            val viewDistance = input.readVarInt()
            val simulationDistance = input.readVarInt()
            val reducedDebugInfo = input.readByte().toInt() != 0
            val respawnScreen = input.readByte().toInt() != 0
            val limitedCrafting = input.readByte().toInt() != 0
            val spawn = SpawnFields.read(input, withSeaLevel)
            val onlineMode = if (withOnlineMode) input.readByte().toInt() != 0 else false
            val enforcesSecureChat = input.readByte().toInt() != 0
            return LoginFields(
                entityId,
                hardcore,
                worldNames,
                maxPlayers,
                viewDistance,
                simulationDistance,
                reducedDebugInfo,
                respawnScreen,
                limitedCrafting,
                spawn,
                onlineMode,
                enforcesSecureChat,
            )
        }
    }
}
