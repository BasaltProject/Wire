#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::protocol::play {

struct TitleTextPacket {
    text::Component text;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TitleTextPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TitleTextPacket&, const TitleTextPacket&) = default;
};

inline constexpr std::int32_t kSetTitleTextId = 0x74;
inline constexpr std::int32_t kSetSubtitleTextId = 0x72;
inline constexpr std::int32_t kSetActionBarTextId = 0x59;

struct SetTitlesAnimationPacket {
    static constexpr std::int32_t kId = 0x75;

    std::int32_t fadeIn = 0;
    std::int32_t stay = 0;
    std::int32_t fadeOut = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetTitlesAnimationPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetTitlesAnimationPacket&, const SetTitlesAnimationPacket&) = default;
};

struct ClearTitlesPacket {
    static constexpr std::int32_t kId = 0x0e;

    bool resetTimes = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClearTitlesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClearTitlesPacket&, const ClearTitlesPacket&) = default;
};

/// parity: SoundSource.
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

[[nodiscard]] std::string_view soundSourceCommandName(SoundSource source) noexcept;

[[nodiscard]] std::optional<SoundSource> soundSourceByCommandName(std::string_view name) noexcept;

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

/// The flag byte is vanilla's: 1 means a source follows, 2 a name.
struct StopSoundPacket {
    static constexpr std::int32_t kId = 0x79;

    std::optional<std::string> sound;
    std::optional<SoundSource> source;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<StopSoundPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const StopSoundPacket&, const StopSoundPacket&) = default;
};

/// parity: ServerboundCommandSuggestionPacket.
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

struct CommandSuggestion {
    std::string text;
    std::optional<text::Component> tooltip;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static CommandSuggestion read(io::ByteBuffer& buffer);

    friend bool operator==(const CommandSuggestion&, const CommandSuggestion&) = default;
};

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
