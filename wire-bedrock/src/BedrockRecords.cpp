#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockRecords.hpp>
#include <basalt/wire/bedrock/BedrockSounds.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <utility>

namespace basalt::wire::bedrock {
namespace {

/// parity: Geyser plays a record at the block's centre, at the volume and pitch Java 1.21 uses.
constexpr float kRecordVolume = 4.0F;
constexpr float kRecordPitch = 1.0F;

} // namespace

void RecordSounds::started(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                           const std::string_view javaSound) {
    const std::string_view named = bedrockSoundName(javaSound);
    if (named.empty()) {
        return;
    }
    const Position at{x, y, z};
    const double centreX = x + 0.5;
    const double centreY = y + 0.5;
    const double centreZ = z + 0.5;
    io::ByteBuffer sound;
    if (protocol_ < kRecordStartedProtocol) {
        writePlaySound(sound, named, centreX, centreY, centreZ, kRecordVolume, kRecordPitch, protocol_);
        post_(BedrockPacket::PlaySound, sound.toVector());
        names_.insert_or_assign(at, std::string{named});
        return;
    }
    const std::int64_t handle = nextHandle_++;
    handles_.insert_or_assign(at, handle);
    writePlaySound(sound, named, centreX, centreY, centreZ, kRecordVolume, kRecordPitch, protocol_, handle);
    post_(BedrockPacket::PlaySound, sound.toVector());
    io::ByteBuffer record;
    writeRecordStarted(record, x, y, z, handle);
    post_(BedrockPacket::RecordStarted, record.toVector());
}

void RecordSounds::stopped(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const Position at{x, y, z};
    io::ByteBuffer body;
    if (const auto handle = handles_.find(at); handle != handles_.end()) {
        writeStopSoundHandle(body, handle->second, protocol_);
        handles_.erase(handle);
        post_(BedrockPacket::ClientboundUpdateSoundData, body.toVector());
    } else if (const auto name = names_.find(at); name != names_.end()) {
        writeStopSound(body, name->second, false);
        names_.erase(name);
        post_(BedrockPacket::StopSound, body.toVector());
    }
}

} // namespace basalt::wire::bedrock
