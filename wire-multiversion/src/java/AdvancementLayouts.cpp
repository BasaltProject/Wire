#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/AdvancementLayouts.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java::advancementLayouts {
namespace {

namespace play = protocol::play;
namespace versions = protocolVersions;

constexpr std::int32_t kBackgroundFlag = 1;
constexpr std::int32_t kToastFlag = 2;
constexpr std::int32_t kHiddenFlag = 4;

void writeText(io::ByteBuffer& output, const text::Component& component, std::int32_t farEnd) {
    if (farEnd >= versions::kV1_20_3) {
        protocol::writeAnonymousNbt(output, text::componentNbt::serialize(component));
    } else {
        protocol::writeString(output, common::writeJson(text::componentJson::serialize(component)));
    }
}

/// Below 1.21.5 a background is the texture's own path: `ns:textures/<path>.png`.
[[nodiscard]] std::string backgroundFor(const std::string& background, std::int32_t farEnd) {
    if (farEnd >= versions::kV1_21_5) {
        return background;
    }
    const std::size_t colon = background.find(':');
    const std::string space = colon == std::string::npos ? "minecraft" : background.substr(0, colon);
    const std::string path = colon == std::string::npos ? background : background.substr(colon + 1);
    return space + ":textures/" + path + ".png";
}

void writeDisplay(io::ByteBuffer& output, const play::AdvancementEntry& entry, std::int32_t farEnd) {
    const play::AdvancementDisplayData& display = *entry.display;
    writeText(output, display.title, farEnd);
    writeText(output, display.description, farEnd);
    if (farEnd >= versions::kV26_1) {
        play::writeItemStackTemplate(output, display.icon);
    } else {
        legacyStacks::write(output, display.icon, farEnd);
    }
    protocol::varInt::write(output, display.frame);
    std::int32_t flags = 0;
    flags |= display.background.has_value() ? kBackgroundFlag : 0;
    flags |= display.showToast ? kToastFlag : 0;
    flags |= display.hidden ? kHiddenFlag : 0;
    output.writeInt(flags);
    if (display.background.has_value()) {
        protocol::writeString(output, backgroundFor(*display.background, farEnd));
    }
    // Before 26.3 the position lived inside the display.
    if (farEnd < versions::kNative) {
        output.writeFloat(entry.x);
        output.writeFloat(entry.y);
    }
}

void writeStrings(io::ByteBuffer& output, const std::vector<std::string>& strings) {
    protocol::varInt::write(output, static_cast<std::int32_t>(strings.size()));
    for (const std::string& value : strings) {
        protocol::writeString(output, value);
    }
}

void writeEntry(io::ByteBuffer& output, const play::AdvancementEntry& entry, std::int32_t farEnd) {
    protocol::writeString(output, entry.id);
    protocol::writeBoolean(output, entry.parent.has_value());
    if (entry.parent.has_value()) {
        protocol::writeString(output, *entry.parent);
    }
    protocol::writeBoolean(output, entry.display.has_value());
    if (entry.display.has_value()) {
        writeDisplay(output, entry, farEnd);
    }
    // parity: before 1.20.2 the criteria went by name ahead of the requirements, with no body.
    if (farEnd < versions::kV1_20_2) {
        std::vector<std::string> criteria;
        for (const std::vector<std::string>& group : entry.requirements) {
            for (const std::string& criterion : group) {
                if (std::ranges::find(criteria, criterion) == criteria.end()) {
                    criteria.push_back(criterion);
                }
            }
        }
        writeStrings(output, criteria);
    }
    protocol::varInt::write(output, static_cast<std::int32_t>(entry.requirements.size()));
    for (const std::vector<std::string>& group : entry.requirements) {
        writeStrings(output, group);
    }
    if (farEnd >= versions::kV1_20) {
        protocol::writeBoolean(output, entry.sendsTelemetryEvent);
    }
    if (farEnd >= versions::kNative) {
        output.writeFloat(entry.x);
        output.writeFloat(entry.y);
    }
}

} // namespace

void write(io::ByteBuffer& output, const play::UpdateAdvancementsPacket& packet, std::int32_t farEnd) {
    protocol::writeBoolean(output, packet.reset);
    protocol::varInt::write(output, static_cast<std::int32_t>(packet.added.size()));
    for (const play::AdvancementEntry& entry : packet.added) {
        writeEntry(output, entry, farEnd);
    }
    writeStrings(output, packet.removed);
    protocol::varInt::write(output, static_cast<std::int32_t>(packet.progress.size()));
    for (const play::AdvancementProgressData& one : packet.progress) {
        protocol::writeString(output, one.id);
        protocol::varInt::write(output, static_cast<std::int32_t>(one.criteria.size()));
        for (const auto& [criterion, obtained] : one.criteria) {
            protocol::writeString(output, criterion);
            protocol::writeBoolean(output, obtained.has_value());
            if (obtained.has_value()) {
                output.writeLong(*obtained);
            }
        }
    }
    if (farEnd >= versions::kV1_21_5) {
        protocol::writeBoolean(output, packet.showAdvancements);
    }
}

} // namespace basalt::wire::multiversion::java::advancementLayouts
