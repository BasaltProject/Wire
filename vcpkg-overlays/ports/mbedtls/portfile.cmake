if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Mbed-TLS/mbedtls
    REF "v${VERSION}"
    SHA512 d7a1e0098fed7b000ac2e4de31d43f427e8a046aeace91719f58222e1289470e15af5ed2a5390cf3693cf93a1efd79f34de9a6a960dc63cc0fd135072809e6e4
    HEAD_REF development
)
file(WRITE "${SOURCE_PATH}/framework/CMakeLists.txt" "# empty placeholder")

# Install Basalt's threading adapter (pthread on POSIX, native SRWLOCK on Windows without PThreads4W).
file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/threading_alt.h" "${SOURCE_PATH}/include/threading_alt.h")
file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/basalt_mbedtls_threads.h" "${SOURCE_PATH}/include/basalt_mbedtls_threads.h")
file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/threading.c" "${SOURCE_PATH}/library/basalt_threading.c")

vcpkg_replace_string(
    "${SOURCE_PATH}/library/CMakeLists.txt"
    "    threading.c"
    "    threading.c\n    basalt_threading.c"
)

vcpkg_replace_string(
    "${SOURCE_PATH}/include/mbedtls/mbedtls_config.h"
    "//#define MBEDTLS_THREADING_PTHREAD"
    "#define MBEDTLS_THREADING_C\n#if defined(_WIN32)\n#define MBEDTLS_THREADING_ALT\n#else\n#define MBEDTLS_THREADING_PTHREAD\n#endif"
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" USE_SHARED_MBEDTLS_LIBRARY)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" USE_STATIC_MBEDTLS_LIBRARY)

if(VCPKG_TARGET_IS_WINDOWS)
    set(LINK_WITH_PTHREAD OFF)
else()
    set(LINK_WITH_PTHREAD ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DLINK_WITH_PTHREAD=${LINK_WITH_PTHREAD}
        -DENABLE_TESTING=OFF
        -DENABLE_PROGRAMS=OFF
        -DMBEDTLS_FATAL_WARNINGS=FALSE
        -DUSE_SHARED_MBEDTLS_LIBRARY=${USE_SHARED_MBEDTLS_LIBRARY}
        -DUSE_STATIC_MBEDTLS_LIBRARY=${USE_STATIC_MBEDTLS_LIBRARY}
    OPTIONS_DEBUG
        -DINSTALL_MBEDTLS_HEADERS=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

file(INSTALL
    "${CMAKE_CURRENT_LIST_DIR}/threading_alt.h"
    "${CMAKE_CURRENT_LIST_DIR}/basalt_mbedtls_threads.h"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include"
)

vcpkg_fixup_pkgconfig()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/MbedTLS")

file(COPY "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
