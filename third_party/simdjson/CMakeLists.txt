# simdjson 3.10.1, Apache-2.0. The single-header build, compiled once into its own static library so
# its 5 MB header reaches exactly one translation unit: wire-common's Json.cpp, which wraps it.

add_library(basalt_simdjson STATIC simdjson.cpp)
add_library(basalt::simdjson ALIAS basalt_simdjson)

target_include_directories(basalt_simdjson SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(basalt_simdjson PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
# Vendored source is not ours to keep warning-clean.
target_compile_options(basalt_simdjson PRIVATE -w)
