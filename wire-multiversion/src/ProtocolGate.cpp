#include <basalt/wire/multiversion/ProtocolGate.hpp>

#include <cstdint>

namespace basalt::wire::multiversion {
namespace {

ProtocolVerdict unknownVerdict(std::int32_t protocol, UnknownProtocolPolicy policy,
                               ProtocolVerdict::Refused::Reason refusal) {
    switch (policy) {
    case UnknownProtocolPolicy::Accept:
        return ProtocolVerdict{ProtocolVerdict::Unknown{protocol, false}};
    case UnknownProtocolPolicy::Warn:
        return ProtocolVerdict{ProtocolVerdict::Unknown{protocol, true}};
    case UnknownProtocolPolicy::Refuse:
        return ProtocolVerdict{ProtocolVerdict::Refused{protocol, refusal}};
    }
    return ProtocolVerdict{ProtocolVerdict::Refused{protocol, refusal}};
}

bool equalsIgnoringCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char left = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] + 32) : a[i];
        const char right = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char>(b[i] + 32) : b[i];
        if (left != right) {
            return false;
        }
    }
    return true;
}

std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

} // namespace

std::optional<UnknownProtocolPolicy> parseUnknownProtocolPolicy(std::string_view name) {
    const std::string_view written = trim(name);
    if (equalsIgnoringCase(written, "accept")) {
        return UnknownProtocolPolicy::Accept;
    }
    if (equalsIgnoringCase(written, "warn")) {
        return UnknownProtocolPolicy::Warn;
    }
    if (equalsIgnoringCase(written, "refuse")) {
        return UnknownProtocolPolicy::Refuse;
    }
    return std::nullopt;
}

ProtocolVerdict ProtocolGate::verdictFor(std::int32_t protocol) const {
    if (protocol == native_) {
        return ProtocolVerdict{ProtocolVerdict::Native{}};
    }
    if (protocolVersions::isSnapshot(protocol)) {
        return unknownVerdict(protocol, snapshotPolicy_, ProtocolVerdict::Refused::Reason::Snapshot);
    }
    if (protocol > native_) {
        return unknownVerdict(protocol, futurePolicy_, ProtocolVerdict::Refused::Reason::TooNew);
    }

    const std::optional<ProtocolVersion> version = protocolVersions::of(protocol);
    if (!version || protocol < protocolVersions::oldest()) {
        return ProtocolVerdict{ProtocolVerdict::Refused{protocol, ProtocolVerdict::Refused::Reason::TooOld}};
    }
    if (!supported_.contains(protocol)) {
        return ProtocolVerdict{ProtocolVerdict::Refused{protocol, ProtocolVerdict::Refused::Reason::NotTranslated}};
    }
    return ProtocolVerdict{ProtocolVerdict::Translated{*version}};
}

} // namespace basalt::wire::multiversion
