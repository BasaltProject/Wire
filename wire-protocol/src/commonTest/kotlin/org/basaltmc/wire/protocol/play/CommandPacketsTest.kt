package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.writeVarInt
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
                ArgumentParser.BrigadierBool,
                ArgumentParser.BrigadierDouble(),
                ArgumentParser.BrigadierDouble(min = 0.0),
                ArgumentParser.BrigadierDouble(min = -1.5, max = 1.5),
                ArgumentParser.ColumnPosition,
                ArgumentParser.Vec2,
                ArgumentParser.BlockState,
                ArgumentParser.BlockPredicate,
                ArgumentParser.ItemStack,
                ArgumentParser.ItemPredicate,
                ArgumentParser.TeamColor,
                ArgumentParser.TextComponent,
                ArgumentParser.Style,
                ArgumentParser.NbtCompound,
                ArgumentParser.NbtTag,
                ArgumentParser.NbtPath,
                ArgumentParser.Objective,
                ArgumentParser.ObjectiveCriteria,
                ArgumentParser.Operation,
                ArgumentParser.Particle,
                ArgumentParser.Angle,
                ArgumentParser.Rotation,
                ArgumentParser.ScoreboardSlot,
                ArgumentParser.ScoreHolder(multiple = true),
                ArgumentParser.ScoreHolder(multiple = false),
                ArgumentParser.Swizzle,
                ArgumentParser.Team,
                ArgumentParser.ItemSlot,
                ArgumentParser.ItemSlots,
                ArgumentParser.ResourceLocation,
                ArgumentParser.EntityAnchor,
                ArgumentParser.IntRange,
                ArgumentParser.FloatRange,
                ArgumentParser.Dimension,
                ArgumentParser.Time(),
                ArgumentParser.Time(min = 1),
                ArgumentParser.Resource("minecraft:enchantment"),
                ArgumentParser.ResourceKey("minecraft:worldgen/structure"),
                ArgumentParser.Heightmap,
                ArgumentParser.LootTable,
                ArgumentParser.Dialog,
                ArgumentParser.Uuid,
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
