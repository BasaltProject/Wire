package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertEquals

class CommandPacketsTest {
    @Test
    fun chatCommandRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ChatCommandPacket("gamemode creative Whiron")
        packet.write(buffer)
        assertEquals(packet, ChatCommandPacket.read(buffer))
    }

    @Test
    fun everyParserRoundTripsItsProperties() {
        val parsers =
            listOf(
                ArgumentParser.BrigadierString(ArgumentParser.BrigadierString.Kind.GREEDY_PHRASE),
                ArgumentParser.BrigadierInteger(),
                ArgumentParser.BrigadierInteger(min = 0),
                ArgumentParser.BrigadierInteger(min = -7, max = 7),
                ArgumentParser.Entity(single = true, playersOnly = true),
                ArgumentParser.Entity(single = false, playersOnly = false),
                ArgumentParser.BlockPosition,
                ArgumentParser.Vec3,
                ArgumentParser.Message,
                ArgumentParser.GameMode,
            )
        for (parser in parsers) {
            val buffer = ByteBuffer()
            buffer.writeVarInt(parser.registryId)
            parser.writeProperties(buffer)
            assertEquals(parser, ArgumentParser.read(buffer))
        }
    }

    @Test
    fun aTreeWithLiteralsArgumentsAndARedirectRoundTrips() {
        // The shape /teleport <targets> <location>, plus /tp redirecting onto it: the redirect is
        // how vanilla declares the alias, so it has to survive the wire.
        val packet =
            CommandsPacket(
                nodes =
                    listOf(
                        CommandNodeData(CommandNodeData.Kind.ROOT, executable = false, children = intArrayOf(1, 4)),
                        CommandNodeData(
                            CommandNodeData.Kind.LITERAL,
                            executable = false,
                            children = intArrayOf(2),
                            name = "teleport",
                        ),
                        CommandNodeData(
                            CommandNodeData.Kind.ARGUMENT,
                            executable = false,
                            children = intArrayOf(3),
                            name = "targets",
                            parser = ArgumentParser.Entity(single = false, playersOnly = false),
                        ),
                        CommandNodeData(
                            CommandNodeData.Kind.ARGUMENT,
                            executable = true,
                            children = intArrayOf(),
                            name = "location",
                            parser = ArgumentParser.Vec3,
                        ),
                        CommandNodeData(
                            CommandNodeData.Kind.LITERAL,
                            executable = false,
                            children = intArrayOf(),
                            redirect = 1,
                            name = "tp",
                        ),
                    ),
                rootIndex = 0,
            )
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, CommandsPacket.read(buffer))
    }
}
