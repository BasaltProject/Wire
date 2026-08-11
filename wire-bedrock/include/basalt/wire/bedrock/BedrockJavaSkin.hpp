#pragma once

#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>

#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace basalt::wire::bedrock {

/// The skin a Java account carries in its profile, fetched and decoded; nullopt when it has none or cannot be read.
[[nodiscard]] std::optional<SkinInfo> javaSkinOf(std::span<const protocol::login::GameProfileProperty> properties);

class SkinCache {
public:
    void remember(const common::Uuid& who, SkinInfo skin);

    void forget(const common::Uuid& who);

    [[nodiscard]] std::optional<SkinInfo> of(const common::Uuid& who) const;

    /// Starts fetching a Java account's skin once; true while it is still on its way, false when held back by the cap.
    static bool fetchJava(const std::shared_ptr<SkinCache>& self, const common::Uuid& who,
                          std::vector<protocol::login::GameProfileProperty> properties,
                          std::size_t maxInFlight = std::numeric_limits<std::size_t>::max());

private:
    mutable std::mutex lock_;
    std::unordered_map<common::Uuid, SkinInfo> skins_;
    std::unordered_map<common::Uuid, bool> fetching_;
    std::size_t inFlight_ = 0;
};

} // namespace basalt::wire::bedrock
