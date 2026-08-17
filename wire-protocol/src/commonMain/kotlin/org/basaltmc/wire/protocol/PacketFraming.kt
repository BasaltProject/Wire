package org.basaltmc.wire.protocol

import org.basaltmc.wire.io.ByteBuffer

/**
 * The play-protocol wire framing, confirmed against the decompiled 26.2
 * `net.minecraft.network.CompressionEncoder`/`CompressionDecoder`:
 *
 * Uncompressed: `VarInt packetLength, [packet id + data]`.
 *
 * Compressed (once a threshold is negotiated via `ClientboundLoginCompressionPacket`):
 * `VarInt packetLength, VarInt dataLength, payload`: `dataLength = 0` and an uncompressed
 * `payload` when the packet is smaller than the threshold, otherwise `dataLength` is the
 * uncompressed size and `payload` is zlib-deflated.
 */
public object PacketFraming {
    public const val MAX_UNCOMPRESSED_SIZE: Int = 8 * 1024 * 1024

    public fun writeUncompressed(
        output: ByteBuffer,
        packetIdAndData: ByteArray,
    ) {
        require(packetIdAndData.size <= MAX_UNCOMPRESSED_SIZE) { "Packet too big: ${packetIdAndData.size}" }
        VarInt.write(output, packetIdAndData.size)
        output.writeBytes(packetIdAndData)
    }

    public fun writeCompressed(
        output: ByteBuffer,
        packetIdAndData: ByteArray,
        threshold: Int,
        compress: (ByteArray) -> ByteArray,
    ) {
        require(packetIdAndData.size <= MAX_UNCOMPRESSED_SIZE) { "Packet too big: ${packetIdAndData.size}" }
        // Framed in place: the frame length is the data-length varint plus the payload, both known
        // without staging the frame in a buffer of its own. This runs once per outbound packet.
        if (packetIdAndData.size < threshold) {
            VarInt.write(output, VarInt.sizeOf(0) + packetIdAndData.size)
            VarInt.write(output, 0)
            output.writeBytes(packetIdAndData)
        } else {
            val compressed = compress(packetIdAndData)
            VarInt.write(output, VarInt.sizeOf(packetIdAndData.size) + compressed.size)
            VarInt.write(output, packetIdAndData.size)
            output.writeBytes(compressed)
        }
    }

    /**
     * Attempts to extract one complete frame's `packet id + data` bytes from [input] (an
     * accumulating receive buffer), decompressing it via [decompress] if [compressionEnabled].
     * Returns `null` and leaves [input] untouched if a full frame is not buffered yet.
     */
    public fun tryReadFrame(
        input: ByteBuffer,
        compressionEnabled: Boolean,
        decompress: (ByteArray, Int) -> ByteArray = { _, _ -> throw IllegalStateException("no decompressor provided") },
    ): ByteArray? {
        val peeked = tryPeekVarInt(input) ?: return null
        val frameLength = peeked.value
        if (frameLength < 0) throw ProtocolException("Negative packet length: $frameLength")
        if (frameLength > MAX_UNCOMPRESSED_SIZE) throw ProtocolException("Packet too big: $frameLength")
        if (input.readableBytes < peeked.byteLength + frameLength) return null

        input.skip(peeked.byteLength)
        val frameBytes = input.readBytes(frameLength)
        if (!compressionEnabled) return frameBytes

        val frameBuffer = ByteBuffer(frameBytes.size).also { it.writeBytes(frameBytes) }
        val dataLength = frameBuffer.readVarInt()
        val payload = frameBuffer.readBytes(frameBuffer.readableBytes)
        return if (dataLength == 0) payload else decompress(payload, dataLength)
    }

    private data class PeekedVarInt(
        val value: Int,
        val byteLength: Int,
    )

    private fun tryPeekVarInt(buffer: ByteBuffer): PeekedVarInt? {
        var result = 0
        var offset = 0
        while (offset < VarInt.MAX_BYTES) {
            if (offset >= buffer.readableBytes) return null
            val current = buffer.peekByte(offset)
            result = result or ((current.toInt() and 127) shl (offset * 7))
            offset++
            if ((current.toInt() and 128) == 0) return PeekedVarInt(result, offset)
        }
        throw ProtocolException("VarInt too big")
    }
}
