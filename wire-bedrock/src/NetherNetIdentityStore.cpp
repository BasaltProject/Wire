#include <basalt/wire/bedrock/NetherNetIdentityStore.hpp>

#include <fstream>
#include <iterator>
#include <vector>

namespace basalt::wire::bedrock {

StoredIdentity loadOrCreateNetherNetIdentity(const std::filesystem::path& path) {
    StoredIdentity result;
    if (std::ifstream in{path, std::ios::binary}; in) {
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        if (auto key = EcKeyPair::fromPrivateDer(bytes)) {
            result.key = std::make_shared<const EcKeyPair>(std::move(*key));
            return result;
        }
        result.unreadable = true;
    }
    auto key = EcKeyPair::generate();
    if (!key) {
        return result;
    }
    result.created = true;
    if (const auto der = key->privateKeyDer(); der) {
        std::error_code ignored;
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path(), ignored);
        }
        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        out.write(reinterpret_cast<const char*>(der->data()), static_cast<std::streamsize>(der->size()));
        result.saved = static_cast<bool>(out);
    }
    result.key = std::make_shared<const EcKeyPair>(std::move(*key));
    return result;
}

} // namespace basalt::wire::bedrock
