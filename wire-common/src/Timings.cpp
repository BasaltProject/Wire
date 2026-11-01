#include <basalt/wire/common/timings/Timings.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>

#if defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

namespace basalt::wire::common {
namespace {

std::atomic<std::uint64_t> nextInstance{1};

struct Frame {
    const Timing* timing = nullptr;
    std::int64_t calls = 0;
    std::int64_t totalNanos = 0;
    std::int64_t maxNanos = 0;
    std::vector<std::int64_t> windowCalls;
    std::vector<std::int64_t> windowNanos;
    std::vector<std::unique_ptr<Frame>> children;

    [[nodiscard]] Frame& child(const Timing& which) {
        for (const std::unique_ptr<Frame>& existing : children) {
            if (existing->timing == &which) {
                return *existing;
            }
        }
        children.push_back(std::make_unique<Frame>());
        children.back()->timing = &which;
        return *children.back();
    }

    void add(std::int64_t nanos, std::size_t window) {
        if (windowCalls.size() <= window) {
            windowCalls.resize(window + 1, 0);
            windowNanos.resize(window + 1, 0);
        }
        windowCalls[window] += 1;
        windowNanos[window] += nanos;
        add(nanos);
    }

    void add(std::int64_t nanos) noexcept {
        calls += 1;
        totalNanos += nanos;
        maxNanos = std::max(maxNanos, nanos);
    }
};

[[nodiscard]] ProfileNode convert(const Frame& frame, std::string name) {
    ProfileNode node;
    node.name = std::move(name);
    node.calls = frame.calls;
    node.totalNanos = frame.totalNanos;
    node.maxNanos = frame.maxNanos;
    node.windowCalls = frame.windowCalls;
    node.windowNanos = frame.windowNanos;
    node.children.reserve(frame.children.size());
    for (const std::unique_ptr<Frame>& child : frame.children) {
        node.children.push_back(convert(*child, child->timing->name()));
    }
    std::ranges::sort(node.children, std::greater<>{}, &ProfileNode::totalNanos);
    return node;
}

} // namespace

std::int64_t ProfileNode::selfNanos() const noexcept {
    std::int64_t inChildren = 0;
    for (const ProfileNode& child : children) {
        inChildren += child.totalNanos;
    }
    return std::max<std::int64_t>(totalNanos - inChildren, 0);
}

struct Timings::ThreadState {
    std::mutex mutex;
    std::string name;
    std::uint64_t generation = 0;
    Frame root;
    std::vector<Frame*> stack;

    void adopt(std::uint64_t current) {
        if (generation != current) {
            root = Frame{};
            stack.clear();
            generation = current;
        }
    }
};

Timings::Timings() : instance_(nextInstance.fetch_add(1, std::memory_order_relaxed)) {
    windowOriginNanos_.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
}

std::size_t Timings::currentWindow() const noexcept {
    const std::int64_t elapsed = std::chrono::steady_clock::now().time_since_epoch().count() -
                                 windowOriginNanos_.load(std::memory_order_relaxed);
    const std::int64_t window = elapsed / (static_cast<std::int64_t>(kWindowSeconds) * 1'000'000'000);
    return static_cast<std::size_t>(std::clamp<std::int64_t>(window, 0, kMaxWindows - 1));
}

Timings::~Timings() = default;

Timings::ThreadState& Timings::threadState() const {
    struct Cache {
        std::uint64_t instance = 0;
        ThreadState* state = nullptr;
    };

    thread_local Cache cache;
    if (cache.instance == instance_) {
        return *cache.state;
    }
    auto created = std::make_shared<ThreadState>();
    {
        const std::scoped_lock lock{threadsMutex_};
        created->name = "thread-" + std::to_string(threads_.size() + 1);
#if defined(__APPLE__) || defined(__linux__)
        std::array<char, 64> label{};
        if (pthread_getname_np(pthread_self(), label.data(), label.size()) == 0 && label[0] != '\0') {
            created->name = label.data();
        }
#endif
        threads_.push_back(created);
    }
    cache = Cache{instance_, created.get()};
    return *created;
}

void Timings::nameThread(std::string_view name) const {
    ThreadState& state = threadState();
    const std::scoped_lock lock{state.mutex};
    state.name = std::string{name};
}

ProfileScope Timings::enterScope(const Timing& timing) const {
    ThreadState& state = threadState();
    const std::uint64_t current = generation_.load(std::memory_order_relaxed);
    const std::scoped_lock lock{state.mutex};
    state.adopt(current);
    Frame& frame = (state.stack.empty() ? state.root : *state.stack.back()).child(timing);
    state.stack.push_back(&frame);
    return ProfileScope{&frame, current};
}

void Timings::leaveScope(ProfileScope scope, std::int64_t nanos) const {
    if (scope.node == nullptr) {
        return;
    }
    ThreadState& state = threadState();
    const std::scoped_lock lock{state.mutex};
    // A reset in between freed the frame: the pointer is only compared, never followed.
    if (state.generation != scope.generation || state.stack.empty() || state.stack.back() != scope.node) {
        return;
    }
    state.stack.back()->add(nanos, currentWindow());
    state.stack.pop_back();
}

void Timings::attribute(const Timing& timing, std::int64_t nanos) const {
    ThreadState& state = threadState();
    const std::uint64_t current = generation_.load(std::memory_order_relaxed);
    const std::scoped_lock lock{state.mutex};
    state.adopt(current);
    (state.stack.empty() ? state.root : *state.stack.back()).child(timing).add(nanos, currentWindow());
}

std::vector<ThreadProfile> Timings::profiles() const {
    std::vector<std::shared_ptr<ThreadState>> threads;
    {
        const std::scoped_lock lock{threadsMutex_};
        threads = threads_;
    }
    const std::uint64_t current = generation_.load(std::memory_order_relaxed);
    std::vector<ThreadProfile> out;
    for (const std::shared_ptr<ThreadState>& state : threads) {
        const std::scoped_lock lock{state->mutex};
        if (state->generation != current || state->root.children.empty()) {
            continue;
        }
        ThreadProfile profile;
        profile.name = state->name;
        profile.root = convert(state->root, "root");
        for (const ProfileNode& child : profile.root.children) {
            profile.root.totalNanos += child.totalNanos;
            profile.root.calls += child.calls;
            if (profile.root.windowNanos.size() < child.windowNanos.size()) {
                profile.root.windowCalls.resize(child.windowNanos.size(), 0);
                profile.root.windowNanos.resize(child.windowNanos.size(), 0);
            }
            for (std::size_t window = 0; window < child.windowNanos.size(); ++window) {
                profile.root.windowCalls[window] += child.windowCalls[window];
                profile.root.windowNanos[window] += child.windowNanos[window];
            }
        }
        out.push_back(std::move(profile));
    }
    std::ranges::sort(out, std::greater<>{}, [](const ThreadProfile& profile) { return profile.root.totalNanos; });
    return out;
}

bool Timing::isEnabled() const noexcept { return owner_->enabled(); }

ProfileScope Timing::enter() const noexcept {
    try {
        return owner_->enterScope(*this);
    } catch (...) {
        return {};
    }
}

void Timing::leave(ProfileScope scope, std::int64_t nanos) noexcept {
    add(nanos);
    try {
        owner_->leaveScope(scope, nanos);
    } catch (...) {
    }
}

void Timing::record(std::int64_t nanos) noexcept {
    add(nanos);
    if (!isEnabled()) {
        return;
    }
    try {
        owner_->attribute(*this, nanos);
    } catch (...) {
    }
}

void Timing::add(std::int64_t nanos) noexcept {
    calls_.fetch_add(1, std::memory_order_relaxed);
    totalNanos_.fetch_add(nanos, std::memory_order_relaxed);
    std::int64_t worst = maxNanos_.load(std::memory_order_relaxed);
    while (nanos > worst && !maxNanos_.compare_exchange_weak(worst, nanos, std::memory_order_relaxed)) {
    }
}

TimingSnapshot Timing::snapshot() const {
    return TimingSnapshot{name_, calls_.load(std::memory_order_relaxed), totalNanos_.load(std::memory_order_relaxed),
                          maxNanos_.load(std::memory_order_relaxed)};
}

void Timing::reset() noexcept {
    calls_.store(0, std::memory_order_relaxed);
    totalNanos_.store(0, std::memory_order_relaxed);
    maxNanos_.store(0, std::memory_order_relaxed);
}

Timing& Timings::of(std::string_view name) {
    const std::scoped_lock lock{mutex_};
    for (const std::unique_ptr<Timing>& timing : timings_) {
        if (timing->name() == name) {
            return *timing;
        }
    }
    timings_.push_back(std::make_unique<Timing>(std::string{name}, *this));
    return *timings_.back();
}

std::vector<TimingSnapshot> Timings::snapshot() const {
    const std::scoped_lock lock{mutex_};
    std::vector<TimingSnapshot> out;
    out.reserve(timings_.size());
    for (const std::unique_ptr<Timing>& timing : timings_) {
        out.push_back(timing->snapshot());
    }
    std::ranges::sort(out, {}, &TimingSnapshot::name);
    return out;
}

void Timings::reset(std::int64_t nowNanos) {
    const std::scoped_lock lock{mutex_};
    for (const std::unique_ptr<Timing>& timing : timings_) {
        timing->reset();
    }
    startedAtNanos_.store(nowNanos, std::memory_order_relaxed);
    windowOriginNanos_.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
    generation_.fetch_add(1, std::memory_order_relaxed);
}

std::int64_t Timings::windowNanos(std::int64_t nowNanos) const noexcept {
    return std::max<std::int64_t>(nowNanos - startedAtNanos_.load(std::memory_order_relaxed), 0);
}

} // namespace basalt::wire::common
