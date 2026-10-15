#pragma once

#include <cstddef>
#include <memory>
#include <optional>

namespace basalt::wire::io {

/// Shared across owners; units are chosen by the consumer (bytes, sessions or pending jobs).
class ResourceBudget {
    struct State;

public:
    /// Each reservation has one owner; independent reservations may run concurrently.
    class Reservation {
    public:
        Reservation(Reservation&& other) noexcept;
        Reservation& operator=(Reservation&& other) noexcept;
        ~Reservation();
        Reservation(const Reservation&) = delete;
        Reservation& operator=(const Reservation&) = delete;

        [[nodiscard]] std::size_t size() const noexcept;
        /// Growth fails atomically; shrinking immediately releases capacity.
        [[nodiscard]] bool resize(std::size_t units) noexcept;
        void reset() noexcept;

    private:
        friend class ResourceBudget;
        Reservation(std::shared_ptr<State> state, std::size_t units) noexcept;
        std::shared_ptr<State> state_;
        std::size_t units_ = 0;
    };

    explicit ResourceBudget(std::size_t limit);
    /// Copies refer to the same budget. Reservations may outlive this handle.
    [[nodiscard]] std::optional<Reservation> acquire(std::size_t units) const noexcept;
    [[nodiscard]] std::size_t used() const noexcept;
    [[nodiscard]] std::size_t limit() const noexcept;

private:
    std::shared_ptr<State> state_;
};

} // namespace basalt::wire::io
