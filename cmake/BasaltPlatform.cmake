# Link requirements that come from the platform rather than from any Basalt module.

include(CheckCXXSourceCompiles)

add_library(basalt_platform INTERFACE)
add_library(basalt::platform ALIAS basalt_platform)

# An atomic wider than a machine word is not lock free, so the compiler emits a call to the
# generic __atomic_* helpers instead of an instruction. On Linux those live in libatomic and the
# link fails unless it is named; macOS and the BSDs carry them in libc, so probe rather than
# hardcode the platform.
check_cxx_source_compiles("
#include <atomic>
#include <cstdint>
struct Wide {
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;
};
int main() {
    std::atomic<Wide> value{};
    Wide expected{};
    value.compare_exchange_strong(expected, Wide{1, 2, 3});
    return static_cast<int>(value.load().a);
}
" BASALT_WIDE_ATOMICS_IN_LIBC)

if(NOT BASALT_WIDE_ATOMICS_IN_LIBC)
    target_link_libraries(basalt_platform INTERFACE atomic)
endif()

if(WIN32)
    target_compile_definitions(basalt_platform INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
                                                          _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()
