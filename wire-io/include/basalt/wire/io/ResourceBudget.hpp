#pragma once

#include <cstddef>
#include <memory>
#include <optional>

namespace basalt::wire::io {

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
        [[nodiscard]] bool resize(std::size_t units) noexcept;
        void reset() noexcept;

    private:
        friend class ResourceBudget;
        Reservation(std::shared_ptr<State> state, std::size_t units) noexcept;
        std::shared_ptr<State> state_;
        std::size_t units_ = 0;
    };

    explicit ResourceBudget(std::size_t limit);
    [[nodiscard]] std::optional<Reservation> acquire(std::size_t units) const noexcept;
    [[nodiscard]] std::size_t used() const noexcept;
    [[nodiscard]] std::size_t limit() const noexcept;

private:
    std::shared_ptr<State> state_;
};

} // namespace basalt::wire::io
