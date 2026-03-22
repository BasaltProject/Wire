#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

/// The index of each parser in vanilla's COMMAND_ARGUMENT_TYPE registry, which is the registration
/// order in ArgumentTypeInfos.bootstrap. A protocol fact, not content: the client resolves the
/// parser by that number alone.
namespace argumentType {
inline constexpr std::int32_t kBrigadierBool = 0;
inline constexpr std::int32_t kBrigadierFloat = 1;
inline constexpr std::int32_t kBrigadierDouble = 2;
inline constexpr std::int32_t kBrigadierInteger = 3;
inline constexpr std::int32_t kBrigadierLong = 4;
inline constexpr std::int32_t kBrigadierString = 5;
inline constexpr std::int32_t kEntity = 6;
inline constexpr std::int32_t kGameProfile = 7;
inline constexpr std::int32_t kBlockPos = 8;
inline constexpr std::int32_t kColumnPos = 9;
inline constexpr std::int32_t kVec3 = 10;
inline constexpr std::int32_t kVec2 = 11;
inline constexpr std::int32_t kBlockState = 12;
inline constexpr std::int32_t kBlockPredicate = 13;
inline constexpr std::int32_t kItemStack = 14;
inline constexpr std::int32_t kItemPredicate = 15;
inline constexpr std::int32_t kTeamColor = 16;
inline constexpr std::int32_t kHexColor = 17;
inline constexpr std::int32_t kComponent = 18;
inline constexpr std::int32_t kStyle = 19;
inline constexpr std::int32_t kMessage = 20;
inline constexpr std::int32_t kNbtCompoundTag = 21;
inline constexpr std::int32_t kNbtTag = 22;
inline constexpr std::int32_t kNbtPath = 23;
inline constexpr std::int32_t kObjective = 24;
inline constexpr std::int32_t kObjectiveCriteria = 25;
inline constexpr std::int32_t kOperation = 26;
inline constexpr std::int32_t kParticle = 27;
inline constexpr std::int32_t kAngle = 28;
inline constexpr std::int32_t kRotation = 29;
inline constexpr std::int32_t kScoreboardSlot = 30;
inline constexpr std::int32_t kScoreHolder = 31;
inline constexpr std::int32_t kSwizzle = 32;
inline constexpr std::int32_t kTeam = 33;
inline constexpr std::int32_t kItemSlot = 34;
inline constexpr std::int32_t kItemSlots = 35;
inline constexpr std::int32_t kResourceLocation = 36;
inline constexpr std::int32_t kFunction = 37;
inline constexpr std::int32_t kEntityAnchor = 38;
inline constexpr std::int32_t kIntRange = 39;
inline constexpr std::int32_t kFloatRange = 40;
inline constexpr std::int32_t kDimension = 41;
inline constexpr std::int32_t kGamemode = 42;
inline constexpr std::int32_t kTime = 43;
inline constexpr std::int32_t kResourceOrTag = 44;
inline constexpr std::int32_t kResourceOrTagKey = 45;
inline constexpr std::int32_t kResource = 46;
inline constexpr std::int32_t kResourceKey = 47;
inline constexpr std::int32_t kResourceSelector = 48;
inline constexpr std::int32_t kTemplateMirror = 49;
inline constexpr std::int32_t kTemplateRotation = 50;
inline constexpr std::int32_t kHeightmap = 51;
inline constexpr std::int32_t kLootTable = 52;
inline constexpr std::int32_t kLootPredicate = 53;
inline constexpr std::int32_t kLootModifier = 54;
inline constexpr std::int32_t kContextFloatProvider = 55;
inline constexpr std::int32_t kContextIntProvider = 56;
inline constexpr std::int32_t kSlotSource = 57;
inline constexpr std::int32_t kDialog = 58;
inline constexpr std::int32_t kFeature = 59;
inline constexpr std::int32_t kSwingAnimation = 60;
inline constexpr std::int32_t kUuid = 61;
inline constexpr std::int32_t kCount = 62;
} // namespace argumentType

/// One argument parser in the command tree, as the client needs to instantiate it: its registry id
/// plus whatever configuration that particular parser carries.
///
/// Most of the fifty-seven carry nothing, which is vanilla's SingletonArgumentInfo. The bounded
/// number parsers hold their limits as absent-or-present rather than as sentinel extremes, because
/// that is exactly what the wire's flag byte says and a sentinel is a value somebody eventually
/// writes by accident.
class ArgumentParser {
public:
    /// A parser with no configuration at all.
    struct None {
        friend bool operator==(const None&, const None&) = default;
    };

    /// `brigadier:string`. The order is StringArgumentType.StringType's own.
    enum class StringKind : std::uint8_t {
        SingleWord,
        QuotablePhrase,
        GreedyPhrase,
    };

    struct StringProperties {
        StringKind kind = StringKind::SingleWord;

        friend bool operator==(const StringProperties&, const StringProperties&) = default;
    };

    template <typename T>
    struct Bounds {
        std::optional<T> min;
        std::optional<T> max;

        friend bool operator==(const Bounds&, const Bounds&) = default;
    };

    /// `minecraft:entity`: a selector. The two flags decide how many entities and of what kind may
    /// match.
    struct EntityProperties {
        bool single = false;
        bool playersOnly = false;

        friend bool operator==(const EntityProperties&, const EntityProperties&) = default;
    };

    /// `minecraft:score_holder`: a selector or a bare name, which is how fake players are scored.
    struct ScoreHolderProperties {
        bool multiple = false;

        friend bool operator==(const ScoreHolderProperties&, const ScoreHolderProperties&) = default;
    };

    /// `minecraft:time`: a tick count with an optional unit, never below this.
    struct TimeProperties {
        std::int32_t min = 0;

        friend bool operator==(const TimeProperties&, const TimeProperties&) = default;
    };

    /// The parsers that name a registry: `resource`, `resource_key`, `resource_or_tag` and friends.
    struct RegistryProperties {
        std::string registry;

        friend bool operator==(const RegistryProperties&, const RegistryProperties&) = default;
    };

    using Properties = std::variant<None, StringProperties, Bounds<std::int32_t>, Bounds<double>, Bounds<float>,
                                    EntityProperties, ScoreHolderProperties, TimeProperties, RegistryProperties>;

    explicit ArgumentParser(std::int32_t registryId, Properties properties = None{})
        : registryId_(registryId), properties_(std::move(properties)) {}

    [[nodiscard]] std::int32_t registryId() const noexcept { return registryId_; }

    [[nodiscard]] const Properties& properties() const noexcept { return properties_; }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&properties_);
    }

    /// Writes the registry id and then whatever configuration follows it.
    void write(io::ByteBuffer& buffer) const;

    [[nodiscard]] static std::optional<ArgumentParser> read(io::ByteBuffer& buffer);

    friend bool operator==(const ArgumentParser&, const ArgumentParser&) = default;

private:
    std::int32_t registryId_;
    Properties properties_;
};

/// One node of the command tree on the wire, already flattened to indices. parity:
/// ClientboundCommandsPacket.Entry, whose flag byte carries both the node kind in its low two bits
/// and the executable, redirect and custom-suggestion bits above them.
struct CommandNodeData {
    enum class Kind : std::uint8_t {
        Root,
        Literal,
        Argument,
    };

    Kind kind = Kind::Root;
    bool executable = false;
    std::vector<std::int32_t> children;
    std::optional<std::int32_t> redirect;
    /// Absent only on the root, which has no name.
    std::optional<std::string> name;
    /// Present exactly on an argument node.
    std::optional<ArgumentParser> parser;
    std::optional<std::string> suggestionId;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<CommandNodeData> read(io::ByteBuffer& buffer);

    friend bool operator==(const CommandNodeData&, const CommandNodeData&) = default;
};

/// Clientbound, id 0x10: the command tree, sent once the player is in play.
///
/// The client parses commands locally against this tree before sending anything, so it is what
/// makes tab completion, the red "unknown command" highlight and argument colouring work. Vanilla
/// sends a per-player tree, already filtered to the commands that player may use.
struct CommandsPacket {
    static constexpr std::int32_t kId = 0x10;

    std::vector<CommandNodeData> nodes;
    std::int32_t rootIndex = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<CommandsPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const CommandsPacket&, const CommandsPacket&) = default;
};

/// Serverbound, id 0x07: a command the player typed, without its leading slash.
struct ChatCommandPacket {
    static constexpr std::int32_t kId = 0x07;
    static constexpr std::int32_t kMaxLength = 256;

    std::string command;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChatCommandPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChatCommandPacket&, const ChatCommandPacket&) = default;
};

/// Serverbound, id 0x08: a command carrying signed message arguments, which is what `/msg` and
/// `/say` become once the client has a chat session.
///
/// Only the command is decoded. The signature block that follows it (timestamp, salt, per-argument
/// signatures, last-seen acknowledgements) is left unread: nothing verifies signatures yet and the
/// frame is discarded whole afterwards, so reading past the command would be ceremony.
struct SignedChatCommandPacket {
    static constexpr std::int32_t kId = 0x08;

    std::string command;

    [[nodiscard]] static std::optional<SignedChatCommandPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SignedChatCommandPacket&, const SignedChatCommandPacket&) = default;
};

} // namespace basalt::wire::protocol::play
