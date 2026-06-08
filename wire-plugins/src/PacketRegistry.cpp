#include <basalt/wire/plugins/PacketRegistry.hpp>

#include <algorithm>
#include <cstdint>
#include <thread>
#include <utility>

namespace basalt::wire::plugins {
namespace {
[[nodiscard]] std::size_t phaseSlot(std::int32_t direction, std::int32_t phase) {
    return static_cast<std::size_t>(direction * kPacketPhaseCount + phase);
}
} // namespace

void PacketRegistry::count(const PacketScope& scope, std::int32_t delta) {
    const bool anyDirection = scope.direction < 0;
    const bool anyPhase = scope.phase < 0;
    if (scope.id < 0) {
        if (anyDirection && anyPhase) {
            everything_.fetch_add(delta);
        } else if (anyPhase) {
            perDirection_[static_cast<std::size_t>(scope.direction)].fetch_add(delta);
        } else {
            for (std::int32_t direction = 0; direction < 2; direction++) {
                if (anyDirection || direction == scope.direction) {
                    perPhase_[phaseSlot(direction, scope.phase)].fetch_add(delta);
                }
            }
        }
        return;
    }
    // An id is always named with its phase and direction, so a wildcard on those with an id never happens.
    if (anyDirection || anyPhase || scope.id >= kCountedIds) {
        oddIds_.fetch_add(delta);
        return;
    }
    perId_[phaseSlot(scope.direction, scope.phase) * kCountedIds + static_cast<std::size_t>(scope.id)].fetch_add(delta);
}

PluginHandle PacketRegistry::insert(std::shared_ptr<Listener> listener) {
    const std::int64_t id = listener->id;
    const std::lock_guard<std::mutex> guard{mutex_};
    count(listener->scope, 1);
    auto next = std::make_shared<Snapshot>(*listeners_);
    next->push_back(std::move(listener));
    std::ranges::stable_sort(
        *next, [](const std::shared_ptr<const Listener>& left, const std::shared_ptr<const Listener>& right) {
            if (left->observer != right->observer) {
                return !left->observer;
            }
            return left->priority < right->priority;
        });
    listeners_ = std::move(next);
    return PluginHandle{[this, id] { remove(id); }};
}

void PacketRegistry::remove(std::int64_t id) {
    const std::lock_guard<std::mutex> guard{mutex_};
    auto next = std::make_shared<Snapshot>();
    next->reserve(listeners_->size());
    for (const std::shared_ptr<const Listener>& entry : *listeners_) {
        if (entry->id == id) {
            count(entry->scope, -1);
        } else {
            next->push_back(entry);
        }
    }
    listeners_ = std::move(next);
}

PluginHandle PacketRegistry::add(const void* owner, const PacketScope& scope, lua::LuaRef fn, std::int32_t priority,
                                 bool observer) {
    auto listener = std::make_shared<Listener>();
    listener->scope = scope;
    listener->owner = owner;
    listener->fn = std::move(fn);
    listener->priority = priority;
    listener->observer = observer;
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        listener->id = nextId_++;
    }
    return insert(std::move(listener));
}

PluginHandle PacketRegistry::addNative(const PacketScope& scope, std::int32_t priority, bool observer,
                                       NativePacketListener listener) {
    auto entry = std::make_shared<Listener>();
    entry->scope = scope;
    entry->native = std::move(listener);
    entry->priority = priority;
    entry->observer = observer;
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        entry->id = nextId_++;
    }
    return insert(std::move(entry));
}

bool PacketRegistry::listening(const PacketAddress& address) const noexcept {
    const std::int32_t direction = address.serverbound ? 1 : 0;
    const std::int32_t phase = static_cast<std::int32_t>(address.phase);
    if (everything_.load(std::memory_order_relaxed) > 0 ||
        perDirection_[static_cast<std::size_t>(direction)].load(std::memory_order_relaxed) > 0 ||
        perPhase_[phaseSlot(direction, phase)].load(std::memory_order_relaxed) > 0 ||
        oddIds_.load(std::memory_order_relaxed) > 0) {
        return true;
    }
    return address.id >= 0 && address.id < kCountedIds &&
           perId_[phaseSlot(direction, phase) * kCountedIds + static_cast<std::size_t>(address.id)].load(
               std::memory_order_relaxed) > 0;
}

void PacketRegistry::drain() const {
    while (inFlight_.load(std::memory_order_acquire) > 0) {
        std::this_thread::yield();
    }
}

bool PacketRegistry::fire(PluginPacket& packet) {
    struct Flight {
        std::atomic<std::int32_t>& count;

        explicit Flight(std::atomic<std::int32_t>& counter) : count(counter) { count.fetch_add(1); }

        ~Flight() { count.fetch_sub(1, std::memory_order_release); }
    };

    const Flight flight{inFlight_};
    std::shared_ptr<const Snapshot> snapshot;
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        snapshot = listeners_;
    }
    packet.clear();
    bool named = false;
    for (const std::shared_ptr<const Listener>& listener : *snapshot) {
        if (!listener->scope.matches(packet.address)) {
            continue;
        }
        const bool frozen = listener->observer || !packet.editable;
        if (packet.cancelled && !frozen) {
            continue;
        }
        const bool wasCancelled = packet.cancelled;
        std::optional<std::vector<std::uint8_t>> kept;
        if (frozen) {
            kept = packet.replacement;
        }
        if (listener->native) {
            listener->native(packet);
        } else if (listener->owner != nullptr && deliver_) {
            if (!named) {
                packet.name = namer_ ? namer_(packet.address) : std::string{};
                named = true;
            }
            deliver_(listener->owner, listener->fn, packet);
        }
        if (frozen) {
            packet.cancelled = wasCancelled;
            packet.replacement = std::move(kept);
        }
    }
    return !packet.cancelled;
}

void PacketRegistry::connectionClosed(std::uint64_t connection) {
    std::vector<std::function<void(std::uint64_t)>> targets;
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        for (const auto& [id, closer] : closers_) {
            targets.push_back(closer);
        }
    }
    for (const std::function<void(std::uint64_t)>& closer : targets) {
        closer(connection);
    }
}

PluginHandle PacketRegistry::addCloseListener(std::function<void(std::uint64_t connection)> listener) {
    const std::lock_guard<std::mutex> guard{mutex_};
    const std::int64_t id = nextId_++;
    closers_.emplace_back(id, std::move(listener));
    return PluginHandle{[this, id] {
        const std::lock_guard<std::mutex> inner{mutex_};
        std::erase_if(closers_, [id](const auto& entry) { return entry.first == id; });
    }};
}
} // namespace basalt::wire::plugins
