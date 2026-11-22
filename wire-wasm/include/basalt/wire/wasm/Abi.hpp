#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::wire::wasm {

inline constexpr std::int32_t kAbiVersion = 1;

inline constexpr std::int32_t kOk = 0;
inline constexpr std::int32_t kErrAbsent = -1;
inline constexpr std::int32_t kErrStale = -2;
inline constexpr std::int32_t kErrInvalid = -3;
inline constexpr std::int32_t kErrDenied = -4;

inline constexpr std::string_view kBasaltModule = "basalt";
inline constexpr std::string_view kProxyModule = "basalt_proxy";
inline constexpr std::string_view kWasiModule = "wasi_snapshot_preview1";

/// Which imports a module may use: the server profile, or the proxy one (see ABI-proxy.md).
enum class Profile : std::uint8_t { Server, Proxy };

/// Which profiles offer an import: the server one, the proxy one, or both.
enum class AbiScope : std::uint8_t { Server, Shared, Proxy };

/// Signature spelling is "params>results" with i = i32, l = i64, f = f32, d = f64.
struct AbiFunction {
    std::string_view module;
    std::string_view name;
    std::string_view signature;
    AbiScope scope = AbiScope::Server;
};

/// Every import generated from the ABI headers (see cmake/BasaltWasmAbi.cmake), then the wasi subset.
[[nodiscard]] std::span<const AbiFunction> abiFunctions();

[[nodiscard]] std::size_t basaltFunctionCount();

/// Whether a module of the profile may import the function; the wasi subset is in both.
[[nodiscard]] bool inProfile(Profile profile, const AbiFunction& function);

[[nodiscard]] std::optional<std::size_t> findAbiFunction(std::string_view module, std::string_view name);

} // namespace basalt::wire::wasm
