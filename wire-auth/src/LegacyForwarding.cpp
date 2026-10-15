#include <basalt/wire/auth/forwarding/LegacyForwarding.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <vector>

namespace basalt::wire::auth::forwarding::legacy {
namespace {

constexpr char kSeparator = '\0';

constexpr std::string_view kFloodgateV1 = "Geyser-Floodgate";
constexpr std::string_view kFloodgateV2 = "^Floodgate^";

std::vector<std::string_view> split(std::string_view host) {
    std::vector<std::string_view> raw;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= host.size(); ++i) {
        if (i == host.size() || host[i] == kSeparator) {
            raw.push_back(host.substr(start, i - start));
            start = i + 1;
        }
    }

    std::vector<std::string_view> parts;
    parts.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == kFloodgateV1) {
            ++i;
        } else if (!raw[i].starts_with(kFloodgateV2)) {
            parts.push_back(raw[i]);
        }
    }
    return parts;
}

std::optional<std::vector<SessionProfileProperty>> parseProperties(std::string_view json) {
    const auto parsed = common::parseJson(json);
    if (!parsed) {
        return std::nullopt;
    }
    const auto* array = parsed->get<std::vector<common::JsonValue>>();
    if (array == nullptr) {
        return std::nullopt;
    }

    std::vector<SessionProfileProperty> properties;
    properties.reserve(array->size());
    for (const common::JsonValue& element : *array) {
        const auto* object = element.get<common::JsonObject>();
        if (object == nullptr) {
            return std::nullopt;
        }
        const auto text = [object](std::string_view key) -> std::optional<std::string> {
            const common::JsonValue* value = object->find(key);
            if (value == nullptr) {
                return std::nullopt;
            }
            const auto* string = value->get<std::string>();
            return string != nullptr ? std::optional<std::string>{*string} : std::nullopt;
        };

        const std::optional<std::string> name = text("name");
        const std::optional<std::string> value = text("value");
        if (!name || !value) {
            return std::nullopt;
        }
        std::optional<std::string> signature = text("signature");
        if (signature && signature->empty()) {
            signature.reset();
        }
        properties.push_back(SessionProfileProperty{*name, *value, std::move(signature)});
    }
    return properties;
}

std::string writeProperties(const std::vector<SessionProfileProperty>& properties) {
    std::vector<common::JsonValue> array;
    array.reserve(properties.size());
    for (const SessionProfileProperty& property : properties) {
        common::JsonObject object;
        object.set("name", common::JsonValue::of(property.name));
        object.set("value", common::JsonValue::of(property.value));
        object.set("signature", common::JsonValue::of(property.signature.value_or("")));
        array.push_back(common::JsonValue::ofObject(std::move(object)));
    }
    return common::writeJson(common::JsonValue::ofArray(std::move(array)));
}

struct TokenCheck {
    std::optional<std::string> failure;
    std::vector<SessionProfileProperty> properties;
};

TokenCheck checkToken(std::vector<SessionProfileProperty> properties, const ProxyForwarding& forwarding) {
    if (forwarding.mode == ForwardingMode::BungeeCord) {
        return TokenCheck{std::nullopt, std::move(properties)};
    }

    const std::string_view property = forwarding.tokenProperty();
    std::vector<const SessionProfileProperty*> carried;
    for (const SessionProfileProperty& candidate : properties) {
        if (candidate.name == property) {
            carried.push_back(&candidate);
        }
    }

    if (carried.size() != 1) {
        const std::string reason = carried.empty()
                                       ? "the forwarded profile carries no \"" + std::string{property} + "\" property"
                                       : "the forwarded profile carries " + std::to_string(carried.size()) + " \"" +
                                             std::string{property} + "\" properties, expected one";
        return TokenCheck{reason, {}};
    }

    bool matched = false;
    for (const std::string& token : forwarding.tokens) {
        if (token == carried.front()->value) {
            matched = true;
            break;
        }
    }
    if (!matched) {
        return TokenCheck{"the \"" + std::string{property} + "\" property does not match any configured token", {}};
    }

    std::vector<SessionProfileProperty> kept;
    kept.reserve(properties.size() - 1);
    for (SessionProfileProperty& candidate : properties) {
        if (candidate.name != property) {
            kept.push_back(std::move(candidate));
        }
    }
    return TokenCheck{std::nullopt, std::move(kept)};
}

} // namespace

ForwardingResult decode(std::string_view host, const ProxyForwarding& forwarding) {
    if (const std::optional<std::string> problem = forwarding.misconfiguration()) {
        return *problem;
    }

    const std::vector<std::string_view> parts = split(host);
    if (parts.size() < 3) {
        return "the handshake carries no forwarded data (" + std::to_string(parts.size()) +
               " of at least 3 parts); either the proxy is not forwarding or somebody connected to this port "
               "directly";
    }

    const std::optional<common::Uuid> profileId = common::Uuid::parse(parts[2]);
    if (!profileId) {
        return "forwarded uuid \"" + std::string{parts[2]} + "\" is not a uuid";
    }

    std::vector<SessionProfileProperty> properties;
    if (parts.size() > 3 && !parts[3].empty()) {
        auto parsed = parseProperties(parts[3]);
        if (!parsed) {
            return std::string{"forwarded properties are not JSON"};
        }
        properties = std::move(*parsed);
    }

    TokenCheck checked = checkToken(std::move(properties), forwarding);
    if (checked.failure) {
        return *checked.failure;
    }

    return ForwardedLogin{std::string{parts[0]}, std::string{parts[1]}, *profileId, std::nullopt,
                          std::move(checked.properties)};
}

std::string encode(std::string_view host, std::string_view address, const common::Uuid& profileId,
                   const std::vector<SessionProfileProperty>& properties, std::string_view tokenProperty,
                   std::string_view token) {
    std::vector<SessionProfileProperty> carried = properties;
    if (!tokenProperty.empty() && !token.empty()) {
        carried.push_back(SessionProfileProperty{std::string{tokenProperty}, std::string{token}, std::nullopt});
    }

    std::string out;
    out += host;
    out += kSeparator;
    out += address;
    out += kSeparator;
    out += profileId.toUndashedString();
    out += kSeparator;
    out += writeProperties(carried);
    return out;
}

} // namespace basalt::wire::auth::forwarding::legacy
