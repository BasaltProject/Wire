#include <basalt/wire/multiversion/java/Link759Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kChatPreview = 12;
constexpr std::int32_t kPlayerChat = 48;
constexpr std::int32_t kServerData = 63;
constexpr std::int32_t kShouldDisplayChatPreview = 75;
constexpr std::int32_t kSystemChat = 95;

constexpr std::int32_t kChatCommand = 3;
constexpr std::int32_t kChatMessage = 4;
constexpr std::int32_t kChatPreviewRequest = 5;

constexpr std::int32_t kGameInfoType = 2;

void rewritePlayerChat(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::string signedContent = pv::readString(in);
    const bool hasUnsigned = pv::readBoolean(in);
    std::string unsignedContent;
    if (hasUnsigned) {
        unsignedContent = pv::readString(in);
    }
    const std::int32_t type = pv::varInt::read(in);
    const auto sender = in.readSpan(16);
    const std::string name = pv::readString(in);
    const bool hasTeam = pv::readBoolean(in);
    std::string team;
    if (hasTeam) {
        team = pv::readString(in);
    }
    in.skip(16);
    in.skip(static_cast<std::size_t>(pv::readCollectionSize(in)));
    if (in.failed()) {
        return;
    }
    pv::writeBoolean(out, false);
    out.writeBytes(sender);
    pv::varInt::write(out, 0);
    pv::writeString(out, std::string{});
    pv::writeBoolean(out, false);
    out.writeBytes(std::vector<std::uint8_t>(16, 0));
    pv::varInt::write(out, 0);
    pv::writeBoolean(out, true);
    pv::writeString(out, hasUnsigned ? unsignedContent : signedContent);
    pv::varInt::write(out, 0);
    pv::varInt::write(out, type);
    pv::writeString(out, name);
    pv::writeBoolean(out, hasTeam);
    if (hasTeam) {
        pv::writeString(out, team);
    }
}

} // namespace

bool Link759Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kChatPreview:
    case kShouldDisplayChatPreview:
        return false;
    case kPlayerChat:
        rewrite(packet, rewritePlayerChat);
        break;
    case kSystemChat:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            pv::writeString(out, pv::readString(in));
            pv::writeBoolean(out, pv::varInt::read(in) == kGameInfoType);
        });
        break;
    case kServerData:
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            for (int part = 0; part < 2; ++part) {
                const bool present = pv::readBoolean(in);
                pv::writeBoolean(out, present);
                if (present) {
                    pv::writeString(out, pv::readString(in));
                }
            }
            out.writeBytes(in.readable());
            pv::writeBoolean(out, false);
        });
        break;
    default:
        break;
    }
    return true;
}

bool Link759Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kChatPreviewRequest:
        return false;
    case kChatMessage:
    case kChatCommand:
        if (packet.body.size() < 2) {
            return false;
        }
        packet.body.resize(packet.body.size() - 2);
        break;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
