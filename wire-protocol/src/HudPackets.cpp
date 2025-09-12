#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>

namespace basalt::wire::protocol::play {
namespace {

constexpr std::array<std::string_view, 11> kSoundSourceNames = {
    "master", "music", "record", "weather", "block", "hostile", "neutral", "player", "ambient", "voice", "ui"};

void writeComponent(io::ByteBuffer& buffer, const text::Component& component) {
    writeAnonymousNbt(buffer, text::componentNbt::serialize(component));
}

text::Component readComponent(io::ByteBuffer& buffer) {
    return text::componentNbt::deserialize(readAnonymousNbt(buffer));
}

std::optional<SoundSource> soundSourceFromOrdinal(std::int32_t ordinal) noexcept {
    if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= kSoundSourceNames.size()) {
        return std::nullopt;
    }
    return static_cast<SoundSource>(ordinal);
}

} // namespace

std::string_view soundSourceCommandName(SoundSource source) noexcept {
    return kSoundSourceNames[static_cast<std::size_t>(source)];
}

std::optional<SoundSource> soundSourceByCommandName(std::string_view name) noexcept {
    for (std::size_t i = 0; i < kSoundSourceNames.size(); ++i) {
        if (kSoundSourceNames[i] == name) {
            return static_cast<SoundSource>(i);
        }
    }
    return std::nullopt;
}

void TitleTextPacket::write(io::ByteBuffer& buffer) const { writeComponent(buffer, text); }

std::optional<TitleTextPacket> TitleTextPacket::read(io::ByteBuffer& buffer) {
    TitleTextPacket packet{readComponent(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SetTitlesAnimationPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeInt(fadeIn);
    buffer.writeInt(stay);
    buffer.writeInt(fadeOut);
}

std::optional<SetTitlesAnimationPacket> SetTitlesAnimationPacket::read(io::ByteBuffer& buffer) {
    SetTitlesAnimationPacket packet{buffer.readInt(), buffer.readInt(), buffer.readInt()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ClearTitlesPacket::write(io::ByteBuffer& buffer) const { writeBoolean(buffer, resetTimes); }

std::optional<ClearTitlesPacket> ClearTitlesPacket::read(io::ByteBuffer& buffer) {
    ClearTitlesPacket packet{readBoolean(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SoundPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, 0);
    writeString(buffer, sound);
    // The optional fixed range of a direct sound event: absent, so the client uses the volume.
    writeBoolean(buffer, false);
    varInt::write(buffer, static_cast<std::int32_t>(source));
    buffer.writeInt(static_cast<std::int32_t>(x * kLocationAccuracy));
    buffer.writeInt(static_cast<std::int32_t>(y * kLocationAccuracy));
    buffer.writeInt(static_cast<std::int32_t>(z * kLocationAccuracy));
    buffer.writeFloat(volume);
    buffer.writeFloat(pitch);
    buffer.writeLong(seed);
}

std::optional<SoundPacket> SoundPacket::read(io::ByteBuffer& buffer) {
    if (varInt::read(buffer) != 0) {
        // A registry index instead of a direct holder: nothing here writes one, and reading one
        // would need the sound_event table this module deliberately does not have.
        return std::nullopt;
    }
    SoundPacket packet;
    packet.sound = readString(buffer);
    if (readBoolean(buffer)) {
        return std::nullopt;
    }
    const std::optional<SoundSource> source = soundSourceFromOrdinal(varInt::read(buffer));
    if (!source) {
        return std::nullopt;
    }
    packet.source = *source;
    packet.x = buffer.readInt() / kLocationAccuracy;
    packet.y = buffer.readInt() / kLocationAccuracy;
    packet.z = buffer.readInt() / kLocationAccuracy;
    packet.volume = buffer.readFloat();
    packet.pitch = buffer.readFloat();
    packet.seed = buffer.readLong();
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void StopSoundPacket::write(io::ByteBuffer& buffer) const {
    std::uint8_t flags = 0;
    if (source) {
        flags |= 1;
    }
    if (sound) {
        flags |= 2;
    }
    buffer.writeByte(flags);
    if (source) {
        varInt::write(buffer, static_cast<std::int32_t>(*source));
    }
    if (sound) {
        writeString(buffer, *sound);
    }
}

std::optional<StopSoundPacket> StopSoundPacket::read(io::ByteBuffer& buffer) {
    const std::uint8_t flags = buffer.readByte();
    StopSoundPacket packet;
    if ((flags & 1) != 0) {
        packet.source = soundSourceFromOrdinal(varInt::read(buffer));
        if (!packet.source) {
            return std::nullopt;
        }
    }
    if ((flags & 2) != 0) {
        packet.sound = readString(buffer);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void CommandSuggestionRequestPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, id);
    writeString(buffer, command);
}

std::optional<CommandSuggestionRequestPacket> CommandSuggestionRequestPacket::read(io::ByteBuffer& buffer) {
    CommandSuggestionRequestPacket packet;
    packet.id = varInt::read(buffer);
    packet.command = readString(buffer, kMaxCommandLength);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void CommandSuggestion::write(io::ByteBuffer& buffer) const {
    writeString(buffer, text);
    writeBoolean(buffer, tooltip.has_value());
    if (tooltip) {
        writeComponent(buffer, *tooltip);
    }
}

CommandSuggestion CommandSuggestion::read(io::ByteBuffer& buffer) {
    CommandSuggestion suggestion;
    suggestion.text = readString(buffer);
    if (readBoolean(buffer)) {
        suggestion.tooltip = readComponent(buffer);
    }
    return suggestion;
}

void CommandSuggestionsPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, id);
    varInt::write(buffer, start);
    varInt::write(buffer, length);
    varInt::write(buffer, static_cast<std::int32_t>(suggestions.size()));
    for (const CommandSuggestion& suggestion : suggestions) {
        suggestion.write(buffer);
    }
}

std::optional<CommandSuggestionsPacket> CommandSuggestionsPacket::read(io::ByteBuffer& buffer) {
    CommandSuggestionsPacket packet;
    packet.id = varInt::read(buffer);
    packet.start = varInt::read(buffer);
    packet.length = varInt::read(buffer);
    const std::int32_t count = readCollectionSize(buffer, 2);
    packet.suggestions.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.suggestions.push_back(CommandSuggestion::read(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
