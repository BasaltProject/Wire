#include <basalt/wire/common/timings/Timings.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::wire::common {

bool Timing::isEnabled() const noexcept { return owner_->enabled(); }

void Timing::record(std::int64_t nanos) noexcept {
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
}

std::int64_t Timings::windowNanos(std::int64_t nowNanos) const noexcept {
    return std::max<std::int64_t>(nowNanos - startedAtNanos_.load(std::memory_order_relaxed), 0);
}

} // namespace basalt::wire::common
