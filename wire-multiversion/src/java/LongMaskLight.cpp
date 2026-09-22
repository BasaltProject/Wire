#include <basalt/wire/multiversion/java/LongMaskLight.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

void writeMask(io::ByteBuffer& output, const std::vector<std::int64_t>& words) {
    protocol::varInt::write(output, static_cast<std::int32_t>(words.size()));
    for (const std::int64_t word : words) {
        output.writeLong(word);
    }
}

std::vector<std::int64_t> readMask(io::ByteBuffer& input) {
    const std::int32_t count = protocol::readCollectionSize(input, 8);
    std::vector<std::int64_t> words;
    words.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        words.push_back(input.readLong());
    }
    return words;
}

void writeArrays(io::ByteBuffer& output, const std::vector<std::vector<std::uint8_t>>& arrays) {
    protocol::varInt::write(output, static_cast<std::int32_t>(arrays.size()));
    for (const std::vector<std::uint8_t>& array : arrays) {
        protocol::writeByteArray(output, array);
    }
}

std::vector<std::vector<std::uint8_t>> readArrays(io::ByteBuffer& input) {
    const std::int32_t count = protocol::readCollectionSize(input);
    std::vector<std::vector<std::uint8_t>> arrays;
    arrays.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        arrays.push_back(protocol::readByteArray(input));
    }
    return arrays;
}

} // namespace

void writeLongMaskLight(io::ByteBuffer& output, const protocol::play::LightData& light) {
    writeMask(output, light.skyLightMask);
    writeMask(output, light.blockLightMask);
    writeMask(output, light.emptySkyLightMask);
    writeMask(output, light.emptyBlockLightMask);
    writeArrays(output, light.skyLight);
    writeArrays(output, light.blockLight);
}

std::optional<protocol::play::LightData> readLongMaskLight(io::ByteBuffer& input) {
    protocol::play::LightData light;
    light.skyLightMask = readMask(input);
    light.blockLightMask = readMask(input);
    light.emptySkyLightMask = readMask(input);
    light.emptyBlockLightMask = readMask(input);
    light.skyLight = readArrays(input);
    light.blockLight = readArrays(input);
    return input.failed() ? std::nullopt : std::optional{std::move(light)};
}

} // namespace basalt::wire::multiversion::java
