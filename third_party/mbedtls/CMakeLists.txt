# Mbed TLS 3.6.2, Apache-2.0. Chosen over OpenSSL for what Basalt actually needs from a crypto
# library: RSA-1024 with PKCS#1 v1.5 for the login key exchange, AES-128-CFB8 for the session
# cipher, and SHA-1 for the asset checksums. That is a small surface, and Mbed TLS is 8 MB of
# dependency-free C that builds on all five targets, against roughly 60 MB of OpenSSL with a build
# system of its own.
#
# The AES-CFB8 that had to be written by hand under Kotlin because the wrapper would not expose it
# is a stock cipher mode here.

file(GLOB BASALT_MBEDTLS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/library/*.c)

add_library(basalt_mbedtls STATIC ${BASALT_MBEDTLS_SOURCES})
add_library(basalt::mbedtls ALIAS basalt_mbedtls)

target_include_directories(basalt_mbedtls SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(basalt_mbedtls PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library)
set_target_properties(basalt_mbedtls PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(basalt_mbedtls PRIVATE -w)
