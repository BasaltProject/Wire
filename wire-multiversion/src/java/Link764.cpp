#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/java/Link764.hpp>
#include <basalt/wire/multiversion/java/PlayerInfoActions.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

/// parity: ClientboundServerDataPacket, numbered as this server does.
constexpr std::int32_t kServerData = 88;

constexpr std::size_t kUuidBytes = 16;
constexpr std::size_t kSignatureBytes = 256;
/// parity: FilterMask.Type.PARTIALLY_FILTERED, the one followed by its bit set.
constexpr std::int32_t kPartiallyFiltered = 2;

void writeJson(io::ByteBuffer& output, const text::Component& component) {
    protocol::writeString(output, common::writeJson(text::componentJson::serializeLegacy(component)));
}

/// Text that went as an NBT component from 1.20.3, read back and written as the json string it was before.
void nbtTextAsJson(io::ByteBuffer& input, io::ByteBuffer& output) {
    writeJson(output, text::componentNbt::deserialize(protocol::readAnonymousNbt(input)));
}

void writeChatType(io::ByteBuffer& input, io::ByteBuffer& output) {
    protocol::varInt::write(output, protocol::varInt::read(input));
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
    case play::CommandSuggestionsPacket::kId: {
        const auto suggestions =
            decode(packet, [](io::ByteBuffer& in) { return play::CommandSuggestionsPacket::read(in); });
        if (!suggestions.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) {
            protocol::varInt::write(out, suggestions->id);
            protocol::varInt::write(out, suggestions->start);
            protocol::varInt::write(out, suggestions->length);
            protocol::varInt::write(out, static_cast<std::int32_t>(suggestions->suggestions.size()));
            for (const play::CommandSuggestion& suggestion : suggestions->suggestions) {
                protocol::writeString(out, suggestion.text);
                protocol::writeBoolean(out, suggestion.tooltip.has_value());
                if (suggestion.tooltip.has_value()) {
                    writeJson(out, *suggestion.tooltip);
                }
            }
        });
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
    case play::PlayerInfoUpdatePacket::kId:
        playerInfoActions::displayNamesAsJson(packet);
        break;
    case play::PlayerCombatKillPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, protocol::varInt::read(input));
            nbtTextAsJson(input, output);
        });
        break;
    case play::PlayerChatPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
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
                nbtTextAsJson(input, output);
            }
            const std::int32_t filter = protocol::varInt::read(input);
            protocol::varInt::write(output, filter);
            if (filter == kPartiallyFiltered) {
                const std::int32_t words = protocol::varInt::read(input);
                protocol::varInt::write(output, words);
                output.writeBytes(
                    input.readSpan(static_cast<std::size_t>(words < 0 ? 0 : words) * sizeof(std::int64_t)));
            }
            writeChatType(input, output);
            nbtTextAsJson(input, output);
            const bool target = protocol::readBoolean(input);
            protocol::writeBoolean(output, target);
            if (target) {
                nbtTextAsJson(input, output);
            }
        });
        break;
    case play::DisguisedChatPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            nbtTextAsJson(input, output);
            writeChatType(input, output);
            nbtTextAsJson(input, output);
            const bool target = protocol::readBoolean(input);
            protocol::writeBoolean(output, target);
            if (target) {
                nbtTextAsJson(input, output);
            }
        });
        break;
    case kServerData:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            nbtTextAsJson(input, output);
            output.writeBytes(input.readable());
        });
        break;
    case play::SetScorePacket::kId:
        // 1.20.3 dropped the action byte, removing a score by a packet of its own, and appended the display.
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::writeString(output, protocol::readString(input));
            output.writeByte(0);
            protocol::writeString(output, protocol::readString(input));
            protocol::varInt::write(output, protocol::varInt::read(input));
        });
        break;
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
