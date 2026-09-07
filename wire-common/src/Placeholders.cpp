#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <algorithm>
#include <cstdint>
#include <format>

namespace basalt::wire::common::placeholder {

PlaceholderValue PlaceholderValue::of(std::string value) {
    return PlaceholderValue{.kind = PlaceholderKind::Text, .text = std::move(value), .reason = std::nullopt};
}

PlaceholderValue PlaceholderValue::of(std::int64_t value) { return of(std::to_string(value)); }

PlaceholderValue PlaceholderValue::of(double value) {
    // A decimal point always, so a whole number still reads as a measurement: "20.0" ticks a
    // second, not "20". Very large and very small numbers come out in the shortest round-trip form
    // the platform writes, which is the exponent notation nobody is comparing against by hand.
    std::string written = std::format("{}", value);
    if (written.find('.') == std::string::npos && written.find('e') == std::string::npos &&
        written.find("inf") == std::string::npos && written.find("nan") == std::string::npos) {
        written += ".0";
    }
    return of(std::move(written));
}

PlaceholderValue PlaceholderValue::of(bool value) { return of(std::string{value ? "true" : "false"}); }

PlaceholderValue PlaceholderValue::unavailable(std::optional<std::string> reason) {
    return PlaceholderValue{.kind = PlaceholderKind::Unavailable, .text = {}, .reason = std::move(reason)};
}

PlaceholderValue PlaceholderValue::unknown() {
    return PlaceholderValue{.kind = PlaceholderKind::Unknown, .text = {}, .reason = std::nullopt};
}

const void* PlaceholderContext::find(const void* slot) const {
    for (const std::pair<const void*, const void*>& entry : values_) {
        if (entry.first == slot) {
            return entry.second;
        }
    }
    return nullptr;
}

PlaceholderContext PlaceholderContext::withRaw(const void* slot, const void* value) const {
    PlaceholderContext next;
    next.values_.reserve(values_.size() + 1);
    for (const std::pair<const void*, const void*>& entry : values_) {
        if (entry.first != slot) {
            next.values_.push_back(entry);
        }
    }
    next.values_.emplace_back(slot, value);
    return next;
}

std::string lowercaseAscii(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (const char character : value) {
        lowered += character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
    }
    return lowered;
}

std::vector<std::string> PlaceholderRegistry::identifiers() const {
    std::vector<std::string> named;
    named.reserve(providers_.size());
    for (const Registration& registration : providers_) {
        named.push_back(registration.identifier);
    }
    return named;
}

void PlaceholderRegistry::registerProvider(std::string_view identifier, PlaceholderProvider provider,
                                           PlaceholderCatalog names) {
    std::string lowercase = lowercaseAscii(identifier);
    for (Registration& registration : providers_) {
        if (registration.identifier == lowercase) {
            registration.provider = std::move(provider);
            registration.names = std::move(names);
            return;
        }
    }
    providers_.push_back(
        Registration{.identifier = std::move(lowercase), .provider = std::move(provider), .names = std::move(names)});
}

void PlaceholderRegistry::unregisterProvider(std::string_view identifier) {
    const std::string lowercase = lowercaseAscii(identifier);
    std::erase_if(providers_,
                  [&lowercase](const Registration& registration) { return registration.identifier == lowercase; });
}

const PlaceholderRegistry::Registration* PlaceholderRegistry::registrationFor(std::string_view lowercaseName) const {
    const Registration* best = nullptr;
    for (const Registration& registration : providers_) {
        const std::string_view identifier = registration.identifier;
        const bool serves = lowercaseName == identifier ||
                            (lowercaseName.starts_with(identifier) && lowercaseName.size() > identifier.size() &&
                             lowercaseName[identifier.size()] == '_');
        if (!serves) {
            continue;
        }
        // The longest match wins, which is what keeps `%basalt_server_online%` apart from a plugin
        // that registered `basalt`.
        if (best == nullptr || identifier.size() > best->identifier.size()) {
            best = &registration;
        }
    }
    return best;
}

std::optional<std::string> PlaceholderRegistry::identifierFor(std::string_view name) const {
    const Registration* found = registrationFor(lowercaseAscii(name));
    return found == nullptr ? std::nullopt : std::optional<std::string>{found->identifier};
}

std::vector<std::string> PlaceholderRegistry::namesOf(std::string_view identifier) const {
    const std::string lowercase = lowercaseAscii(identifier);
    for (const Registration& registration : providers_) {
        if (registration.identifier == lowercase) {
            return registration.names ? registration.names() : std::vector<std::string>{};
        }
    }
    return {};
}

PlaceholderValue PlaceholderRegistry::resolve(std::string_view name, const PlaceholderContext& context) const {
    // Through one matching, not a second copy of it: the copy is what let a broken one of them sit
    // behind a working one.
    const std::string lowercase = lowercaseAscii(name);
    const Registration* found = registrationFor(lowercase);
    if (found == nullptr) {
        return PlaceholderValue::unknown();
    }
    std::string_view params{lowercase};
    params.remove_prefix(found->identifier.size());
    if (params.starts_with('_')) {
        params.remove_prefix(1);
    }
    return found->provider(params, context);
}

} // namespace basalt::wire::common::placeholder
