# Wire: the network layer shared by Basalt Server and Basalt Proxy.
#
# Builds standalone, and is also pulled in with add_subdirectory() by both consumers. Wire owns
# the shared cmake modules and third_party/, so whichever side is on top, these come from here.

cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED PROJECT_NAME)
    project(basalt-wire LANGUAGES C CXX)
    set(BASALT_WIRE_STANDALONE ON)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(BASALT_BUILD_TESTS "Build the test executables" ON)
option(BASALT_BUILD_BENCH "Build the benchmark executables" OFF)
option(BASALT_WERROR "Treat warnings as errors" OFF)
set(BASALT_SANITIZE "" CACHE STRING "Comma-separated sanitizers: address, undefined, thread")

if(NOT TARGET basalt_warnings)
    include(BasaltWarnings)
    include(BasaltParity)
    include(BasaltSanitizers)
    include(BasaltModule)
    add_subdirectory(third_party)

    if(BASALT_BUILD_TESTS)
        enable_testing()
    endif()
endif()

add_subdirectory(wire-common)
add_subdirectory(wire-nbt)
add_subdirectory(wire-text)
add_subdirectory(wire-io)
add_subdirectory(wire-protocol)
add_subdirectory(wire-auth)
add_subdirectory(wire-multiversion)
