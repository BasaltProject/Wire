# Floating point settings the whole project depends on for vanilla parity.
#
# Java never fuses a multiply and an add; Clang does by default (-ffp-contract=on). The difference
# is real and it is not academic: `x * x + y * y` inside the gaussian sampler comes out one ulp off,
# and the very first gaussian of seed 0 stops matching vanilla. Every target links this, not just
# the ones that look numeric.

add_library(basalt_parity INTERFACE)
add_library(basalt::parity ALIAS basalt_parity)

target_compile_options(basalt_parity INTERFACE
    -ffp-contract=off
    -fno-fast-math
    -fno-associative-math
)
