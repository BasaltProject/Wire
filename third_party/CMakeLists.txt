# Vendored dependencies, compiled from source. Nothing here is looked up on the host system:
# the cross-compilation sysroots carry none of it.

add_subdirectory(doctest)
add_subdirectory(mbedtls)
add_subdirectory(simdjson)
add_subdirectory(zlib)
add_subdirectory(zstd)
