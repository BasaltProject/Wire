#pragma once

#include <basalt/wire/bedrock/BedrockPacketIds.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::bedrock {

/// Which record each jukebox plays for one client, so a stop reaches the sound a start began.
class RecordSounds {
public:
    using Post = std::function<void(BedrockPacket, std::vector<std::uint8_t>)>;

    RecordSounds(int protocol, Post post) : protocol_(protocol), post_(std::move(post)) {}

    void started(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view javaSound);

    void stopped(std::int32_t x, std::int32_t y, std::int32_t z);

private:
    using Position = std::array<std::int32_t, 3>;

    int protocol_;
    Post post_;
    std::map<Position, std::int64_t> handles_;
    std::map<Position, std::string> names_;
    std::int64_t nextHandle_ = 0;
};

} // namespace basalt::wire::bedrock
