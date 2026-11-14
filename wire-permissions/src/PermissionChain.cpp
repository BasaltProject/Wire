#include <basalt/wire/permissions/PermissionChain.hpp>

#include <algorithm>
#include <atomic>
#include <mutex>

namespace basalt::wire::permissions {
namespace {

constexpr int kMaxDepth = 8;

thread_local int gDepth = 0;

struct DepthGuard {
    DepthGuard() noexcept { ++gDepth; }

    ~DepthGuard() { --gDepth; }

    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;

    [[nodiscard]] static bool tooDeep() noexcept { return gDepth >= kMaxDepth; }
};

using Providers = std::vector<std::shared_ptr<const Provider>>;
using SharedProviders = std::shared_ptr<const Providers>;

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
using AtomicProviders = std::atomic<SharedProviders>;
#else
// Older libc++ lacks atomic<shared_ptr>, so publish through the shared_ptr atomic operations.
class AtomicProviders {
public:
    explicit AtomicProviders(SharedProviders value) : value_(std::move(value)) {}

    [[nodiscard]] SharedProviders load(std::memory_order order) const {
        return std::atomic_load_explicit(&value_, order);
    }

    void store(SharedProviders value, std::memory_order order) {
        std::atomic_store_explicit(&value_, std::move(value), order);
    }

private:
    SharedProviders value_;
};
#endif

} // namespace

struct PermissionChain::State {
    std::mutex writer;
    AtomicProviders snapshot{std::make_shared<const Providers>()};
    std::atomic<bool> onlyDefault{true};
    std::atomic<std::uint64_t> version{0};

    void publish(Providers next) {
        const bool plain =
            std::ranges::none_of(next, [](const auto& entry) { return entry->priority > kPriorityDefault; });
        snapshot.store(std::make_shared<const Providers>(std::move(next)), std::memory_order_release);
        onlyDefault.store(plain, std::memory_order_release);
        version.fetch_add(1, std::memory_order_acq_rel);
    }

    void erase(const std::shared_ptr<const Provider>& provider) {
        const std::lock_guard lock{writer};
        Providers next = *snapshot.load(std::memory_order_acquire);
        if (const auto found = std::ranges::find(next, provider); found != next.end()) {
            next.erase(found);
            publish(std::move(next));
        }
    }
};

Tristate PermissionChain::walk(std::span<const std::shared_ptr<const Provider>> providers, const Subject& subject,
                               std::string_view node, const Context& context) {
    for (std::size_t index = 0; index < providers.size(); ++index) {
        const auto& provider = *providers[index];
        if (!provider.check)
            continue;
        const Tristate answer = provider.check(Query{providers.subspan(index + 1), subject, node, context});
        if (answer != Tristate::kUndefined)
            return answer;
    }
    return Tristate::kUndefined;
}

std::string_view Context::value(std::string_view key) const noexcept {
    if (key == kContextServer)
        return server;
    if (key == kContextWorld)
        return world;
    for (const auto& entry : extra)
        if (entry.key == key)
            return entry.value;
    return {};
}

Tristate Query::rest() const {
    if (DepthGuard::tooDeep())
        return Tristate::kUndefined;
    const DepthGuard guard;
    return PermissionChain::walk(below_, *subject_, node_, *context_);
}

ProviderHandle::ProviderHandle(ProviderHandle&& other) noexcept
    : owner_(std::move(other.owner_)), provider_(std::move(other.provider_)) {
    other.owner_.reset();
}

ProviderHandle& ProviderHandle::operator=(ProviderHandle&& other) noexcept {
    if (this != &other) {
        remove();
        owner_ = std::move(other.owner_);
        provider_ = std::move(other.provider_);
        other.owner_.reset();
    }
    return *this;
}

ProviderHandle::~ProviderHandle() { remove(); }

void ProviderHandle::remove() {
    if (const auto owner = owner_.lock())
        static_cast<PermissionChain::State*>(owner.get())->erase(provider_);
    owner_.reset();
    provider_.reset();
}

PermissionChain::PermissionChain() : state_(std::make_shared<State>()) {}

PermissionChain::~PermissionChain() = default;

ProviderHandle PermissionChain::provide(Provider provider) {
    auto shared = std::make_shared<const Provider>(std::move(provider));
    {
        const std::lock_guard lock{state_->writer};
        Providers next = *state_->snapshot.load(std::memory_order_acquire);
        const auto at =
            std::ranges::find_if(next, [&](const auto& entry) { return entry->priority < shared->priority; });
        next.insert(at, shared);
        state_->publish(std::move(next));
    }
    return ProviderHandle{std::weak_ptr<void>{state_}, std::move(shared)};
}

Tristate PermissionChain::check(const Subject& subject, std::string_view node, const Context& context) const {
    if (DepthGuard::tooDeep())
        return Tristate::kUndefined;
    const DepthGuard guard;
    const auto providers = state_->snapshot.load(std::memory_order_acquire);
    return walk(*providers, subject, node, context);
}

std::optional<std::string> PermissionChain::meta(const Subject& subject, std::string_view key,
                                                 const Context& context) const {
    if (DepthGuard::tooDeep())
        return std::nullopt;
    const DepthGuard guard;
    const auto providers = state_->snapshot.load(std::memory_order_acquire);
    for (const auto& provider : *providers)
        if (provider->meta)
            if (auto value = provider->meta(subject, key, context))
                return value;
    return std::nullopt;
}

bool PermissionChain::onlyDefault() const noexcept { return state_->onlyDefault.load(std::memory_order_acquire); }

std::vector<std::string> PermissionChain::ids() const {
    std::vector<std::string> result;
    for (const auto& provider : *state_->snapshot.load(std::memory_order_acquire))
        result.push_back(provider->id);
    return result;
}

void PermissionChain::changed() noexcept { state_->version.fetch_add(1, std::memory_order_acq_rel); }

std::uint64_t PermissionChain::version() const noexcept { return state_->version.load(std::memory_order_acquire); }

} // namespace basalt::wire::permissions
