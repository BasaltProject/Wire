#include <basalt/wire/io/ResourceBudget.hpp>

#include <atomic>
#include <utility>

namespace basalt::wire::io {

struct ResourceBudget::State {
    explicit State(std::size_t maximum) : limit(maximum) {}

    const std::size_t limit;
    std::atomic<std::size_t> used{0};

    bool acquire(std::size_t units) noexcept {
        auto current = used.load(std::memory_order_relaxed);
        do {
            if (units > limit - current)
                return false;
        } while (!used.compare_exchange_weak(current, current + units, std::memory_order_relaxed));
        return true;
    }
};

ResourceBudget::ResourceBudget(std::size_t limit) : state_(std::make_shared<State>(limit)) {}

std::optional<ResourceBudget::Reservation> ResourceBudget::acquire(std::size_t units) const noexcept {
    if (!state_ || !state_->acquire(units))
        return std::nullopt;
    return Reservation{state_, units};
}

std::size_t ResourceBudget::used() const noexcept { return state_ ? state_->used.load(std::memory_order_relaxed) : 0; }

std::size_t ResourceBudget::limit() const noexcept { return state_ ? state_->limit : 0; }

ResourceBudget::Reservation::Reservation(std::shared_ptr<State> state, std::size_t units) noexcept
    : state_(std::move(state)), units_(units) {}

ResourceBudget::Reservation::Reservation(Reservation&& other) noexcept
    : state_(std::move(other.state_)), units_(std::exchange(other.units_, 0)) {}

ResourceBudget::Reservation& ResourceBudget::Reservation::operator=(Reservation&& other) noexcept {
    if (this != &other) {
        reset();
        state_ = std::move(other.state_);
        units_ = std::exchange(other.units_, 0);
    }
    return *this;
}

ResourceBudget::Reservation::~Reservation() { reset(); }

std::size_t ResourceBudget::Reservation::size() const noexcept { return units_; }

bool ResourceBudget::Reservation::resize(std::size_t units) noexcept {
    if (!state_)
        return false;
    if (units > units_) {
        if (!state_->acquire(units - units_))
            return false;
    } else {
        state_->used.fetch_sub(units_ - units, std::memory_order_relaxed);
    }
    units_ = units;
    return true;
}

void ResourceBudget::Reservation::reset() noexcept {
    if (state_) {
        state_->used.fetch_sub(units_, std::memory_order_relaxed);
        units_ = 0;
        state_.reset();
    }
}

} // namespace basalt::wire::io
