# Warning set shared by every Basalt target. Kept in Wire because Server and Proxy both consume it.

add_library(basalt_warnings INTERFACE)
add_library(basalt::warnings ALIAS basalt_warnings)

target_compile_options(basalt_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wimplicit-fallthrough
)

if(BASALT_WERROR)
    target_compile_options(basalt_warnings INTERFACE -Werror)
endif()
