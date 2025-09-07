#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

namespace basalt::wire::protocol::status {
namespace {

using common::JsonObject;
using common::JsonValue;

constexpr std::string_view kFaviconPrefix = "data:image/png;base64,";

std::optional<std::string> stringField(const JsonObject& object, std::string_view key) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* text = value->get<std::string>();
    return text != nullptr ? std::optional<std::string>{*text} : std::nullopt;
}

std::optional<std::int32_t> intField(const JsonObject& object, std::string_view key) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* integer = value->get<std::int64_t>()) {
        return static_cast<std::int32_t>(*integer);
    }
    if (const auto* number = value->get<double>()) {
        return static_cast<std::int32_t>(*number);
    }
    return std::nullopt;
}

} // namespace

std::string ServerStatusPayload::toJsonString() const {
    JsonObject root;
    root.set("description", text::componentJson::serialize(description));

    if (players) {
        JsonObject body;
        body.set("max", JsonValue::of(static_cast<std::int64_t>(players->max)));
        body.set("online", JsonValue::of(static_cast<std::int64_t>(players->online)));
        std::vector<JsonValue> sample;
        sample.reserve(players->sample.size());
        for (const SamplePlayer& entry : players->sample) {
            JsonObject player;
            player.set("name", JsonValue::of(entry.name));
            player.set("id", JsonValue::of(entry.id));
            sample.push_back(JsonValue::ofObject(std::move(player)));
        }
        body.set("sample", JsonValue::ofArray(std::move(sample)));
        root.set("players", JsonValue::ofObject(std::move(body)));
    }

    if (version) {
        JsonObject body;
        body.set("name", JsonValue::of(version->name));
        body.set("protocol", JsonValue::of(static_cast<std::int64_t>(version->protocol)));
        root.set("version", JsonValue::ofObject(std::move(body)));
    }

    if (favicon) {
        root.set("favicon", JsonValue::of(std::string{kFaviconPrefix} + common::base64::encode(*favicon)));
    }

    root.set("enforcesSecureChat", JsonValue::of(enforcesSecureChat));
    return common::writeJson(JsonValue::ofObject(std::move(root)));
}

std::optional<ServerStatusPayload> ServerStatusPayload::fromJsonString(std::string_view text) {
    const auto parsed = common::parseJson(text);
    if (!parsed) {
        return std::nullopt;
    }
    const auto* root = parsed->get<JsonObject>();
    if (root == nullptr) {
        return std::nullopt;
    }

    ServerStatusPayload payload;
    if (const JsonValue* description = root->find("description")) {
        payload.description = text::componentJson::deserialize(*description);
    }

    if (const JsonValue* playersValue = root->find("players")) {
        if (const auto* body = playersValue->get<JsonObject>()) {
            Players players;
            players.max = intField(*body, "max").value_or(0);
            players.online = intField(*body, "online").value_or(0);
            if (const JsonValue* sample = body->find("sample")) {
                if (const auto* array = sample->get<std::vector<JsonValue>>()) {
                    for (const JsonValue& entry : *array) {
                        if (const auto* player = entry.get<JsonObject>()) {
                            players.sample.push_back(SamplePlayer{stringField(*player, "name").value_or(""),
                                                                  stringField(*player, "id").value_or("")});
                        }
                    }
                }
            }
            payload.players = std::move(players);
        }
    }

    if (const JsonValue* versionValue = root->find("version")) {
        if (const auto* body = versionValue->get<JsonObject>()) {
            payload.version = Version{stringField(*body, "name").value_or(""), intField(*body, "protocol").value_or(0)};
        }
    }

    if (const std::optional<std::string> favicon = stringField(*root, "favicon")) {
        if (favicon->starts_with(kFaviconPrefix)) {
            payload.favicon = common::base64::decode(std::string_view{*favicon}.substr(kFaviconPrefix.size()));
        }
    }

    if (const JsonValue* secure = root->find("enforcesSecureChat")) {
        if (const auto* flag = secure->get<bool>()) {
            payload.enforcesSecureChat = *flag;
        }
    }
    return payload;
}

void StatusResponsePacket::write(io::ByteBuffer& buffer) const { writeString(buffer, status.toJsonString()); }

std::optional<StatusResponsePacket> StatusResponsePacket::read(io::ByteBuffer& buffer) {
    const std::string json = readString(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    auto payload = ServerStatusPayload::fromJsonString(json);
    if (!payload) {
        return std::nullopt;
    }
    return StatusResponsePacket{std::move(*payload)};
}

} // namespace basalt::wire::protocol::status
