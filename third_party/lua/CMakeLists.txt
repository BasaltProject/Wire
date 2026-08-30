# Lua 5.5.1, MIT, the official source as published by lua.org (SHA-256
# 1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce, the digest lua.org itself
# publishes beside the download).
#
# ADR-008 replaced the project's own Kotlin interpreter with this: it is C, it links, and there is no
# second implementation of the language to keep correct. The sandbox the plugin API needs (an
# instruction budget, a memory ceiling) is `lua_sethook` and a custom allocator, which is how it is
# done in C.
#
# The two `main` files are left out: `lua.c` is the standalone interpreter and `luac.c` the compiler,
# and a server that embeds the library wants neither.

file(GLOB BASALT_LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_library(basalt_lua STATIC ${BASALT_LUA_SOURCES})
add_library(basalt::lua ALIAS basalt_lua)

target_include_directories(basalt_lua SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# LUA_USE_POSIX turns on the POSIX pieces of the standard library that build everywhere but Windows,
# where the default (ANSI C only) is what compiles.
if(NOT WIN32)
    target_compile_definitions(basalt_lua PRIVATE LUA_USE_POSIX)
endif()
set_target_properties(basalt_lua PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
target_compile_options(basalt_lua PRIVATE -w)
