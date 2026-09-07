# basalt_module() and basalt_test(): the only two ways a Basalt module enters the build.
#
# A module is one static library plus one public include root. The dependency rule of the
# project ("the arrows always point down") stops being a convention here: a module that does
# not list another in DEPENDS cannot see its headers.

include(GNUInstallDirs)

function(basalt_module NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS;PRIVATE_DEPENDS" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "basalt_module(${NAME}): SOURCES is required")
    endif()

    add_library(${NAME} STATIC ${ARG_SOURCES})
    add_library(basalt::${NAME} ALIAS ${NAME})

    target_include_directories(${NAME}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(${NAME}
        PUBLIC ${ARG_DEPENDS}
        PRIVATE ${ARG_PRIVATE_DEPENDS} basalt::warnings basalt::sanitizers basalt::parity basalt::platform
    )
    set_target_properties(${NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )
endfunction()

function(basalt_test NAME)
    if(NOT BASALT_BUILD_TESTS)
        return()
    endif()

    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    add_executable(${NAME} ${ARG_SOURCES})
    target_link_libraries(${NAME} PRIVATE ${ARG_DEPENDS} basalt::doctest basalt::warnings basalt::sanitizers
                                    basalt::parity basalt::platform basalt::test_warnings)
    set_target_properties(${NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

function(basalt_bench NAME)
    if(NOT BASALT_BUILD_BENCH)
        return()
    endif()

    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    add_executable(${NAME} ${ARG_SOURCES})
    target_link_libraries(${NAME} PRIVATE ${ARG_DEPENDS} basalt::warnings basalt::sanitizers basalt::parity
                                    basalt::platform)
    set_target_properties(${NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bench"
    )
endfunction()
