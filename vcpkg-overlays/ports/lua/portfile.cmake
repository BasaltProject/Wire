vcpkg_download_distfile(ARCHIVE
    URLS "https://www.lua.org/ftp/lua-${VERSION}.tar.gz"
         # lua.org rate-limits CI runners; Fedora's lookaside holds the identical tarball.
         # The hash in the URL is part of Fedora's layout, so it is pinned to this exact release.
         "https://src.fedoraproject.org/repo/pkgs/lua/lua-${VERSION}.tar.gz/sha512/1c8cb3d7194732d7685f80a30e0804cb24dc6aee1f8d1383bb9fd5d0699b84c70b823e686555dfa8fd61a32f32c0c687a8419ff4aa285f39d4f3bf3d5cd635c0/lua-${VERSION}.tar.gz"
    FILENAME "lua-${VERSION}.tar.gz"
    SHA512 1c8cb3d7194732d7685f80a30e0804cb24dc6aee1f8d1383bb9fd5d0699b84c70b823e686555dfa8fd61a32f32c0c687a8419ff4aa285f39d4f3bf3d5cd635c0
)
vcpkg_extract_source_archive(
    SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES
        vs2015-impl-c99.patch
        uwp-no-popen.diff
)
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        cpp     COMPILE_AS_CPP
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DVERSION=${VERSION}
)
vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME unofficial-lua CONFIG_PATH share/unofficial-lua)

if(VCPKG_TARGET_IS_WINDOWS AND VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/luaconf.h" "defined(LUA_BUILD_AS_DLL)" "1")
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

file(READ "${SOURCE_PATH}/doc/readme.html" readme)
string(REGEX REPLACE "^.*<H2><A NAME=\"license\">License</A></H2>" "" license "${readme}")
string(REGEX REPLACE [[<A HREF="([^"]+)">([^<]*)</A>]] "\\2 [\\1]" license "${license}")
string(REGEX REPLACE "<[^>]*>" "" license "${license}")
string(REGEX REPLACE "\n\n+" "\n\n" license "${license}")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "${license}")
