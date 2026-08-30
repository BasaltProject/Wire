#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <utility>

namespace basalt::wire::protocol::play {
namespace {

constexpr std::uint8_t kFlagDarkenScreen = 1;
constexpr std::uint8_t kFlagPlayMusic = 2;
constexpr std::uint8_t kFlagCreateWorldFog = 4;

constexpr std::array<std::string_view, 5> kOverlayNames = {"progress", "notched_6", "notched_10", "notched_12",
                                                           "notched_20"};

constexpr std::array<std::string_view, 4> kVisibilityNames = {"always", "never", "hideForOtherTeams", "hideForOwnTeam"};

constexpr std::array<std::string_view, 4> kCollisionNames = {"always", "never", "pushOtherTeams", "pushOwnTeam"};

constexpr std::array<std::pair<std::string_view, std::int32_t>, 16> kTeamColors = {{{"black", 0x000000},
                                                                                    {"dark_blue", 0x0000AA},
                                                                                    {"dark_green", 0x00AA00},
                                                                                    {"dark_aqua", 0x00AAAA},
                                                                                    {"dark_red", 0xAA0000},
                                                                                    {"dark_purple", 0xAA00AA},
                                                                                    {"gold", 0xFFAA00},
                                                                                    {"gray", 0xAAAAAA},
                                                                                    {"dark_gray", 0x555555},
                                                                                    {"blue", 0x5555FF},
                                                                                    {"green", 0x55FF55},
                                                                                    {"aqua", 0x55FFFF},
                                                                                    {"red", 0xFF5555},
                                                                                    {"light_purple", 0xFF55FF},
                                                                                    {"yellow", 0xFFFF55},
                                                                                    {"white", 0xFFFFFF}}};

void writeComponent(io::ByteBuffer& buffer, const text::Component& component) {
    writeAnonymousNbt(buffer, text::componentNbt::serialize(component));
}

text::Component readComponent(io::ByteBuffer& buffer) {
    return text::componentNbt::deserialize(readAnonymousNbt(buffer));
}

template <typename Enum>
std::optional<Enum> fromOrdinal(std::int32_t ordinal, std::size_t count) noexcept {
    if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= count) {
        return std::nullopt;
    }
    return static_cast<Enum>(ordinal);
}

std::uint8_t packBossFlags(bool darkenScreen, bool playMusic, bool createWorldFog) noexcept {
    std::uint8_t flags = 0;
    if (darkenScreen) {
        flags |= kFlagDarkenScreen;
    }
    if (playMusic) {
        flags |= kFlagPlayMusic;
    }
    if (createWorldFog) {
        flags |= kFlagCreateWorldFog;
    }
    return flags;
}

} // namespace

std::string_view bossBarOverlayName(BossBarOverlay overlay) noexcept {
    return kOverlayNames[static_cast<std::size_t>(overlay)];
}

std::string_view nameTagVisibilityName(NameTagVisibility visibility) noexcept {
    return kVisibilityNames[static_cast<std::size_t>(visibility)];
}

std::optional<NameTagVisibility> nameTagVisibilityByName(std::string_view name) noexcept {
    for (std::size_t i = 0; i < kVisibilityNames.size(); ++i) {
        if (kVisibilityNames[i] == name) {
            return static_cast<NameTagVisibility>(i);
        }
    }
    return std::nullopt;
}

std::string_view collisionRuleName(CollisionRule rule) noexcept {
    return kCollisionNames[static_cast<std::size_t>(rule)];
}

std::optional<CollisionRule> collisionRuleByName(std::string_view name) noexcept {
    for (std::size_t i = 0; i < kCollisionNames.size(); ++i) {
        if (kCollisionNames[i] == name) {
            return static_cast<CollisionRule>(i);
        }
    }
    return std::nullopt;
}

std::int32_t teamColorRgb(TeamColor color) noexcept { return kTeamColors[static_cast<std::size_t>(color)].second; }

std::optional<TeamColor> teamColorByName(std::string_view name) noexcept {
    for (std::size_t i = 0; i < kTeamColors.size(); ++i) {
        if (kTeamColors[i].first == name) {
            return static_cast<TeamColor>(i);
        }
    }
    return std::nullopt;
}

void TabListPacket::write(io::ByteBuffer& buffer) const {
    writeComponent(buffer, header);
    writeComponent(buffer, footer);
}

std::optional<TabListPacket> TabListPacket::read(io::ByteBuffer& buffer) {
    TabListPacket packet;
    packet.header = readComponent(buffer);
    packet.footer = readComponent(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void NumberFormat::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, typeId());
    if (const auto* styled = as<Styled>()) {
        writeAnonymousNbt(buffer, nbt::NbtTag::ofCompound(styled->style));
    } else if (const auto* fixed = as<Fixed>()) {
        writeComponent(buffer, fixed->value);
    }
    // Blank writes nothing at all, which is the whole point of it.
}

std::optional<NumberFormat> NumberFormat::read(io::ByteBuffer& buffer) {
    const std::int32_t id = varInt::read(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    if (id == 0) {
        return NumberFormat{Blank{}};
    }
    if (id == 1) {
        const nbt::NbtTag tag = readAnonymousNbt(buffer);
        const auto* compound = tag.get<nbt::NbtCompound>();
        if (compound == nullptr) {
            buffer.markFailed();
            return std::nullopt;
        }
        return NumberFormat{Styled{*compound}};
    }
    if (id == 2) {
        return NumberFormat{Fixed{readComponent(buffer)}};
    }
    buffer.markFailed();
    return std::nullopt;
}

void SetObjectivePacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, objectiveName);
    buffer.writeByte(method);
    if (method != kMethodAdd && method != kMethodChange) {
        return;
    }
    writeComponent(buffer, displayName.value_or(text::Component::empty()));
    varInt::write(buffer, static_cast<std::int32_t>(renderType));
    writeBoolean(buffer, numberFormat.has_value());
    if (numberFormat) {
        numberFormat->write(buffer);
    }
}

std::optional<SetObjectivePacket> SetObjectivePacket::read(io::ByteBuffer& buffer) {
    SetObjectivePacket packet;
    packet.objectiveName = readString(buffer);
    packet.method = buffer.readByte();
    if (buffer.failed()) {
        return std::nullopt;
    }
    if (packet.method != kMethodAdd && packet.method != kMethodChange) {
        return packet;
    }

    packet.displayName = readComponent(buffer);
    const std::optional<ObjectiveRenderType> renderType = fromOrdinal<ObjectiveRenderType>(varInt::read(buffer), 2);
    if (!renderType) {
        buffer.markFailed();
        return std::nullopt;
    }
    packet.renderType = *renderType;
    if (readBoolean(buffer)) {
        packet.numberFormat = NumberFormat::read(buffer);
        if (!packet.numberFormat) {
            return std::nullopt;
        }
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SetDisplayObjectivePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(slot));
    writeString(buffer, objectiveName);
}

std::optional<SetDisplayObjectivePacket> SetDisplayObjectivePacket::read(io::ByteBuffer& buffer) {
    SetDisplayObjectivePacket packet;
    // parity: an unknown slot folds to the list rather than refusing the packet.
    packet.slot = fromOrdinal<DisplaySlot>(varInt::read(buffer), 3).value_or(DisplaySlot::List);
    packet.objectiveName = readString(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SetScorePacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, owner);
    writeString(buffer, objectiveName);
    varInt::write(buffer, score);
    writeBoolean(buffer, display.has_value());
    if (display) {
        writeComponent(buffer, *display);
    }
    writeBoolean(buffer, numberFormat.has_value());
    if (numberFormat) {
        numberFormat->write(buffer);
    }
}

std::optional<SetScorePacket> SetScorePacket::read(io::ByteBuffer& buffer) {
    SetScorePacket packet;
    packet.owner = readString(buffer);
    packet.objectiveName = readString(buffer);
    packet.score = varInt::read(buffer);
    if (readBoolean(buffer)) {
        packet.display = readComponent(buffer);
    }
    if (readBoolean(buffer)) {
        packet.numberFormat = NumberFormat::read(buffer);
        if (!packet.numberFormat) {
            return std::nullopt;
        }
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ResetScorePacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, owner);
    writeBoolean(buffer, objectiveName.has_value());
    if (objectiveName) {
        writeString(buffer, *objectiveName);
    }
}

std::optional<ResetScorePacket> ResetScorePacket::read(io::ByteBuffer& buffer) {
    ResetScorePacket packet;
    packet.owner = readString(buffer);
    if (readBoolean(buffer)) {
        packet.objectiveName = readString(buffer);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void BossEventPacket::write(io::ByteBuffer& buffer) const {
    writeUuid(buffer, id);
    varInt::write(buffer, static_cast<std::int32_t>(operation.index()));

    if (const auto* add = std::get_if<Add>(&operation)) {
        writeComponent(buffer, add->name);
        buffer.writeFloat(add->progress);
        varInt::write(buffer, static_cast<std::int32_t>(add->color));
        varInt::write(buffer, static_cast<std::int32_t>(add->overlay));
        buffer.writeByte(packBossFlags(add->darkenScreen, add->playMusic, add->createWorldFog));
    } else if (const auto* progress = std::get_if<UpdateProgress>(&operation)) {
        buffer.writeFloat(progress->progress);
    } else if (const auto* name = std::get_if<UpdateName>(&operation)) {
        writeComponent(buffer, name->name);
    } else if (const auto* style = std::get_if<UpdateStyle>(&operation)) {
        varInt::write(buffer, static_cast<std::int32_t>(style->color));
        varInt::write(buffer, static_cast<std::int32_t>(style->overlay));
    } else if (const auto* properties = std::get_if<UpdateProperties>(&operation)) {
        buffer.writeByte(packBossFlags(properties->darkenScreen, properties->playMusic, properties->createWorldFog));
    }
    // A remove is the id and the type, and nothing else.
}

std::optional<BossEventPacket> BossEventPacket::read(io::ByteBuffer& buffer) {
    BossEventPacket packet;
    packet.id = readUuid(buffer);
    const std::int32_t type = varInt::read(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }

    switch (type) {
    case 0: {
        Add add;
        add.name = readComponent(buffer);
        add.progress = buffer.readFloat();
        const auto color = fromOrdinal<BossBarColor>(varInt::read(buffer), 7);
        const auto overlay = fromOrdinal<BossBarOverlay>(varInt::read(buffer), 5);
        if (!color || !overlay) {
            buffer.markFailed();
            return std::nullopt;
        }
        add.color = *color;
        add.overlay = *overlay;
        const std::uint8_t flags = buffer.readByte();
        add.darkenScreen = (flags & kFlagDarkenScreen) != 0;
        add.playMusic = (flags & kFlagPlayMusic) != 0;
        add.createWorldFog = (flags & kFlagCreateWorldFog) != 0;
        packet.operation = std::move(add);
        break;
    }
    case 1:
        packet.operation = Remove{};
        break;
    case 2:
        packet.operation = UpdateProgress{buffer.readFloat()};
        break;
    case 3:
        packet.operation = UpdateName{readComponent(buffer)};
        break;
    case 4: {
        const auto color = fromOrdinal<BossBarColor>(varInt::read(buffer), 7);
        const auto overlay = fromOrdinal<BossBarOverlay>(varInt::read(buffer), 5);
        if (!color || !overlay) {
            buffer.markFailed();
            return std::nullopt;
        }
        packet.operation = UpdateStyle{*color, *overlay};
        break;
    }
    case 5: {
        const std::uint8_t flags = buffer.readByte();
        packet.operation = UpdateProperties{(flags & kFlagDarkenScreen) != 0, (flags & kFlagPlayMusic) != 0,
                                            (flags & kFlagCreateWorldFog) != 0};
        break;
    }
    default:
        buffer.markFailed();
        return std::nullopt;
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

std::uint8_t SetPlayerTeamPacket::Parameters::options() const noexcept {
    std::uint8_t flags = 0;
    if (friendlyFire) {
        flags |= 1;
    }
    if (seeFriendlyInvisibles) {
        flags |= 2;
    }
    return flags;
}

void SetPlayerTeamPacket::Parameters::write(io::ByteBuffer& buffer) const {
    writeComponent(buffer, displayName);
    writeComponent(buffer, prefix);
    writeComponent(buffer, suffix);
    varInt::write(buffer, static_cast<std::int32_t>(nameTagVisibility));
    varInt::write(buffer, static_cast<std::int32_t>(collisionRule));
    writeBoolean(buffer, color.has_value());
    if (color) {
        varInt::write(buffer, static_cast<std::int32_t>(*color));
    }
    buffer.writeByte(options());
}

std::optional<SetPlayerTeamPacket::Parameters> SetPlayerTeamPacket::Parameters::read(io::ByteBuffer& buffer) {
    Parameters parameters;
    parameters.displayName = readComponent(buffer);
    parameters.prefix = readComponent(buffer);
    parameters.suffix = readComponent(buffer);

    const auto visibility = fromOrdinal<NameTagVisibility>(varInt::read(buffer), 4);
    const auto collision = fromOrdinal<CollisionRule>(varInt::read(buffer), 4);
    if (!visibility || !collision) {
        buffer.markFailed();
        return std::nullopt;
    }
    parameters.nameTagVisibility = *visibility;
    parameters.collisionRule = *collision;

    if (readBoolean(buffer)) {
        parameters.color = fromOrdinal<TeamColor>(varInt::read(buffer), 16);
        if (!parameters.color) {
            buffer.markFailed();
            return std::nullopt;
        }
    }

    const std::uint8_t options = buffer.readByte();
    parameters.friendlyFire = (options & 1) != 0;
    parameters.seeFriendlyInvisibles = (options & 2) != 0;
    return buffer.failed() ? std::nullopt : std::optional{std::move(parameters)};
}

void SetPlayerTeamPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, teamName);
    buffer.writeByte(method);
    if ((method == kMethodAdd || method == kMethodChange) && parameters) {
        parameters->write(buffer);
    }
    if (method == kMethodAdd || method == kMethodJoin || method == kMethodLeave) {
        varInt::write(buffer, static_cast<std::int32_t>(members.size()));
        for (const std::string& member : members) {
            writeString(buffer, member);
        }
    }
}

std::optional<SetPlayerTeamPacket> SetPlayerTeamPacket::read(io::ByteBuffer& buffer) {
    SetPlayerTeamPacket packet;
    packet.teamName = readString(buffer);
    packet.method = buffer.readByte();
    if (buffer.failed()) {
        return std::nullopt;
    }

    if (packet.method == kMethodAdd || packet.method == kMethodChange) {
        packet.parameters = Parameters::read(buffer);
        if (!packet.parameters) {
            return std::nullopt;
        }
    }
    if (packet.method == kMethodAdd || packet.method == kMethodJoin || packet.method == kMethodLeave) {
        const std::int32_t count = readCollectionSize(buffer);
        packet.members.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
            packet.members.push_back(readString(buffer));
        }
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ShowDialogPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, dialogId + 1); }

std::optional<ShowDialogPacket> ShowDialogPacket::read(io::ByteBuffer& buffer) {
    ShowDialogPacket packet{varInt::read(buffer) - 1};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

} // namespace basalt::wire::protocol::play
