#pragma once

#include <basalt/wire/bedrock/BedrockKeys.hpp>

#include <filesystem>
#include <memory>

namespace basalt::wire::bedrock {

struct StoredIdentity {
    std::shared_ptr<const EcKeyPair> key;
    bool created = false;
    bool unreadable = false;
    bool saved = true;
};

/// The key a client pins on first use, so it has to survive a restart or every player is asked again.
[[nodiscard]] StoredIdentity loadOrCreateNetherNetIdentity(const std::filesystem::path& path);

} // namespace basalt::wire::bedrock
