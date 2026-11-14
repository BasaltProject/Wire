#include <basalt/wire/multiversion/java/ChatShapes.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>

#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java::chatShapes {
namespace {

namespace play = protocol::play;

/// The 1.18.2 chat id, and the ids 1.19 gives a command and a message.
constexpr std::int32_t kChat1_18 = 0x03;
constexpr std::int32_t kCommand1_19 = 0x03;
constexpr std::int32_t kChat1_19 = 0x04;

constexpr std::size_t kSeenBitSetBytes = 3;
constexpr std::size_t kUuidBytes = 16;
constexpr std::size_t kSignatureBytes = 256;
/// parity: FilterMask.Type.PARTIALLY_FILTERED, the one followed by its bit set.
constexpr std::int32_t kPartiallyFiltered = 2;

[[nodiscard]] bool isChat(const MutablePacket& packet) { return packet.nativeId() == play::ChatMessagePacket::kId; }

[[nodiscard]] bool isCommand(const MutablePacket& packet) { return packet.nativeId() == play::ChatCommandPacket::kId; }

template <typename Body>
void rebuild(MutablePacket& packet, Body&& body) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    body(input, output);
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

void skipBytes(io::ByteBuffer& input) {
    const std::int32_t length = protocol::varInt::read(input);
    input.skip(static_cast<std::size_t>(length < 0 ? 0 : length));
}

} // namespace

bool from1_18(MutablePacket& packet) {
    if (packet.id != kChat1_18) {
        return false;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string text = protocol::readString(input);
    if (input.failed()) {
        return false;
    }
    io::ByteBuffer output;
    const bool command = !text.empty() && text.front() == '/';
    protocol::writeString(output, command ? text.substr(1) : text);
    output.writeLong(0);
    output.writeLong(0);
    protocol::varInt::write(output, 0);
    protocol::writeBoolean(output, false);
    packet.id = command ? kCommand1_19 : kChat1_19;
    packet.body = output.toVector();
    return true;
}

void to1_19_1(MutablePacket& packet) {
    if (!isChat(packet) && !isCommand(packet)) {
        return;
    }
    io::ByteBuffer output;
    output.writeBytes(packet.body);
    protocol::varInt::write(output, 0);
    protocol::writeBoolean(output, false);
    packet.body = output.toVector();
}

void to1_19_3(MutablePacket& packet) {
    if (!isChat(packet) && !isCommand(packet)) {
        return;
    }
    const bool command = isCommand(packet);
    rebuild(packet, [command](io::ByteBuffer& input, io::ByteBuffer& output) {
        protocol::writeString(output, protocol::readString(input));
        output.writeLong(input.readLong());
        output.writeLong(input.readLong());
        if (command) {
            const std::int32_t arguments = protocol::varInt::read(input);
            for (std::int32_t i = 0; i < arguments && !input.failed(); ++i) {
                static_cast<void>(protocol::readString(input));
                skipBytes(input);
            }
            protocol::varInt::write(output, 0);
        } else {
            skipBytes(input);
            protocol::writeBoolean(output, false);
        }
        protocol::varInt::write(output, 0);
        for (std::size_t i = 0; i < kSeenBitSetBytes; ++i) {
            output.writeByte(0);
        }
    });
}

void to1_20_5(MutablePacket& packet) {
    if (!isCommand(packet)) {
        return;
    }
    rebuild(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
        protocol::writeString(output, protocol::readString(input));
    });
}

void chatTypeAsIndex(MutablePacket& packet) {
    const auto copyText = [](io::ByteBuffer& input, io::ByteBuffer& output) {
        protocol::writeAnonymousNbt(output, protocol::readAnonymousNbt(input));
    };
    const auto copyType = [](io::ByteBuffer& input, io::ByteBuffer& output) {
        const std::int32_t holder = protocol::varInt::read(input);
        protocol::varInt::write(output, holder > 0 ? holder - 1 : 0);
    };
    const auto copyTail = [&](io::ByteBuffer& input, io::ByteBuffer& output) {
        copyType(input, output);
        copyText(input, output);
        const bool target = protocol::readBoolean(input);
        protocol::writeBoolean(output, target);
        if (target) {
            copyText(input, output);
        }
    };
    if (packet.nativeId() == play::DisguisedChatPacket::kId) {
        rebuild(packet, [&](io::ByteBuffer& input, io::ByteBuffer& output) {
            copyText(input, output);
            copyTail(input, output);
        });
        return;
    }
    if (packet.nativeId() != play::PlayerChatPacket::kId) {
        return;
    }
    rebuild(packet, [&](io::ByteBuffer& input, io::ByteBuffer& output) {
        output.writeBytes(input.readSpan(kUuidBytes));
        protocol::varInt::write(output, protocol::varInt::read(input));
        const bool signature = protocol::readBoolean(input);
        protocol::writeBoolean(output, signature);
        if (signature) {
            output.writeBytes(input.readSpan(kSignatureBytes));
        }
        protocol::writeString(output, protocol::readString(input));
        output.writeBytes(input.readSpan(2 * sizeof(std::int64_t)));
        const std::int32_t previous = protocol::varInt::read(input);
        protocol::varInt::write(output, previous);
        for (std::int32_t i = 0; i < previous && !input.failed(); ++i) {
            const std::int32_t id = protocol::varInt::read(input);
            protocol::varInt::write(output, id);
            if (id == 0) {
                output.writeBytes(input.readSpan(kSignatureBytes));
            }
        }
        const bool unsignedContent = protocol::readBoolean(input);
        protocol::writeBoolean(output, unsignedContent);
        if (unsignedContent) {
            copyText(input, output);
        }
        const std::int32_t filter = protocol::varInt::read(input);
        protocol::varInt::write(output, filter);
        if (filter == kPartiallyFiltered) {
            const std::int32_t words = protocol::varInt::read(input);
            protocol::varInt::write(output, words);
            output.writeBytes(input.readSpan(static_cast<std::size_t>(words < 0 ? 0 : words) * sizeof(std::int64_t)));
        }
        copyTail(input, output);
    });
}

void to1_21_5(MutablePacket& packet) {
    if (!isChat(packet) && packet.nativeId() != play::SignedChatCommandPacket::kId) {
        return;
    }
    io::ByteBuffer output;
    output.writeBytes(packet.body);
    output.writeByte(0);
    packet.body = output.toVector();
}

} // namespace basalt::wire::multiversion::java::chatShapes
