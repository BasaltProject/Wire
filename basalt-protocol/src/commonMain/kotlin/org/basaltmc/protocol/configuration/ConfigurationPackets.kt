package org.basaltmc.protocol.configuration

import org.basaltmc.io.ByteBuffer
import org.basaltmc.nbt.NbtTag
import org.basaltmc.protocol.readAnonymousNbt
import org.basaltmc.protocol.readBoolean
import org.basaltmc.protocol.readProtocolString
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeAnonymousNbt
import org.basaltmc.protocol.writeBoolean
import org.basaltmc.protocol.writeProtocolString
import org.basaltmc.protocol.writeVarInt

/** Serverbound, id 0x00: client settings. Mirrors `ServerboundClientInformationPacket`. */
public data class ClientInformationPacket(
    val locale: String,
    val viewDistance: Byte,
    val chatFlags: Int,
    val chatColors: Boolean,
    val skinParts: Byte,
    val mainHand: Int,
    val enableTextFiltering: Boolean,
    val enableServerListing: Boolean,
    val particleStatus: Int,
) {
    public companion object {
        public const val ID: Int = 0

        public fun read(buffer: ByteBuffer): ClientInformationPacket =
            ClientInformationPacket(
                locale = buffer.readProtocolString(),
                viewDistance = buffer.readByte(),
                chatFlags = buffer.readVarInt(),
                chatColors = buffer.readBoolean(),
                skinParts = buffer.readByte(),
                mainHand = buffer.readVarInt(),
                enableTextFiltering = buffer.readBoolean(),
                enableServerListing = buffer.readBoolean(),
                particleStatus = buffer.readVarInt(),
            )
    }
}

/** One entry of a [SelectKnownPacksPacket]: a data pack identity (namespace, id, version). */
public data class KnownPack(
    val namespace: String,
    val id: String,
    val version: String,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(namespace)
        buffer.writeProtocolString(id)
        buffer.writeProtocolString(version)
    }

    public companion object {
        public fun read(buffer: ByteBuffer): KnownPack =
            KnownPack(buffer.readProtocolString(), buffer.readProtocolString(), buffer.readProtocolString())
    }
}

/**
 * Clientbound id 0x0e / serverbound id 0x07: negotiates which data packs both ends already know,
 * so [RegistryDataPacket] can omit entries the client already has bundled. Mirrors
 * `(Clientbound|Serverbound)SelectKnownPacksPacket`.
 *
 * Basalt sends an empty pack list: it has no datapacks of its own yet, so every registry
 * entry it sends must carry its full value.
 */
public data class SelectKnownPacksPacket(
    val packs: List<KnownPack>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(packs.size)
        for (pack in packs) pack.write(buffer)
    }

    public companion object {
        public const val CLIENTBOUND_ID: Int = 0x0e
        public const val SERVERBOUND_ID: Int = 0x07

        public fun read(buffer: ByteBuffer): SelectKnownPacksPacket {
            val count = buffer.readVarInt()
            return SelectKnownPacksPacket(List(count) { KnownPack.read(buffer) })
        }
    }
}

/** One entry of a [RegistryDataPacket]: an identifier and its value (absent to defer to the client's own copy). */
public data class RegistryEntry(
    val key: String,
    val value: NbtTag?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(key)
        buffer.writeBoolean(value != null)
        value?.let { buffer.writeAnonymousNbt(it) }
    }

    public companion object {
        public fun read(buffer: ByteBuffer): RegistryEntry {
            val key = buffer.readProtocolString()
            val value = if (buffer.readBoolean()) buffer.readAnonymousNbt() else null
            return RegistryEntry(key, value)
        }
    }
}

/** Clientbound, id 0x07: one registry's worth of entries. Mirrors `ClientboundRegistryDataPacket`. */
public data class RegistryDataPacket(
    val registryId: String,
    val entries: List<RegistryEntry>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(registryId)
        buffer.writeVarInt(entries.size)
        for (entry in entries) entry.write(buffer)
    }

    public companion object {
        public const val ID: Int = 7

        public fun read(buffer: ByteBuffer): RegistryDataPacket {
            val registryId = buffer.readProtocolString()
            val count = buffer.readVarInt()
            return RegistryDataPacket(registryId, List(count) { RegistryEntry.read(buffer) })
        }
    }
}

/** One named tag within a [TagGroup]: [ids] are numeric registry state/entry IDs, not names. */
public data class TagEntry(
    val name: String,
    val ids: List<Int>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(name)
        buffer.writeVarInt(ids.size)
        for (id in ids) buffer.writeVarInt(id)
    }

    public companion object {
        public fun read(buffer: ByteBuffer): TagEntry {
            val name = buffer.readProtocolString()
            val count = buffer.readVarInt()
            return TagEntry(name, List(count) { buffer.readVarInt() })
        }
    }
}

/** Every tag defined for one registry (`registryId`, matching [RegistryDataPacket]'s id format). */
public data class TagGroup(
    val registryId: String,
    val tags: List<TagEntry>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(registryId)
        buffer.writeVarInt(tags.size)
        for (tag in tags) tag.write(buffer)
    }

    public companion object {
        public fun read(buffer: ByteBuffer): TagGroup {
            val registryId = buffer.readProtocolString()
            val count = buffer.readVarInt()
            return TagGroup(registryId, List(count) { TagEntry.read(buffer) })
        }
    }
}

/** Clientbound, id 0x0d: every tag Basalt defines, grouped by registry. Mirrors `ClientboundUpdateTagsPacket`. */
public data class TagsPacket(
    val groups: List<TagGroup>,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(groups.size)
        for (group in groups) group.write(buffer)
    }

    public companion object {
        public const val ID: Int = 0x0d

        public fun read(buffer: ByteBuffer): TagsPacket {
            val count = buffer.readVarInt()
            return TagsPacket(List(count) { TagGroup.read(buffer) })
        }
    }
}

/** Both directions, id 0x03: ends Configuration. Empty body. Mirrors `(Clientbound|Serverbound)FinishConfigurationPacket`. */
public object FinishConfigurationPacket {
    public const val ID: Int = 3
}

/** Both directions, id 0x04 in Configuration: mirrors `ClientboundConfigurationKeepAlivePacket`/its serverbound reply. */
public data class ConfigurationKeepAlivePacket(
    val keepAliveId: Long,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(keepAliveId)
    }

    public companion object {
        public const val ID: Int = 4

        public fun read(buffer: ByteBuffer): ConfigurationKeepAlivePacket =
            ConfigurationKeepAlivePacket(buffer.readLong())
    }
}
