# Link requirements that come from the platform rather than from any Basalt module.

include(CheckCXXSourceCompiles)

add_library(basalt_platform INTERFACE)
add_library(basalt::platform ALIAS basalt_platform)

# The compiler may call the typed operator new of Apple's libc++abi, which aborts when libc++ is linked
# statically and its own initializer has not run. Defining the typed operators here keeps them in the binary.
if(APPLE)
    add_library(basalt_typed_new STATIC ${CMAKE_CURRENT_LIST_DIR}/AppleTypedNew.cpp)
    target_link_libraries(basalt_platform INTERFACE basalt_typed_new)
endif()

# An atomic wider than a machine word is not lock free, so the compiler emits a call to the
# generic __atomic_* helpers instead of an instruction. On Linux those live in libatomic and the
# link fails unless it is named; macOS carries them in libSystem, and elsewhere the probe decides.
if(APPLE)
    set(BASALT_WIDE_ATOMICS_IN_LIBC TRUE)
else()
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
endif()

if(NOT BASALT_WIDE_ATOMICS_IN_LIBC)
    target_link_libraries(basalt_platform INTERFACE atomic)
endif()

if(WIN32)
    target_compile_definitions(basalt_platform INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
                                                          _WINSOCK_DEPRECATED_NO_WARNINGS
                                                          _USE_STD_VECTOR_ALGORITHMS=0)
endif()
