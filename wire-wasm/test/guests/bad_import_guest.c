#include "basalt_abi.h"

__attribute__((import_module("basalt"), import_name("not_in_the_abi"))) void not_in_the_abi(void);

BASALT_EXPORT(basalt_abi_version) int32_t basalt_abi_version(void) { return BASALT_ABI_VERSION; }
BASALT_EXPORT(basalt_init) void basalt_init(void) { not_in_the_abi(); }
BASALT_EXPORT(basalt_callback) void basalt_callback(int32_t token, int32_t ctx) {
    (void)token;
    (void)ctx;
}
