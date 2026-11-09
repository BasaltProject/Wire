cmake_minimum_required(VERSION 3.28)
project(monocypher LANGUAGES C)

add_library(monocypher
    src/monocypher.c
    src/optional/monocypher-ed25519.c
)
add_library(monocypher::monocypher ALIAS monocypher)

target_include_directories(monocypher PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/optional>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)

install(FILES
    src/monocypher.h
    src/optional/monocypher-ed25519.h
    DESTINATION include
)
install(TARGETS monocypher
    EXPORT monocypher-config
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(EXPORT monocypher-config
    NAMESPACE monocypher::
    DESTINATION share/monocypher
)
