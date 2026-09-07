# Warning set shared by every Basalt target. Kept in Wire because Server and Proxy both consume it.

include(CheckCXXCompilerFlag)

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

# Clang 19 added this one. It fires on aggregate initialisation that leaves a field out even when
# the field has a default, so writing every field back in would only duplicate the defaults. Probe
# the positive form: an unknown -Wno- flag is accepted silently and would tell us nothing.
check_cxx_compiler_flag(-Wmissing-designated-field-initializers BASALT_HAS_MISSING_DESIGNATED_FIELD)
if(BASALT_HAS_MISSING_DESIGNATED_FIELD)
    target_compile_options(basalt_warnings INTERFACE -Wno-missing-designated-field-initializers)
endif()

if(BASALT_WERROR)
    target_compile_options(basalt_warnings INTERFACE -Werror)
endif()

# Warnings that only a test can trip, kept apart so the real code keeps the full set. Linked after
# basalt::warnings so these land later on the command line and win.
add_library(basalt_test_warnings INTERFACE)
add_library(basalt::test_warnings ALIAS basalt_test_warnings)

# doctest::Approx takes a double, so every Approx(1.0F) in a test promotes a float. That is the
# shape of doctest's API, not ours, and -Wdouble-promotion still guards the code that generates.
target_compile_options(basalt_test_warnings INTERFACE -Wno-double-promotion)
