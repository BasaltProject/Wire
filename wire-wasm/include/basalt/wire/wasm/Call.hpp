#pragma once

#include <basalt/wire/wasm/Memory.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace basalt::wire::wasm {

class Call;

/// Runs on the fast unchecked path: arguments are read and the result written straight in the call frame.
using HostFn = void (*)(void* user, Call& call);

struct HostImport {
    std::string_view name;
    HostFn fn = nullptr;
    void* user = nullptr;
    std::string_view module = kBasaltModule;
};

/// One guest-to-host call. The frame layout differs per backend, so arguments are read by position and type.
class Call {
public:
    using MemoryProvider = GuestMemory (*)(void* backend);

    Call(std::byte* frame, std::size_t stride, void* backend, MemoryProvider memory)
        : frame_(frame), stride_(stride), backend_(backend), memory_(memory) {}

    [[nodiscard]] std::int32_t i32(std::size_t index) const noexcept { return load<std::int32_t>(index); }

    [[nodiscard]] std::int64_t i64(std::size_t index) const noexcept { return load<std::int64_t>(index); }

    [[nodiscard]] float f32(std::size_t index) const noexcept { return load<float>(index); }

    [[nodiscard]] double f64(std::size_t index) const noexcept { return load<double>(index); }

    void ret(std::int32_t value) noexcept { std::memcpy(frame_, &value, sizeof value); }

    void ret(std::int64_t value) noexcept { std::memcpy(frame_, &value, sizeof value); }

    void ret(float value) noexcept { std::memcpy(frame_, &value, sizeof value); }

    void ret(double value) noexcept { std::memcpy(frame_, &value, sizeof value); }

    [[nodiscard]] GuestMemory memory() const { return memory_(backend_); }

    /// Makes the guest trap when this host function returns; `reason` need only outlive the function.
    void trap(std::string_view reason) noexcept { trap_ = reason.empty() ? std::string_view{"trap"} : reason; }

    [[nodiscard]] std::string_view trapReason() const noexcept { return trap_; }

private:
    template <typename T>
    [[nodiscard]] T load(std::size_t index) const noexcept {
        T value;
        std::memcpy(&value, frame_ + index * stride_, sizeof value);
        return value;
    }

    std::byte* frame_;
    std::size_t stride_;
    void* backend_;
    MemoryProvider memory_;
    std::string_view trap_;
};

} // namespace basalt::wire::wasm
