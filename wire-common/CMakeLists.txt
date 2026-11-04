basalt_module(wire-common
    SOURCES
        src/Base64.cpp
        src/Identifier.cpp
        src/JavaNumbers.cpp
        src/Json.cpp
        src/Logger.cpp
        src/LogOutput.cpp
        src/Md5.cpp
        src/Sha256.cpp
        src/Mth.cpp
        src/NativeSampler.cpp
        src/Panic.cpp
        src/PlaceholderCondition.cpp
        src/Placeholders.cpp
        src/PlaceholderText.cpp
        src/TickDurations.cpp
        src/Timings.cpp
        src/Toml.cpp
        src/Uuid.cpp
    PRIVATE_DEPENDS basalt::simdjson
)

# Android before API 33 and musl do not provide the execinfo backtrace API.
# Probe both declarations and linkage rather than treating every Linux libc as glibc.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
#include <execinfo.h>
int main() {
    void* frames[1];
    int count = backtrace(frames, 1);
    backtrace_symbols_fd(frames, count, 2);
}
" BASALT_HAS_EXECINFO)
if(BASALT_HAS_EXECINFO)
    target_compile_definitions(wire-common PRIVATE BASALT_HAS_EXECINFO=1)
endif()

basalt_test(wire-common-placeholder-test
    SOURCES
        test/PlaceholderTest.cpp
    DEPENDS basalt::wire-common
)

basalt_test(wire-common-test
    SOURCES
        test/CommonTest.cpp
    DEPENDS basalt::wire-common
)
