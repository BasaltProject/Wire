#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/java/PlayerInfoActions.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <cstddef>

namespace basalt::wire::multiversion::java::playerInfoActions {
namespace {

namespace play = protocol::play;
using Update = play::PlayerInfoUpdatePacket;

constexpr std::size_t kUuidBytes = 16;
constexpr int kActions = 8;

void copyBytes(io::ByteBuffer& input, io::ByteBuffer* output) {
    const std::int32_t length = protocol::varInt::read(input);
    const std::span<const std::uint8_t> bytes = input.readSpan(static_cast<std::size_t>(length < 0 ? 0 : length));
    if (output != nullptr) {
        protocol::varInt::write(*output, length);
        output->writeBytes(bytes);
    }
}

void copyString(io::ByteBuffer& input, io::ByteBuffer* output) { copyBytes(input, output); }

/// One action's data for one entry, copied when `output` is set and skipped otherwise.
void copyAction(std::uint8_t action, io::ByteBuffer& input, io::ByteBuffer* output, bool jsonIn, bool jsonOut) {
    io::ByteBuffer scratch;
    io::ByteBuffer& out = output != nullptr ? *output : scratch;
    switch (action) {
    case Update::kAddPlayer: {
        copyString(input, output);
        const std::int32_t properties = protocol::varInt::read(input);
        protocol::varInt::write(out, properties);
        for (std::int32_t i = 0; i < properties && !input.failed(); ++i) {
            copyString(input, output);
            copyString(input, output);
            const bool signed_ = protocol::readBoolean(input);
            protocol::writeBoolean(out, signed_);
            if (signed_) {
                copyString(input, output);
            }
        }
        break;
    }
    case Update::kInitializeChat: {
        const bool present = protocol::readBoolean(input);
        protocol::writeBoolean(out, present);
        if (present) {
            out.writeBytes(input.readSpan(kUuidBytes + sizeof(std::int64_t)));
            copyBytes(input, output);
            copyBytes(input, output);
        }
        break;
    }
    case Update::kUpdateGameMode:
    case Update::kUpdateLatency:
    case Update::kUpdateListOrder:
        protocol::varInt::write(out, protocol::varInt::read(input));
        break;
    case Update::kUpdateListed:
    case Update::kUpdateHat:
        out.writeByte(input.readByte());
        break;
    case Update::kUpdateDisplayName: {
        const bool present = protocol::readBoolean(input);
        protocol::writeBoolean(out, present);
        if (present && jsonIn) {
            protocol::writeString(out, protocol::readString(input));
        } else if (present && jsonOut) {
            const text::Component name = text::componentNbt::deserialize(protocol::readAnonymousNbt(input));
            protocol::writeString(out, common::writeJson(text::componentJson::serializeLegacy(name)));
        } else if (present) {
            protocol::writeAnonymousNbt(out, protocol::readAnonymousNbt(input));
        }
        break;
    }
    default:
        input.markFailed();
        break;
    }
}

} // namespace

namespace {

void rewrite(MutablePacket& packet, std::uint8_t dropped, bool jsonIn, bool jsonOut) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::uint8_t actions = input.readByte();
    if ((actions & dropped) == 0 && !(jsonOut && (actions & Update::kUpdateDisplayName) != 0)) {
        return;
    }
    io::ByteBuffer output;
    output.writeByte(static_cast<std::uint8_t>(actions & ~dropped));
    const std::int32_t entries = protocol::varInt::read(input);
    protocol::varInt::write(output, entries);
    for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
        output.writeBytes(input.readSpan(kUuidBytes));
        for (int bit = 0; bit < kActions; ++bit) {
            const auto action = static_cast<std::uint8_t>(1U << bit);
            if ((actions & action) != 0) {
                copyAction(action, input, (dropped & action) != 0 ? nullptr : &output, jsonIn, jsonOut);
            }
        }
    }
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

} // namespace

void drop(MutablePacket& packet, std::uint8_t dropped, bool jsonNames) { rewrite(packet, dropped, jsonNames, false); }

void displayNamesAsJson(MutablePacket& packet) { rewrite(packet, 0, false, true); }

} // namespace basalt::wire::multiversion::java::playerInfoActions
