#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <utility>

namespace basalt::wire::auth {
namespace {

/// One string member of a JSON object, or nothing when it is absent or not a string.
[[nodiscard]] std::optional<std::string> stringMember(const common::JsonObject& object, std::string_view key) {
    const common::JsonValue* value = object.find(key);
    const auto* text = value == nullptr ? nullptr : value->get<std::string>();
    return text == nullptr ? std::nullopt : std::optional{*text};
}

/// The profile the session server answered with: an undashed uuid, a name, and the signed
/// properties a skin travels in.
[[nodiscard]] std::optional<SessionProfile> profileFrom(const std::string& body) {
    const std::expected<common::JsonValue, common::JsonError> parsed = common::parseJson(body);
    const auto* object = parsed.has_value() ? parsed->get<common::JsonObject>() : nullptr;
    if (object == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::string> id = stringMember(*object, "id");
    const std::optional<std::string> name = stringMember(*object, "name");
    if (!id.has_value() || !name.has_value()) {
        return std::nullopt;
    }
    const std::optional<common::Uuid> uuid = common::Uuid::parse(*id);
    if (!uuid.has_value()) {
        return std::nullopt;
    }
    SessionProfile profile{.id = *uuid, .name = *name, .properties = {}};
    const common::JsonValue* properties = object->find("properties");
    const auto* list = properties == nullptr ? nullptr : properties->get<std::vector<common::JsonValue>>();
    if (list != nullptr) {
        for (const common::JsonValue& entry : *list) {
            const auto* fields = entry.get<common::JsonObject>();
            if (fields == nullptr) {
                continue;
            }
            const std::optional<std::string> propertyName = stringMember(*fields, "name");
            const std::optional<std::string> value = stringMember(*fields, "value");
            if (!propertyName.has_value() || !value.has_value()) {
                continue;
            }
            profile.properties.push_back(SessionProfileProperty{
                .name = *propertyName, .value = *value, .signature = stringMember(*fields, "signature")});
        }
    }
    return profile;
}

/// A dashless uuid as every other one this server prints.
[[nodiscard]] std::string dashed(const std::string& id) {
    if (id.size() != 32) {
        return id;
    }
    return id.substr(0, 8) + "-" + id.substr(8, 4) + "-" + id.substr(12, 4) + "-" + id.substr(16, 4) + "-" +
           id.substr(20, 12);
}

} // namespace

MojangSessionService::MojangSessionService(Fetch fetch) : fetch_(std::move(fetch)) {
    if (!fetch_) {
        fetch_ = [](std::string_view url) { return httpsGet(url); };
    }
}

std::optional<SessionProfile> MojangSessionService::hasJoined(std::string_view username, std::string_view serverHash,
                                                              std::string_view clientAddress) {
    std::string url = "https://sessionserver.mojang.com/session/minecraft/hasJoined?username=" + std::string{username} +
                      "&serverId=" + std::string{serverHash};
    // Sent only when the operator asked for it: it is the player's own address, and Mojang uses it
    // to tie the session to where the login came from.
    if (!clientAddress.empty()) {
        url += "&ip=" + std::string{clientAddress};
    }
    const std::expected<HttpsResponse, HttpsError> answered = fetch_(url);
    // A 204 with no body is "that is not them", and so is anything that is not a 200 with JSON.
    if (!answered.has_value() || !answered->ok() || answered->body.empty()) {
        return std::nullopt;
    }
    return profileFrom(answered->text());
}

std::optional<std::string> mojangUuidOf(const std::string& name, const MojangSessionService::Fetch& fetch) {
    const MojangSessionService::Fetch get =
        fetch ? fetch : MojangSessionService::Fetch{[](std::string_view url) { return httpsGet(url); }};
    const std::expected<HttpsResponse, HttpsError> answered =
        get("https://api.mojang.com/users/profiles/minecraft/" + name);
    if (!answered.has_value() || !answered->ok() || answered->body.empty()) {
        return std::nullopt;
    }
    const std::expected<common::JsonValue, common::JsonError> parsed = common::parseJson(answered->text());
    const auto* object = parsed.has_value() ? parsed->get<common::JsonObject>() : nullptr;
    if (object == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::string> id = stringMember(*object, "id");
    return id.has_value() ? std::optional{dashed(*id)} : std::nullopt;
}

} // namespace basalt::wire::auth
