package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.ComponentJson
import org.basaltmc.wire.text.ComponentNbt
import org.basaltmc.wire.text.PlainTextSerializer

/**
 * Says what an entity data entry is, and where it sits, in the numbers the far end uses. Both have
 * moved between versions, and a wrong kind is not a field the client ignores but a packet it
 * misreads. What a version has no kind for is left out.
 */
internal class EntityDataRewriter(
    private val farEnd: Int,
    /** The older side of every link, newest first: an index is moved once per step, in order. */
    private val links: List<Int>,
    private val stack: (WireItemStack?) -> WireItemStack?,
) {
    private val types: IntArray? = ENTITY_DATA_TYPES[farEnd]

    /** 1.8 writes a kind and an index in one byte; everything after it writes them apart. */
    private val packed: Boolean = farEnd < ProtocolVersions.V1_9

    /** Null when nothing has to change. [nativeType] is -1 for an entity nothing said the type of. */
    fun rewrite(
        data: SetEntityDataPacket,
        nativeType: Int,
    ): ByteArray? {
        val types = types ?: return null
        val output = ByteBuffer()
        output.writeVarInt(data.entityId)
        for (entry in data.entries) {
            val index = indexFor(nativeType, entry.index)
            if (index >= 0) write(output, entry, index, types)
        }
        // 1.8 ends its list with a different byte, because the one it uses for an entry is the type
        // and the index packed together and 0xFF is a real pair there.
        output.writeByte(if (packed) END_OF_PACKED_METADATA else END_OF_METADATA)
        return output.toByteArray()
    }

    /** Where an entry sits in the far end's own list, or -1 for one it does not keep. Walked one link at a time. */
    private fun indexFor(
        nativeType: Int,
        nativeIndex: Int,
    ): Int {
        var index = nativeIndex
        for (link in links) {
            for (rule in ENTITY_DATA_INDEX_RULES[link].orEmpty()) {
                if (nativeType < 0 || !rule.types.contains(nativeType)) continue
                if (index == rule.index) return NONE
                if (rule.shift && index > rule.index) index--
            }
            // A link that lists what it keeps rather than what it lost drops everything else.
            val moves = ENTITY_DATA_INDEX_MOVES[link] ?: continue
            if (nativeType < 0) return NONE
            index =
                moves
                    .firstOrNull { it.from == index && it.types.contains(nativeType) }
                    ?.to ?: return NONE
        }
        return index
    }

    private fun write(
        output: ByteBuffer,
        entry: SetEntityDataPacket.Entry,
        index: Int,
        types: IntArray,
    ) {
        val native = nativeType(entry)
        var mapped = types.getOrElse(native) { NONE }
        // A name is a string of its own below 1.13, which is where the optional kind starts.
        val asLegacyName = mapped == NONE && entry is SetEntityDataPacket.Entry.OptionalText
        if (asLegacyName) mapped = types.getOrElse(STRING) { NONE }
        // A version without a kind for a boolean says it with a byte, which is what it always was.
        val asByte = mapped == NONE && entry is SetEntityDataPacket.Entry.Booleans
        if (asByte) mapped = types.getOrElse(BYTE) { NONE }
        if (mapped == NONE) return
        val payload = ByteBuffer()
        if (!payload(payload, entry, asLegacyName, asByte)) return
        if (packed) {
            // 1.8 puts the kind in the top three bits of the same byte as the index.
            if (index > PACKED_INDEX_MASK || mapped > PACKED_TYPE_LIMIT) return
            output.writeByte(((mapped shl PACKED_TYPE_SHIFT) or index).toByte())
        } else {
            output.writeByte(index.toByte())
            output.writeVarInt(mapped)
        }
        output.writeBytes(payload.toByteArray())
    }

    /** False for an entry this build cannot put in that version's shape, which is left out. */
    private fun payload(
        output: ByteBuffer,
        entry: SetEntityDataPacket.Entry,
        asLegacyName: Boolean,
        asByte: Boolean,
    ): Boolean {
        when (entry) {
            is SetEntityDataPacket.Entry.Bytes -> output.writeByte(entry.value)
            is SetEntityDataPacket.Entry.Ints -> output.writeVarInt(entry.value)
            is SetEntityDataPacket.Entry.Floats -> output.writeFloat(entry.value)
            is SetEntityDataPacket.Entry.Booleans ->
                if (asByte) output.writeByte(if (entry.value) 1 else 0) else output.writeBoolean(entry.value)
            is SetEntityDataPacket.Entry.Rotations -> {
                output.writeFloat(entry.x)
                output.writeFloat(entry.y)
                output.writeFloat(entry.z)
            }
            is SetEntityDataPacket.Entry.Text -> writeComponent(output, entry.value, asLegacyName)
            is SetEntityDataPacket.Entry.OptionalText -> {
                if (asLegacyName) {
                    output.writeProtocolString(entry.value?.let { PlainTextSerializer.serialize(it) } ?: "")
                } else {
                    output.writeBoolean(entry.value != null)
                    entry.value?.let { writeComponent(output, it, false) }
                }
            }
            is SetEntityDataPacket.Entry.Poses -> output.writeVarInt(entry.pose.ordinal)
            is SetEntityDataPacket.Entry.VillagerData -> {
                output.writeVarInt(entry.typeId)
                output.writeVarInt(entry.professionId)
                output.writeVarInt(entry.level)
            }
            is SetEntityDataPacket.Entry.RegistryId -> output.writeVarInt(entry.value)
            // A stack is written the way its own version writes one, which is three shapes over
            // the years and the same work wherever a stack travels.
            is SetEntityDataPacket.Entry.Item -> LegacyStacks.write(output, stack(entry.stack), farEnd)
            is SetEntityDataPacket.Entry.OptionalBlockPos -> {
                // The packing of a position changed in 1.14, and this only ever carries a bed.
                if (farEnd < ProtocolVersions.V1_14) return false
                output.writeBoolean(entry.pos != null)
                entry.pos?.let { output.writeLong(it.pack()) }
            }
        }
        return true
    }

    /** A component travels as NBT from 1.20.3, as a string of json below it, as plain text where
     * the field is a string at all. */
    private fun writeComponent(
        output: ByteBuffer,
        component: org.basaltmc.wire.text.Component,
        asLegacyName: Boolean,
    ) {
        when {
            asLegacyName -> output.writeProtocolString(PlainTextSerializer.serialize(component))
            farEnd >= ProtocolVersions.V1_20_3 -> output.writeAnonymousNbt(ComponentNbt.serialize(component))
            else -> output.writeProtocolString(ComponentJson.serialize(component).toString())
        }
    }

    private fun nativeType(entry: SetEntityDataPacket.Entry): Int =
        when (entry) {
            is SetEntityDataPacket.Entry.Bytes -> BYTE
            is SetEntityDataPacket.Entry.Ints -> VAR_INT
            is SetEntityDataPacket.Entry.Floats -> FLOAT
            is SetEntityDataPacket.Entry.Text -> COMPONENT
            is SetEntityDataPacket.Entry.OptionalText -> OPTIONAL_COMPONENT
            is SetEntityDataPacket.Entry.Item -> ITEM
            is SetEntityDataPacket.Entry.Booleans -> BOOLEAN
            is SetEntityDataPacket.Entry.Rotations -> ROTATIONS
            is SetEntityDataPacket.Entry.OptionalBlockPos -> OPTIONAL_BLOCK_POSITION
            is SetEntityDataPacket.Entry.VillagerData -> VILLAGER_DATA
            is SetEntityDataPacket.Entry.Poses -> POSE
            is SetEntityDataPacket.Entry.RegistryId -> entry.registry.ordinal + CAT_VARIANT
        }

    private companion object {
        const val NONE = -1
        const val END_OF_METADATA = 0xFF.toByte()
        const val END_OF_PACKED_METADATA = 0x7F.toByte()
        const val PACKED_TYPE_SHIFT = 5
        const val PACKED_INDEX_MASK = 0x1F
        const val PACKED_TYPE_LIMIT = 7

        // This build's own numbering, which is what the generated table is indexed by.
        const val BYTE = 0
        const val VAR_INT = 1
        const val FLOAT = 3
        const val STRING = 4
        const val COMPONENT = 5
        const val OPTIONAL_COMPONENT = 6
        const val ITEM = 7
        const val BOOLEAN = 8
        const val ROTATIONS = 9
        const val OPTIONAL_BLOCK_POSITION = 11
        const val VILLAGER_DATA = 18
        const val POSE = 20
        const val CAT_VARIANT = 21
    }
}
