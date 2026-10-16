# Monocypher 4.0.2, BSD-2-Clause or CC0-1.0. Ed25519 (RFC 8032) for the native forwarding signature,
# which mbedTLS does not provide. Only the core and the SHA-512 Ed25519 variant are built.

add_library(basalt_monocypher STATIC monocypher.c monocypher-ed25519.c)
add_library(basalt::monocypher ALIAS basalt_monocypher)

target_include_directories(basalt_monocypher SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(basalt_monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(basalt_monocypher PRIVATE -w)
