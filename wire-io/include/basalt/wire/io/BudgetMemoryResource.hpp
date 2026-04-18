#pragma once

#include <basalt/wire/io/ResourceBudget.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <optional>

namespace basalt::wire::io {

/// Owner-only allocation resource. Both budgets include old and new storage during reallocation.
class BudgetMemoryResource final : public std::pmr::memory_resource {
public:
    static constexpr std::size_t kMaxAllocations = 16;
    BudgetMemoryResource(ResourceBudget global, ResourceBudget local);
    ~BudgetMemoryResource() override = default;
    BudgetMemoryResource(const BudgetMemoryResource&) = delete;
    BudgetMemoryResource& operator=(const BudgetMemoryResource&) = delete;

private:
    struct Block {
        std::optional<ResourceBudget::Reservation> global;
        std::optional<ResourceBudget::Reservation> local;
        std::unique_ptr<std::byte[]> storage;
        void* aligned = nullptr;
    };

    void* do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) noexcept override;
    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

    ResourceBudget global_;
    ResourceBudget local_;
    std::array<Block, kMaxAllocations> blocks_;
};

} // namespace basalt::wire::io
