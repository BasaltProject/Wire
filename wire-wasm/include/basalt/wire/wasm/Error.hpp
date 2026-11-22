#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace basalt::wire::wasm {

enum class ErrorCode : std::uint8_t {
    Backend,
    Compile,
    InvalidModule,
    AbiVersion,
    Link,
    Instantiate,
    Trap,
    Interrupted,
    Unsupported,
    Io,
};

struct Failure {
    ErrorCode code = ErrorCode::Backend;
    std::string message;
};

template <typename T>
using Result = std::expected<T, Failure>;

[[nodiscard]] inline std::unexpected<Failure> fail(ErrorCode code, std::string message) {
    return std::unexpected<Failure>(Failure{code, std::move(message)});
}

} // namespace basalt::wire::wasm
