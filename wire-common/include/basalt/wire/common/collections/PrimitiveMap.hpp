#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::wire::common {

/// splitmix64's finalizer. Not vanilla-related: it only spreads keys evenly across buckets, which
/// packed chunk coordinates would otherwise refuse to do.
[[nodiscard]] constexpr std::uint64_t mixKey(std::uint64_t x) noexcept {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

/// Open-addressing map keyed by a primitive integer: no boxing, no per-entry node, one probe
/// sequence over two flat arrays. Replaces the hand-written Long2ObjectMap and Int2IntMap, which
/// were the same code twice.
///
/// The two lowest key values are reserved as slot sentinels and must not be stored.
template <typename Key, typename Value>
class PrimitiveMap {
public:
    static constexpr Key kEmpty = std::numeric_limits<Key>::min();
    static constexpr Key kTombstone = std::numeric_limits<Key>::min() + 1;

    explicit PrimitiveMap(std::size_t initialCapacity = 16) { reset(nextPowerOfTwo(std::max<std::size_t>(initialCapacity, 4))); }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] bool contains(Key key) const noexcept { return findSlot(key) >= 0; }

    [[nodiscard]] const Value* find(Key key) const noexcept {
        const std::ptrdiff_t slot = findSlot(key);
        return slot >= 0 ? &values_[static_cast<std::size_t>(slot)] : nullptr;
    }

    [[nodiscard]] Value* find(Key key) noexcept {
        const std::ptrdiff_t slot = findSlot(key);
        return slot >= 0 ? &values_[static_cast<std::size_t>(slot)] : nullptr;
    }

    [[nodiscard]] Value getOrDefault(Key key, Value fallback) const noexcept {
        const Value* found = find(key);
        return found != nullptr ? *found : fallback;
    }

    /// Returns the previous value, if the key was present.
    std::optional<Value> put(Key key, Value value) {
        assert(key != kEmpty && key != kTombstone && "key is reserved and cannot be stored");
        if (occupied_ * 4 >= keys_.size() * 3) {
            grow();
        }

        std::size_t index = indexFor(key);
        std::ptrdiff_t firstTombstone = -1;
        while (true) {
            const Key existing = keys_[index];
            if (existing == kEmpty) {
                const std::size_t target = firstTombstone >= 0 ? static_cast<std::size_t>(firstTombstone) : index;
                if (target == index) {
                    ++occupied_;
                }
                keys_[target] = key;
                values_[target] = std::move(value);
                ++size_;
                return std::nullopt;
            }
            if (existing == kTombstone) {
                if (firstTombstone < 0) {
                    firstTombstone = static_cast<std::ptrdiff_t>(index);
                }
            } else if (existing == key) {
                return std::exchange(values_[index], std::move(value));
            }
            index = (index + 1) & (keys_.size() - 1);
        }
    }

    /// Returns the removed value, if the key was present.
    std::optional<Value> remove(Key key) {
        const std::ptrdiff_t slot = findSlot(key);
        if (slot < 0) {
            return std::nullopt;
        }
        const auto at = static_cast<std::size_t>(slot);
        Value previous = std::move(values_[at]);
        keys_[at] = kTombstone;
        values_[at] = Value{};
        --size_;
        return previous;
    }

    void clear() { reset(keys_.size()); }

    /// Calls `visit(key, value)` for every live entry, in slot order.
    template <typename Visitor>
    void forEach(Visitor&& visit) const {
        for (std::size_t i = 0; i < keys_.size(); ++i) {
            if (keys_[i] != kEmpty && keys_[i] != kTombstone) {
                visit(keys_[i], values_[i]);
            }
        }
    }

private:
    [[nodiscard]] std::size_t indexFor(Key key) const noexcept {
        return mixKey(static_cast<std::uint64_t>(key)) & (keys_.size() - 1);
    }

    [[nodiscard]] std::ptrdiff_t findSlot(Key key) const noexcept {
        if (key == kEmpty || key == kTombstone) {
            return -1;
        }
        std::size_t index = indexFor(key);
        while (true) {
            const Key existing = keys_[index];
            if (existing == kEmpty) {
                return -1;
            }
            if (existing == key) {
                return static_cast<std::ptrdiff_t>(index);
            }
            index = (index + 1) & (keys_.size() - 1);
        }
    }

    void reset(std::size_t capacity) {
        keys_.assign(capacity, kEmpty);
        values_.assign(capacity, Value{});
        occupied_ = 0;
        size_ = 0;
    }

    void grow() {
        std::vector<Key> oldKeys = std::move(keys_);
        std::vector<Value> oldValues = std::move(values_);
        reset(oldKeys.size() * 2);
        for (std::size_t i = 0; i < oldKeys.size(); ++i) {
            if (oldKeys[i] != kEmpty && oldKeys[i] != kTombstone) {
                put(oldKeys[i], std::move(oldValues[i]));
            }
        }
    }

    [[nodiscard]] static std::size_t nextPowerOfTwo(std::size_t x) noexcept { return std::bit_ceil(x); }

    std::vector<Key> keys_;
    std::vector<Value> values_;
    /// Live entries plus tombstones from remove(); drives when to grow.
    std::size_t occupied_ = 0;
    std::size_t size_ = 0;
};

template <typename Value>
using Long2ObjectMap = PrimitiveMap<std::int64_t, Value>;

using Int2IntMap = PrimitiveMap<std::int32_t, std::int32_t>;

} // namespace basalt::wire::common
