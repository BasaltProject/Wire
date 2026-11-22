#pragma once

#include <basalt/wire/wasm/Abi.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace basalt::wire::wasm {

/// A bounds-checked window over the guest linear memory, valid until the guest runs again.
class GuestMemory {
public:
    GuestMemory() = default;

    GuestMemory(std::uint8_t* base, std::size_t size) : base_(base), size_(size) {}

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    [[nodiscard]] std::optional<std::span<std::uint8_t>> range(std::uint32_t ptr, std::uint32_t len) const noexcept {
        if (static_cast<std::uint64_t>(ptr) + len > size_) {
            return std::nullopt;
        }
        return std::span<std::uint8_t>{base_ + ptr, len};
    }

    [[nodiscard]] std::optional<std::string_view> string(std::int32_t ptr, std::int32_t len) const noexcept {
        if (len < 0) {
            return std::nullopt;
        }
        const auto view = range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(len));
        if (!view) {
            return std::nullopt;
        }
        return std::string_view{reinterpret_cast<const char*>(view->data()), view->size()};
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] std::optional<T> read(std::int32_t ptr) const noexcept {
        const auto view = range(static_cast<std::uint32_t>(ptr), sizeof(T));
        if (!view) {
            return std::nullopt;
        }
        T value;
        std::memcpy(&value, view->data(), sizeof(T));
        return value;
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] bool write(std::int32_t ptr, const T& value) const noexcept {
        const auto view = range(static_cast<std::uint32_t>(ptr), sizeof(T));
        if (!view) {
            return false;
        }
        std::memcpy(view->data(), &value, sizeof(T));
        return true;
    }

    /// The buffer-returning convention of the ABI: the length the full answer needs, written only when it fits.
    [[nodiscard]] std::int32_t put(std::int32_t ptr, std::int32_t cap, std::string_view data) const noexcept {
        if (cap < 0 || data.size() > static_cast<std::size_t>(INT32_MAX)) {
            return kErrInvalid;
        }
        const auto needed = static_cast<std::int32_t>(data.size());
        if (needed > cap) {
            return needed;
        }
        if (needed == 0) {
            return 0;
        }
        const auto view = range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(needed));
        if (!view) {
            return kErrInvalid;
        }
        std::memcpy(view->data(), data.data(), data.size());
        return needed;
    }

private:
    std::uint8_t* base_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace basalt::wire::wasm
