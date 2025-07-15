package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.PacketIdTables
import org.basaltmc.wire.multiversion.ProtocolLink
import org.basaltmc.wire.multiversion.RegistryRemovals
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState

/**
 * A link between two versions that differ only in what things are called.
 *
 * That is most of the modern range: Mojang inserts names into registries and reshuffles packet ids,
 * and the packets themselves stay as they were. The renumbering itself is not done here, it is done
 * once for the whole chain (see [org.basaltmc.wire.multiversion.ComposedTables]); this class exists
 * to carry the id tables and to be the base of the links that do have a packet to reshape.
 */
internal open class RenumberingLink(
    protected val tables: VersionTables,
) : ProtocolLink {
    override val older: Int get() = tables.older

    override val packets: PacketIdTables get() = tables.packets

    override val registryRemovals: RegistryRemovals = RegistryRemovals(tables.newEntries, tables.newAttributes)

    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean = true

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean = true

    /** Reads the packet's body without changing it. */
    protected inline fun <T> decode(
        packet: MutablePacket,
        read: (ByteBuffer) -> T,
    ): T = read(ByteBuffer().apply { writeBytes(packet.body) })

    /** Replaces the packet's body with whatever [write] puts in the buffer. */
    protected inline fun encode(
        packet: MutablePacket,
        write: (ByteBuffer) -> Unit,
    ) {
        val output = ByteBuffer()
        write(output)
        packet.body = output.toByteArray()
    }

    /** Decodes, lets [block] write the older shape, and puts the result back in the packet. */
    protected inline fun rewrite(
        packet: MutablePacket,
        block: (ByteBuffer, ByteBuffer) -> Unit,
    ) {
        val input = ByteBuffer().apply { writeBytes(packet.body) }
        val output = ByteBuffer()
        block(input, output)
        if (output.readableBytes > 0) packet.body = output.toByteArray()
    }
}
