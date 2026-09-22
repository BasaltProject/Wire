#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::protocol::play {

/// The three title lines share a body: one component, written as network NBT.
struct TitleTextPacket {
    text::Component text;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TitleTextPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TitleTextPacket&, const TitleTextPacket&) = default;
};

/// Clientbound, id 0x74: the big line of a title.
inline constexpr std::int32_t kSetTitleTextId = 0x74;
/// Clientbound, id 0x72: the smaller line under it.
inline constexpr std::int32_t kSetSubtitleTextId = 0x72;
/// Clientbound, id 0x59: the title system's line above the hotbar (not a system chat message with
/// the action-bar flag), which `/title ... actionbar` sets and `/title ... clear` removes.
inline constexpr std::int32_t kSetActionBarTextId = 0x59;

/// Clientbound, id 0x75: how long a title fades in, stays and fades out, in ticks.
struct SetTitlesAnimationPacket {
    static constexpr std::int32_t kId = 0x75;

    std::int32_t fadeIn = 0;
    std::int32_t stay = 0;
    std::int32_t fadeOut = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetTitlesAnimationPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetTitlesAnimationPacket&, const SetTitlesAnimationPacket&) = default;
};

/// Clientbound, id 0x0e: hides whatever title is showing, optionally resetting its timings too.
struct ClearTitlesPacket {
    static constexpr std::int32_t kId = 0x0e;

    bool resetTimes = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClearTitlesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClearTitlesPacket&, const ClearTitlesPacket&) = default;
};

/// The mixer a sound plays through. The ordinal is what the wire carries. parity: SoundSource.
enum class SoundSource : std::uint8_t {
    Master,
    Music,
    Records,
    Weather,
    Blocks,
    Hostile,
    Neutral,
    Players,
    Ambient,
    Voice,
    Ui,
};

/// The name `/playsound` and `/stopsound` take, which is not always the constant's own.
[[nodiscard]] std::string_view soundSourceCommandName(SoundSource source) noexcept;

[[nodiscard]] std::optional<SoundSource> soundSourceByCommandName(std::string_view name) noexcept;

/// Clientbound, id 0x77: play a sound at a position. The sound goes as a direct holder (a zero,
/// then the id) so the client resolves the name itself; coordinates are eighths of a block
/// (vanilla's LOCATION_ACCURACY).
struct SoundPacket {
    static constexpr std::int32_t kId = 0x77;
    static constexpr double kLocationAccuracy = 8.0;

    std::string sound;
    SoundSource source = SoundSource::Master;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float volume = 1.0F;
    float pitch = 1.0F;
    std::int64_t seed = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SoundPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SoundPacket&, const SoundPacket&) = default;
};

/// Clientbound, id 0x79: stop one sound, every sound from one source, or all of them. The flag byte
/// is vanilla's: 1 means a source follows, 2 a name.
struct StopSoundPacket {
    static constexpr std::int32_t kId = 0x79;

    std::optional<std::string> sound;
    std::optional<SoundSource> source;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<StopSoundPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const StopSoundPacket&, const StopSoundPacket&) = default;
};

/// The line being typed, asked while it is typed. parity: ServerboundCommandSuggestionPacket.
/// The id comes back untouched: the client drops a reply with an older id.
struct CommandSuggestionRequestPacket {
    static constexpr std::int32_t kId = 0x0f;
    /// parity: vanilla's own cap on the line it will ask about.
    static constexpr std::int32_t kMaxCommandLength = 32500;

    std::int32_t id = 0;
    std::string command;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<CommandSuggestionRequestPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const CommandSuggestionRequestPacket&, const CommandSuggestionRequestPacket&) = default;
};

/// One completion: the text that replaces the range, and the grey note drawn beside it.
struct CommandSuggestion {
    std::string text;
    std::optional<text::Component> tooltip;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static CommandSuggestion read(io::ByteBuffer& buffer);

    friend bool operator==(const CommandSuggestion&, const CommandSuggestion&) = default;
};

/// The completions for a request. The start and length are the slice of the line, in characters,
/// that the pick replaces.
struct CommandSuggestionsPacket {
    static constexpr std::int32_t kId = 0x0f;

    std::int32_t id = 0;
    std::int32_t start = 0;
    std::int32_t length = 0;
    std::vector<CommandSuggestion> suggestions;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<CommandSuggestionsPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const CommandSuggestionsPacket&, const CommandSuggestionsPacket&) = default;
};

} // namespace basalt::wire::protocol::play
