#include <basalt/wire/io/BudgetMemoryResource.hpp>

#include <algorithm>
#include <bit>
#include <exception>
#include <limits>
#include <new>
#include <utility>

namespace basalt::wire::io {

BudgetMemoryResource::BudgetMemoryResource(ResourceBudget global, ResourceBudget local)
    : global_(std::move(global)), local_(std::move(local)) {}

void* BudgetMemoryResource::do_allocate(std::size_t bytes, std::size_t alignment) {
    if (!std::has_single_bit(alignment))
        throw std::bad_alloc{};
    const auto size = std::max<std::size_t>(bytes, 1);
    if (alignment - 1 > std::numeric_limits<std::size_t>::max() - size)
        throw std::bad_alloc{};
    const auto allocationBytes = size + alignment - 1;
    const auto slot = std::ranges::find_if(blocks_, [](const Block& block) { return !block.storage; });
    if (slot == blocks_.end())
        throw std::bad_alloc{};
    auto global = global_.acquire(allocationBytes);
    auto local = local_.acquire(allocationBytes);
    if (!global || !local)
        throw std::bad_alloc{};
    auto storage = std::make_unique_for_overwrite<std::byte[]>(allocationBytes);
    void* aligned = storage.get();
    auto space = allocationBytes;
    if (!std::align(alignment, size, aligned, space))
        throw std::bad_alloc{};
    *slot = Block{std::move(global), std::move(local), std::move(storage), aligned};
    return aligned;
}

void BudgetMemoryResource::do_deallocate(void* pointer, std::size_t, std::size_t) noexcept {
    const auto slot =
        std::ranges::find_if(blocks_, [&](const Block& block) { return block.storage && block.aligned == pointer; });
    if (slot == blocks_.end())
        std::terminate();
    slot->storage.reset();
    *slot = Block{};
}

bool BudgetMemoryResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept { return this == &other; }

} // namespace basalt::wire::io
