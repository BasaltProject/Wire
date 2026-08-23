package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.play.AttackPacket
import org.basaltmc.wire.protocol.play.InteractPacket
import org.basaltmc.wire.protocol.play.LowPrecisionVec3
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 26.1 to 1.21.11.
 *
 * The clock becomes the overworld's alone, attacking goes back inside interacting, and a wolf's
 * sounds are flattened to the adult's, which is the only set 1.21.11 has.
 */
internal class Link774(
    tables: org.basaltmc.wire.multiversion.VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        // Renumbering first, shape second, like every other link: what is written below is no
        // longer the packet this server writes, so nothing may read it as one afterwards.
        if (!super.clientbound(state, packet)) return false
        if (state == ProtocolState.CONFIGURATION && packet.nativeId == RegistryDataPacket.ID) {
            flattenWolfSounds(packet)
            return true
        }
        if (state == ProtocolState.PLAY && packet.nativeId == SetTimePacket.ID) {
            rewrite(packet) { input, output ->
                val time = SetTimePacket.read(input)
                val clock = time.clocks.firstOrNull { it.clockId == SetTimePacket.OVERWORLD_CLOCK }
                output.writeLong(time.gameTime)
                output.writeLong(clock?.totalTicks ?: time.gameTime)
                // A rate of zero is a clock that is not running, which is what `doDaylightCycle`
                // off looks like to a client that only has the flag.
                output.writeBoolean((clock?.rate ?: 1f) != 0f)
            }
        }
        return true
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (!super.serverbound(state, packet)) return false
        if (state != ProtocolState.PLAY || packet.nativeId != InteractPacket.ID) return true
        rewrite(packet) { input, output ->
            val entityId = input.readVarInt()
            val action = input.readVarInt()
            var x = 0.0
            var y = 0.0
            var z = 0.0
            if (action == INTERACT_AT) {
                x = input.readFloat().toDouble()
                y = input.readFloat().toDouble()
                z = input.readFloat().toDouble()
            }
            val hand = if (action == ATTACK) InteractPacket.MAIN_HAND else input.readVarInt()
            val sneaking = input.readByte().toInt() != 0
            if (action == ATTACK) {
                packet.overrideId(AttackPacket.ID)
                output.writeVarInt(entityId)
                return@rewrite
            }
            output.writeVarInt(entityId)
            output.writeVarInt(hand)
            LowPrecisionVec3.write(output, x, y, z)
            output.writeBoolean(sneaking)
        }
        return true
    }

    /** A wolf's sounds, as the one flat set 1.21.11 reads. */
    private fun flattenWolfSounds(packet: MutablePacket) {
        val data = decode(packet) { RegistryDataPacket.read(it) }
        if (data.registryId != WOLF_SOUNDS) return
        val flattened =
            data.entries.map { entry ->
                val element = entry.value as? NbtCompound ?: return@map entry
                val adult = element["adult_sounds"] as? NbtCompound ?: return@map entry
                val kept = NbtCompound()
                for ((name, tag) in adult) if (name !in SOUNDS_1_21_11_LACKS) kept[name] = tag
                entry.copy(value = kept)
            }
        encode(packet) { RegistryDataPacket(data.registryId, flattened).write(it) }
    }

    private companion object {
        const val WOLF_SOUNDS = "minecraft:wolf_sound_variant"

        /** parity: `WolfSoundVariant` had no step of its own until the puppies arrived with 26.1. */
        val SOUNDS_1_21_11_LACKS = setOf("step_sound")

        /** parity: `ServerboundInteractPacket.ActionType` before 26.1 split the attack out. */
        const val ATTACK = 1
        const val INTERACT_AT = 2
    }
}
