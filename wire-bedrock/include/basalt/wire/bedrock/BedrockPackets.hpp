#pragma once

#include <basalt/wire/bedrock/BedrockPacketIds.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::bedrock {

/// What a client asks for before anything else, and the only packet either side sends in the
/// clear and uncompressed.
struct RequestNetworkSettings {
    int protocolVersion = 0;
};

[[nodiscard]] std::optional<RequestNetworkSettings> readRequestNetworkSettings(io::ByteBuffer& buffer);

void writeNetworkSettings(io::ByteBuffer& buffer, std::uint16_t compressionThreshold, std::uint16_t algorithm);

/// The two halves of a login: what the client claims to be, and the key it wants to talk with.
struct LoginRequest {
    int protocolVersion = 0;
    std::string name;
    std::string xuid;
    std::string identityUuid;
    /// The client's public key, as the DER a JWT header carries.
    std::vector<std::uint8_t> identityPublicKey;
    /// True when Mojang signed the chain, rather than the client signing itself.
    bool authenticated = false;
};

[[nodiscard]] std::optional<LoginRequest> readLogin(io::ByteBuffer& buffer);

/// What a client answers the resource pack question with.
enum class ResourcePackResponse : std::uint8_t {
    Refused = 1,
    SendPacks = 2,
    HaveAllPacks = 3,
    Completed = 4,
};

[[nodiscard]] std::optional<ResourcePackResponse> readResourcePackResponse(io::ByteBuffer& buffer);

/// The states a client is told it is in. The numbers are the protocol's.
enum class PlayStatus : std::int32_t {
    LoginSuccess = 0,
    ClientTooOld = 1,
    ServerTooOld = 2,
    PlayerSpawn = 3,
};

void writePlayStatus(io::ByteBuffer& buffer, PlayStatus status);

void writeServerToClientHandshake(io::ByteBuffer& buffer, std::string_view jwt);

void writeResourcePacksInfo(io::ByteBuffer& buffer);

void writeResourcePackStack(io::ByteBuffer& buffer, std::string_view gameVersion);

void writeDisconnect(io::ByteBuffer& buffer, std::string_view reason);

/// Everything the client needs before it will draw a world. Most of it is the server describing a
/// world nobody configured, so the defaults here are the answer for an ordinary server and only a
/// handful of fields are ever set by a caller.
struct StartGameInfo {
    std::int64_t uniqueEntityId = 1;
    std::uint64_t runtimeEntityId = 1;
    std::int32_t playerGameType = 0;
    float x = 0.0F;
    float y = 64.0F;
    float z = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;

    std::int64_t seed = 0;
    std::int32_t dimension = 0;
    std::int32_t levelGameType = 0;
    std::int32_t difficulty = 2;
    std::int32_t spawnX = 0;
    std::int32_t spawnY = 64;
    std::int32_t spawnZ = 0;
    bool commandsEnabled = true;
    std::int64_t currentTick = 0;

    std::string levelId = "basalt";
    std::string levelName = "Basalt";
    std::string vanillaVersion;
    std::string multiplayerCorrelationId;
    std::string serverId;
    std::string worldId;
};

void writeStartGame(io::ByteBuffer& buffer, const StartGameInfo& info);

/// How far the client should ask the server for, and how far it draws.
void writeChunkRadiusUpdated(io::ByteBuffer& buffer, std::int32_t radius);

[[nodiscard]] std::optional<std::int32_t> readRequestChunkRadius(io::ByteBuffer& buffer);

/// The centre of what the server is willing to send, which is what stops a client waiting for
/// chunks that are not coming.
void writeNetworkChunkPublisherUpdate(io::ByteBuffer& buffer, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::uint32_t radius);

/// An empty chunk column: every subchunk absent, biomes flat, no block entities. It is what a
/// client needs to stop waiting for a chunk it was told to expect.
void writeEmptyLevelChunk(io::ByteBuffer& buffer, std::int32_t chunkX, std::int32_t chunkZ, std::uint32_t subChunkCount,
                          std::int32_t dimension);

} // namespace basalt::wire::bedrock
