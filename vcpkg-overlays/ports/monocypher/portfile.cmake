vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO LoupVaillant/Monocypher
    REF "${VERSION}"
    SHA512 fdbf680d0a239410b1ac27b55dc50dc7076698766684d8deec25292cb289fe834445be8607f9aafdbeb75a4b365af39578129aeb9ecdbea4e9dc7fd0402318ac
    HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME monocypher CONFIG_PATH share/monocypher)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENCE.md")
