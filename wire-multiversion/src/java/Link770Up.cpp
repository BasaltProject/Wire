#include <basalt/wire/multiversion/java/Link770Up.hpp>
#include <basalt/wire/multiversion/java/NewRegistries771.hpp>
#include <basalt/wire/multiversion/java/Tables770.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <array>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

using protocol::ProtocolState;

constexpr std::int32_t kChangeDifficulty771 = 10;
constexpr std::int32_t kLogin771 = 43;
constexpr std::int32_t kChangeDifficultyIn771 = 3;
constexpr std::int32_t kChangeGameMode771 = 4;
constexpr std::int32_t kChatCommand771 = 6;
constexpr std::int32_t kPlayerCommand771 = 41;
constexpr std::int32_t kPlayerInput771 = 42;

constexpr std::uint8_t kSneakBit = 1U << 5U;
constexpr std::int32_t kCloudHeight = 192;
constexpr std::int32_t kPressShift = 0;
constexpr std::int32_t kReleaseShift = 1;
/// parity: ServerboundPlayerCommandPacket.Action lost its two shift entries, so the rest moved down by two.
constexpr std::int32_t kActionsRemoved = 2;

constexpr std::array<const char*, 4> kModes = {"survival", "creative", "adventure", "spectator"};

std::int32_t stepClientbound(ProtocolState state, std::int32_t id771) {
    return tables770().packets.toOlder(state, id771);
}

std::int32_t stepServerbound(std::int32_t id771) {
    return tables770().packets.serverboundToOlder(ProtocolState::Play, id771);
}

} // namespace

bool Link770Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    static const std::int32_t registryData =
        stepClientbound(ProtocolState::Configuration, protocol::configuration::RegistryDataPacket::kId);
    static const std::int32_t finish =
        stepClientbound(ProtocolState::Configuration, protocol::configuration::kFinishConfigurationId);
    static const std::int32_t difficulty = stepClientbound(ProtocolState::Play, kChangeDifficulty771);
    static const std::int32_t login = stepClientbound(ProtocolState::Play, kLogin771);

    if (state == ProtocolState::Configuration && packet.id == registryData) {
        auto data =
            decode(packet, [](io::ByteBuffer& in) { return protocol::configuration::RegistryDataPacket::read(in); });
        if (!data.has_value() || data->registryId != "minecraft:dimension_type") {
            return true;
        }
        for (protocol::configuration::RegistryEntry& entry : data->entries) {
            nbt::NbtCompound* element = entry.value ? entry.value->get<nbt::NbtCompound>() : nullptr;
            if (element == nullptr || element->contains("cloud_height")) {
                continue;
            }
            // The client draws clouds unless the effects are the Nether's or the End's.
            const std::string_view effects = element->getString("effects");
            if (effects != "minecraft:the_nether" && effects != "minecraft:the_end") {
                element->putInt("cloud_height", kCloudHeight);
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
    } else if (state == ProtocolState::Configuration && packet.id == finish) {
        const auto added = registriesAddedIn1_21_6();
        const std::vector<std::uint8_t> finishBody = packet.body;
        packet.overrideId(protocol::configuration::RegistryDataPacket::kId);
        packet.body.assign(added[0].begin(), added[0].end());
        for (std::size_t index = 1; index < added.size(); ++index) {
            packet.extras.emplace_back(registryData,
                                       std::vector<std::uint8_t>{added[index].begin(), added[index].end()});
        }
        packet.extras.emplace_back(finish, finishBody);
    } else if (state == ProtocolState::Play && packet.id == difficulty) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, input.readByte());
            output.writeBytes(input.readable());
        });
    } else if (state == ProtocolState::Play && packet.id == login) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = input.readInt();
        if (!input.failed()) {
            entityId_ = entityId;
        }
        sneaking_ = false;
    }
    return true;
}

bool Link770Up::serverboundForeign(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.id != kChangeGameMode771) {
        return false;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t mode = protocol::varInt::read(input);
    if (input.failed() || mode < 0 || mode >= static_cast<std::int32_t>(kModes.size())) {
        return false;
    }
    packet.id = stepServerbound(kChatCommand771);
    io::ByteBuffer out;
    protocol::writeString(out, std::string{"gamemode "} + kModes[static_cast<std::size_t>(mode)]);
    packet.body = out.toVector();
    return true;
}

bool Link770Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::serverbound(state, packet)) {
        return false;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    static const std::int32_t difficulty = stepServerbound(kChangeDifficultyIn771);
    static const std::int32_t command = stepServerbound(kPlayerCommand771);
    static const std::int32_t input = stepServerbound(kPlayerInput771);
    const std::int32_t id = packet.nativeId();
    if (id == difficulty) {
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            out.writeByte(static_cast<std::uint8_t>(protocol::varInt::read(in)));
            out.writeBytes(in.readable());
        });
    } else if (id == command) {
        rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            protocol::varInt::write(out, protocol::varInt::read(in));
            protocol::varInt::write(out, protocol::varInt::read(in) + kActionsRemoved);
            out.writeBytes(in.readable());
        });
    } else if (id == input) {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const bool shift = (in.readByte() & kSneakBit) != 0;
        if (shift != sneaking_) {
            sneaking_ = shift;
            io::ByteBuffer out;
            protocol::varInt::write(out, entityId_);
            protocol::varInt::write(out, shift ? kPressShift : kReleaseShift);
            protocol::varInt::write(out, 0);
            packet.extras.emplace_back(command, out.toVector());
        }
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
