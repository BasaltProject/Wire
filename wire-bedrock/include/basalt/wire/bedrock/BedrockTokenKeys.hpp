#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::bedrock {

struct JwtParts;

class MojangTokenKeys {
public:
    ~MojangTokenKeys();

    [[nodiscard]] static MojangTokenKeys& instance();

    /// Idempotent: starts the background fetch of the signing keys, which retries until it works.
    void start();

    /// Needs start() first; false while the keys are not loaded, so a login is never refused over the key service.
    [[nodiscard]] bool verify(const JwtParts& token);

    void install(std::string issuer, std::vector<std::pair<std::string, std::vector<std::uint8_t>>> keys);

private:
    MojangTokenKeys();

    struct State;
    std::unique_ptr<State> state_;
};

} // namespace basalt::wire::bedrock
