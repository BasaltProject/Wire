#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

struct TabListPacket {
    static constexpr std::int32_t kId = 0x7d;

    text::Component header;
    text::Component footer;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TabListPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TabListPacket&, const TabListPacket&) = default;
};

/// How a score's number is drawn, vanilla's NumberFormat registry.
class NumberFormat {
public:
    struct Blank {
        friend bool operator==(const Blank&, const Blank&) = default;
    };

    struct Styled {
        nbt::NbtCompound style;

        friend bool operator==(const Styled&, const Styled&) = default;
    };

    struct Fixed {
        text::Component value;

        friend bool operator==(const Fixed&, const Fixed&) = default;
    };

    using Payload = std::variant<Blank, Styled, Fixed>;

    NumberFormat() = default;

    explicit NumberFormat(Payload payload) : payload_(std::move(payload)) {}

    [[nodiscard]] std::int32_t typeId() const noexcept { return static_cast<std::int32_t>(payload_.index()); }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&payload_);
    }

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<NumberFormat> read(io::ByteBuffer& buffer);

    friend bool operator==(const NumberFormat&, const NumberFormat&) = default;

private:
    Payload payload_ = Blank{};
};

/// parity: DisplaySlot.
enum class DisplaySlot : std::int32_t {
    List = 0,
    Sidebar = 1,
    BelowName = 2,
};

/// parity: ObjectiveCriteria.RenderType.
enum class ObjectiveRenderType : std::uint8_t {
    Integer,
    Hearts,
};

struct SetObjectivePacket {
    static constexpr std::int32_t kId = 0x6c;
    static constexpr std::uint8_t kMethodAdd = 0;
    static constexpr std::uint8_t kMethodRemove = 1;
    static constexpr std::uint8_t kMethodChange = 2;

    std::string objectiveName;
    std::uint8_t method = kMethodAdd;
    std::optional<text::Component> displayName;
    ObjectiveRenderType renderType = ObjectiveRenderType::Integer;
    std::optional<NumberFormat> numberFormat;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetObjectivePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetObjectivePacket&, const SetObjectivePacket&) = default;
};

struct SetDisplayObjectivePacket {
    static constexpr std::int32_t kId = 0x64;

    DisplaySlot slot = DisplaySlot::List;
    std::string objectiveName;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetDisplayObjectivePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetDisplayObjectivePacket&, const SetDisplayObjectivePacket&) = default;
};

struct SetScorePacket {
    static constexpr std::int32_t kId = 0x70;

    std::string owner;
    std::string objectiveName;
    std::int32_t score = 0;
    std::optional<text::Component> display;
    std::optional<NumberFormat> numberFormat;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetScorePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetScorePacket&, const SetScorePacket&) = default;
};

struct ResetScorePacket {
    static constexpr std::int32_t kId = 0x50;

    std::string owner;
    std::optional<std::string> objectiveName;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ResetScorePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ResetScorePacket&, const ResetScorePacket&) = default;
};

/// parity: BossEvent.BossBarColor.
enum class BossBarColor : std::uint8_t {
    Pink,
    Blue,
    Red,
    Green,
    Yellow,
    Purple,
    White,
};

/// parity: BossEvent.BossBarOverlay: a plain bar, or one notched into segments.
enum class BossBarOverlay : std::uint8_t {
    Progress,
    Notched6,
    Notched10,
    Notched12,
    Notched20,
};

[[nodiscard]] std::string_view bossBarOverlayName(BossBarOverlay overlay) noexcept;

/// A bar is added once and then updated: re-adding it restarts the client's fade animation, which flickers.
struct BossEventPacket {
    static constexpr std::int32_t kId = 0x09;

    struct Add {
        text::Component name;
        float progress = 1.0F;
        BossBarColor color = BossBarColor::Purple;
        BossBarOverlay overlay = BossBarOverlay::Progress;
        bool darkenScreen = false;
        bool playMusic = false;
        bool createWorldFog = false;

        friend bool operator==(const Add&, const Add&) = default;
    };

    struct Remove {
        friend bool operator==(const Remove&, const Remove&) = default;
    };

    struct UpdateProgress {
        float progress = 1.0F;

        friend bool operator==(const UpdateProgress&, const UpdateProgress&) = default;
    };

    struct UpdateName {
        text::Component name;

        friend bool operator==(const UpdateName&, const UpdateName&) = default;
    };

    struct UpdateStyle {
        BossBarColor color = BossBarColor::Purple;
        BossBarOverlay overlay = BossBarOverlay::Progress;

        friend bool operator==(const UpdateStyle&, const UpdateStyle&) = default;
    };

    struct UpdateProperties {
        bool darkenScreen = false;
        bool playMusic = false;
        bool createWorldFog = false;

        friend bool operator==(const UpdateProperties&, const UpdateProperties&) = default;
    };

    /// The order is the operation's own type id.
    using Operation = std::variant<Add, Remove, UpdateProgress, UpdateName, UpdateStyle, UpdateProperties>;

    common::Uuid id;
    Operation operation = Remove{};

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BossEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BossEventPacket&, const BossEventPacket&) = default;
};

/// parity: Team.Visibility.
enum class NameTagVisibility : std::uint8_t {
    Always,
    Never,
    HideForOtherTeams,
    HideForOwnTeam,
};

[[nodiscard]] std::string_view nameTagVisibilityName(NameTagVisibility visibility) noexcept;

[[nodiscard]] std::optional<NameTagVisibility> nameTagVisibilityByName(std::string_view name) noexcept;

/// parity: Team.CollisionRule.
enum class CollisionRule : std::uint8_t {
    Always,
    Never,
    PushOtherTeams,
    PushOwnTeam,
};

[[nodiscard]] std::string_view collisionRuleName(CollisionRule rule) noexcept;

[[nodiscard]] std::optional<CollisionRule> collisionRuleByName(std::string_view name) noexcept;

/// parity: TeamColor.
enum class TeamColor : std::uint8_t {
    Black,
    DarkBlue,
    DarkGreen,
    DarkAqua,
    DarkRed,
    DarkPurple,
    Gold,
    Gray,
    DarkGray,
    Blue,
    Green,
    Aqua,
    Red,
    LightPurple,
    Yellow,
    White,
};

/// parity: TeamColor's own packed RGB, which is what a waypoint icon carries.
[[nodiscard]] std::int32_t teamColorRgb(TeamColor color) noexcept;

[[nodiscard]] std::optional<TeamColor> teamColorByName(std::string_view name) noexcept;

struct SetPlayerTeamPacket {
    static constexpr std::int32_t kId = 0x6f;
    static constexpr std::uint8_t kMethodAdd = 0;
    static constexpr std::uint8_t kMethodRemove = 1;
    static constexpr std::uint8_t kMethodChange = 2;
    static constexpr std::uint8_t kMethodJoin = 3;
    static constexpr std::uint8_t kMethodLeave = 4;

    struct Parameters {
        text::Component displayName;
        text::Component prefix;
        text::Component suffix;
        NameTagVisibility nameTagVisibility = NameTagVisibility::Always;
        CollisionRule collisionRule = CollisionRule::Always;
        std::optional<TeamColor> color;
        bool friendlyFire = true;
        bool seeFriendlyInvisibles = true;

        /// parity: PlayerTeam.packOptions.
        [[nodiscard]] std::uint8_t options() const noexcept;

        void write(io::ByteBuffer& buffer) const;
        [[nodiscard]] static std::optional<Parameters> read(io::ByteBuffer& buffer);

        friend bool operator==(const Parameters&, const Parameters&) = default;
    };

    std::string teamName;
    std::uint8_t method = kMethodAdd;
    std::optional<Parameters> parameters;
    std::vector<std::string> members;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetPlayerTeamPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetPlayerTeamPacket&, const SetPlayerTeamPacket&) = default;
};

/// parity: Dialog.STREAM_CODEC is ByteBufCodecs.holder, not holderRegistry, so the id really does.
struct ShowDialogPacket {
    static constexpr std::int32_t kId = 0x8f;

    std::int32_t dialogId = 0;
    /// Sent instead of a registry id when set: the dialog written out in full, as a plugin builds one.
    std::optional<nbt::NbtCompound> inlineDialog;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ShowDialogPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ShowDialogPacket&, const ShowDialogPacket&) = default;
};

/// What a `dynamic/custom` dialog button sends back: the id it was given and the inputs it held.
struct ServerboundCustomClickActionPacket {
    static constexpr std::int32_t kId = 0x44;

    std::string id;
    std::optional<nbt::NbtTag> payload;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundCustomClickActionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundCustomClickActionPacket&,
                           const ServerboundCustomClickActionPacket&) = default;
};

inline constexpr std::int32_t kClearDialogId = 0x8e;

} // namespace basalt::wire::protocol::play
