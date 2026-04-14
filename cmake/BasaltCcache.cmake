# Optional compiler caching shared by consumers. An explicit launcher always wins.
option(BASALT_USE_CCACHE "Use ccache when available" ON)
if(BASALT_USE_CCACHE)
    find_program(BASALT_CCACHE_PROGRAM NAMES ccache)
    if(BASALT_CCACHE_PROGRAM)
        foreach(_basalt_language C CXX)
            if(NOT DEFINED CMAKE_${_basalt_language}_COMPILER_LAUNCHER)
                set(CMAKE_${_basalt_language}_COMPILER_LAUNCHER
                    "${BASALT_CCACHE_PROGRAM}" CACHE STRING "Compiler launcher")
            endif()
        endforeach()
    endif()
endif()
