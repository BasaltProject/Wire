package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.CompactBitStorage
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtLongArray
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.BitStorage
import org.basaltmc.wire.protocol.play.InteractPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerAbilitiesPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.16 to 1.15.2, where a dimension is a number and there are only three of them.
 *
 * Everything 1.16 added to describe the world goes away here: the list of worlds, the registry of
 * dimension types and the name of the one being joined, replaced by the number 1.15.2 knows
 * (the nether is -1, the overworld 0, the end 1) and by the word it calls the world generator.
 *
 * The rest is fields 1.16 added and one packet it merged: equipment arrives as every slot at once
 * and leaves as one packet per slot, which is what this client reads.
 */
internal class Link578(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            LoginPlayPacket.ID -> rewriteLogin(packet)
            RespawnPacket.ID -> rewriteRespawn(packet)
            SystemChatPacket.ID -> dropSender(packet)
            MapChunkPacket.ID -> repackChunk(packet)
            UpdateLightPacket.ID -> dropTrustEdges(packet)
            SetEquipmentPacket.ID -> splitEquipment(packet)
            UpdateAttributesPacket.ID -> renameAttributes(packet)
        }
        return true
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            InteractPacket.ID -> {
                // 1.16 asked whether the player was sneaking when they clicked. This one does not say.
                val body = packet.body
                encode(packet) { output ->
                    output.writeBytes(body)
                    output.writeByte(0)
                }
            }
            ServerboundPlayerAbilitiesPacket.ID -> {
                // Two speeds the client had no business sending, and 1.16 stopped asking for.
                val flags = decode(packet) { it.readByte() }
                encode(packet) { it.writeByte(flags) }
            }
        }
        return true
    }

    private fun rewriteLogin(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readInt()
        val gamemode = input.readByte()
        input.readByte()
        repeat(input.readVarInt()) { input.readProtocolString() }
        input.readAnonymousNbt()
        val dimension = input.readProtocolString()
        input.readProtocolString()
        val hashedSeed = input.readLong()
        val maxPlayers = input.readByte()
        val viewDistance = input.readVarInt()
        val reducedDebugInfo = input.readByte()
        val respawnScreen = input.readByte()
        input.readByte()
        val flat = input.readByte().toInt() != 0
        encode(packet) { output ->
            output.writeInt(entityId)
            output.writeByte(gamemode)
            output.writeInt(dimensionNumber(dimension))
            output.writeLong(hashedSeed)
            output.writeByte(maxPlayers)
            output.writeProtocolString(levelType(flat))
            output.writeVarInt(viewDistance)
            output.writeByte(reducedDebugInfo)
            output.writeByte(respawnScreen)
        }
    }

    private fun rewriteRespawn(packet: MutablePacket) {
        val input = decode(packet) { it }
        val dimension = input.readProtocolString()
        input.readProtocolString()
        val hashedSeed = input.readLong()
        val gamemode = input.readByte()
        input.readByte()
        input.readByte()
        val flat = input.readByte().toInt() != 0
        encode(packet) { output ->
            output.writeInt(dimensionNumber(dimension))
            output.writeLong(hashedSeed)
            output.writeByte(gamemode)
            output.writeProtocolString(levelType(flat))
        }
    }

    /** 1.16 started saying who said it, which this client does not read. */
    private fun dropSender(packet: MutablePacket) {
        val input = decode(packet) { it }
        val json = input.readProtocolString(MAX_TEXT)
        val position = input.readByte()
        encode(packet) { output ->
            output.writeProtocolString(json, MAX_TEXT)
            output.writeByte(position)
        }
    }

    /**
     * The chunk, packed the way everything below 1.16 reads it.
     *
     * 1.16 stopped letting a value span two longs, in the sections and in the heightmaps alike.
     * Every number is the same; only where its bits sit changes. The flag 1.16 added about old
     * light goes with it.
     */
    private fun repackChunk(packet: MutablePacket) {
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val fullChunk = input.readByte()
        input.readByte()
        val mask = input.readVarInt()
        val heightmaps = input.readAnonymousNbt()
        // 1.16 writes the biomes as a fixed thousand and twenty four, and so does this one.
        val biomes = IntArray(BIOMES_IN_CHUNK) { input.readInt() }
        val sections = ByteBuffer().apply { writeBytes(input.readBytes(input.readVarInt())) }
        val rest = input.readBytes(input.readableBytes)

        val packed = ByteBuffer()
        for (index in 0 until SECTIONS) {
            if (mask and (1 shl index) == 0) continue
            packed.writeShort(sections.readShort())
            repackContainer(sections, packed)
        }
        val data = packed.toByteArray()
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeByte(fullChunk)
            output.writeVarInt(mask)
            output.writeAnonymousNbt(compactHeightmaps(heightmaps))
            for (biome in biomes) output.writeInt(biome)
            output.writeVarInt(data.size)
            output.writeBytes(data)
            output.writeBytes(rest)
        }
    }

    private fun repackContainer(
        input: ByteBuffer,
        output: ByteBuffer,
    ) {
        val bits = input.readByte().toInt() and 0xFF
        output.writeByte(bits.toByte())
        if (bits <= BLOCK_MAX_INDIRECT_BITS) {
            val entries = input.readVarInt()
            output.writeVarInt(entries)
            repeat(entries) { output.writeVarInt(input.readVarInt()) }
        }
        val words = LongArray(input.readVarInt()) { input.readLong() }
        val storage = BitStorage(bits, BLOCKS_PER_SECTION, words)
        val compact = CompactBitStorage.pack(IntArray(BLOCKS_PER_SECTION) { storage[it] }, bits)
        output.writeVarInt(compact.size)
        for (word in compact) output.writeLong(word)
    }

    /** The same repacking for the heightmaps, which are nine bits of height per column. */
    private fun compactHeightmaps(heightmaps: NbtTag): NbtTag {
        val compound = heightmaps as? NbtCompound ?: return heightmaps
        val rewritten = NbtCompound()
        for ((name, value) in compound) {
            val words = (value as? NbtLongArray)?.value
            if (words == null) {
                rewritten[name] = value
                continue
            }
            val storage = BitStorage(HEIGHTMAP_BITS, HEIGHTMAP_ENTRIES, words)
            rewritten[name] =
                NbtLongArray(
                    CompactBitStorage.pack(IntArray(HEIGHTMAP_ENTRIES) { storage[it] }, HEIGHTMAP_BITS),
                )
        }
        return rewritten
    }

    private fun dropTrustEdges(packet: MutablePacket) {
        val input = decode(packet) { it }
        val x = input.readVarInt()
        val z = input.readVarInt()
        input.readByte()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeVarInt(x)
            output.writeVarInt(z)
            output.writeBytes(rest)
        }
    }

    /**
     * One packet per slot, which is what a client this old reads.
     *
     * 1.16 put every slot of an entity in one packet, ending the list by leaving the top bit of a
     * slot clear. The first one stays in this packet and the rest follow it.
     */
    private fun splitEquipment(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val slots = mutableListOf<Pair<Int, ByteArray>>()
        while (true) {
            val slot = input.readByte().toInt()
            val start = input.readableBytes
            val item = itemBytes(input)
            if (start == input.readableBytes) break
            slots += (slot and SLOT_MASK) to item
            if (slot and MORE_SLOTS == 0) break
        }
        if (slots.isEmpty()) return
        encode(packet) { output ->
            output.writeVarInt(entityId)
            output.writeVarInt(slots[0].first)
            output.writeBytes(slots[0].second)
        }
        for (index in 1 until slots.size) {
            val body =
                org.basaltmc.wire.io
                    .ByteBuffer()
                    .apply {
                        writeVarInt(entityId)
                        writeVarInt(slots[index].first)
                        writeBytes(slots[index].second)
                    }.toByteArray()
            packet.extras += tables.packets.toOlder(ProtocolState.PLAY, packet.id) to body
        }
    }

    /** A stack as everything below 1.20.5 writes it: present, and then what it is. */
    private fun itemBytes(input: org.basaltmc.wire.io.ByteBuffer): ByteArray {
        val output =
            org.basaltmc.wire.io
                .ByteBuffer()
        val present = input.readByte()
        output.writeByte(present)
        if (present.toInt() == 0) return output.toByteArray()
        output.writeVarInt(input.readVarInt())
        output.writeByte(input.readByte())
        output.writeAnonymousNbt(input.readAnonymousNbt())
        return output.toByteArray()
    }

    /** parity: `Attributes` was renamed wholesale in 1.16, and this client knows the old spelling. */
    private fun renameAttributes(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val count = input.readInt()
        val entries =
            List(count) {
                val name = input.readProtocolString()
                val base = input.readDouble()
                val modifiers = input.readBytes(input.readVarInt() * MODIFIER_BYTES)
                Triple(name, base, modifiers)
            }
        encode(packet) { output ->
            output.writeVarInt(entityId)
            output.writeInt(count)
            for ((name, base, modifiers) in entries) {
                output.writeProtocolString(LEGACY_NAMES[name] ?: name)
                output.writeDouble(base)
                output.writeVarInt(modifiers.size / MODIFIER_BYTES)
                output.writeBytes(modifiers)
            }
        }
    }

    private fun dimensionNumber(name: String): Int =
        when (name) {
            NETHER -> -1
            THE_END -> 1
            else -> 0
        }

    private fun levelType(flat: Boolean): String = if (flat) "flat" else "default"

    private companion object {
        const val NETHER = "minecraft:the_nether"
        const val THE_END = "minecraft:the_end"
        const val MAX_TEXT = 262144

        const val SECTIONS = 16
        const val BIOMES_IN_CHUNK = 1024
        const val BLOCKS_PER_SECTION = 16 * 16 * 16
        const val BLOCK_MAX_INDIRECT_BITS = 8
        const val HEIGHTMAP_BITS = 9
        const val HEIGHTMAP_ENTRIES = 256

        const val SLOT_MASK = 0x7F
        const val MORE_SLOTS = 0x80

        /** A uuid, an amount and an operation, which do not change shape here. */
        const val MODIFIER_BYTES = 16 + 8 + 1

        /** parity: the keys of `Attributes` before 1.16 gave them a namespace and snake case. */
        val LEGACY_NAMES =
            mapOf(
                "minecraft:generic.max_health" to "generic.maxHealth",
                "minecraft:generic.follow_range" to "generic.followRange",
                "minecraft:generic.knockback_resistance" to "generic.knockbackResistance",
                "minecraft:generic.movement_speed" to "generic.movementSpeed",
                "minecraft:generic.flying_speed" to "generic.flyingSpeed",
                "minecraft:generic.attack_damage" to "generic.attackDamage",
                "minecraft:generic.attack_knockback" to "generic.attackKnockback",
                "minecraft:generic.attack_speed" to "generic.attackSpeed",
                "minecraft:generic.armor" to "generic.armor",
                "minecraft:generic.armor_toughness" to "generic.armorToughness",
                "minecraft:generic.luck" to "generic.luck",
                "minecraft:horse.jump_strength" to "horse.jumpStrength",
                "minecraft:zombie.spawn_reinforcements" to "zombie.spawnReinforcements",
            )
    }
}
