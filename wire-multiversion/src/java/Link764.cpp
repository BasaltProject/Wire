#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/java/Link764.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

void writeJson(io::ByteBuffer& output, const text::Component& component) {
    protocol::writeString(output, common::writeJson(text::componentJson::serialize(component)));
}

} // namespace

bool Link764::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::SystemChatPacket::kId: {
        const auto chat = decode(packet, [](io::ByteBuffer& in) { return play::SystemChatPacket::read(in); });
        if (!chat.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            writeJson(output, chat->content);
            protocol::writeBoolean(output, chat->isActionBar);
        });
        break;
    }
    case play::kSetTitleTextId:
    case play::kSetSubtitleTextId:
    case play::kSetActionBarTextId: {
        const auto title = decode(packet, [](io::ByteBuffer& in) { return play::TitleTextPacket::read(in); });
        if (!title.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) { writeJson(out, title->text); });
        break;
    }
    case play::PlayDisconnectPacket::kId: {
        const auto disconnect = decode(packet, [](io::ByteBuffer& in) { return play::PlayDisconnectPacket::read(in); });
        if (!disconnect.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) { writeJson(out, disconnect->reason); });
        break;
    }
    case play::TabListPacket::kId: {
        const auto list = decode(packet, [](io::ByteBuffer& in) { return play::TabListPacket::read(in); });
        if (!list.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            writeJson(output, list->header);
            writeJson(output, list->footer);
        });
        break;
    }
    case play::OpenScreenPacket::kId: {
        const auto screen = decode(packet, [](io::ByteBuffer& in) { return play::OpenScreenPacket::read(in); });
        if (!screen.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, screen->containerId);
            protocol::varInt::write(output, screen->menuType);
            writeJson(output, screen->title);
        });
        break;
    }
    case play::BossEventPacket::kId:
    case play::SetObjectivePacket::kId:
    case play::SetPlayerTeamPacket::kId:
        return false;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
