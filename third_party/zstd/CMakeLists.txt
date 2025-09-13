# zstd 1.5.6, BSD-3-Clause. The Linear region format wraps a whole region in one zstd stream, once
# per file, instead of Anvil's per-chunk zlib: that is where the format's 50 % on the Overworld and
# 95 % on the End come from, so this is not an optional dependency.
#
# Only the common, compress and decompress trees are vendored: the dictionary builder, the legacy
# format readers and the deprecated buffered API are not used by anything here. The hand-written
# assembly of the Huffman decoder is left out too, so one source set builds on all five targets.

file(GLOB BASALT_ZSTD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/common/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/compress/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/decompress/*.c
)

add_library(basalt_zstd STATIC ${BASALT_ZSTD_SOURCES})
add_library(basalt::zstd ALIAS basalt_zstd)

target_include_directories(basalt_zstd SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(basalt_zstd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_compile_definitions(basalt_zstd PRIVATE ZSTD_DISABLE_ASM=1 ZSTD_LEGACY_SUPPORT=0)
set_target_properties(basalt_zstd PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(basalt_zstd PRIVATE -w)
