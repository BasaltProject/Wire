package org.basaltmc.wire.protocol

/**
 * The five connection states of the Minecraft protocol, confirmed against the decompiled 26.2
 * server jar (`net.minecraft.network.ConnectionProtocol`). Configuration/Play packet tables are
 * not implemented yet: a connection cannot progress past `ServerboundLoginAcknowledgedPacket`
 * in this pass.
 */
public enum class ProtocolState {
    HANDSHAKE,
    STATUS,
    LOGIN,
    CONFIGURATION,
    PLAY,
}
