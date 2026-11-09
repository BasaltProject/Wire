cmake_minimum_required(VERSION 3.28)

option(COMPILE_AS_CPP "Enable the lua C++ library")

project(lua LANGUAGES C)

set(CMAKE_C_STANDARD 99)

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_compile_definitions(LUA_USE_IOS)
else()
    add_compile_definitions(LUA_USE_POSIX)
    find_library(HAVE_LIBM NAMES m)
endif()

set(SRC_LUAI "${PROJECT_SOURCE_DIR}/src/lua.c")
set(SRC_LUAC "${PROJECT_SOURCE_DIR}/src/luac.c")
file(GLOB SRC_LIBLUA "${PROJECT_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM SRC_LIBLUA "${SRC_LUAI}" "${SRC_LUAC}")

add_library(lua ${SRC_LIBLUA})
target_include_directories(lua PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
    $<INSTALL_INTERFACE:include>
)
set_target_properties(lua PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(lua PUBLIC LUA_BUILD_AS_DLL)
endif()

if(NOT WIN32 AND HAVE_LIBM)
    target_link_libraries(lua PRIVATE m)
endif()

install(FILES src/lualib.h src/lua.h src/luaconf.h src/lauxlib.h src/lua.hpp
    DESTINATION include
)
install(TARGETS lua
    EXPORT unofficial-lua-config
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/unofficial-lua-config-version.cmake"
    VERSION "${VERSION}"
    COMPATIBILITY "SameMajorVersion"
)
install(FILES "${PROJECT_BINARY_DIR}/unofficial-lua-config-version.cmake"
    DESTINATION share/unofficial-lua
)
install(EXPORT unofficial-lua-config
    NAMESPACE unofficial::lua::
    DESTINATION share/unofficial-lua
)
