#include "basalt_abi.h"

static const char kHello[] = "hello";
static const char kKey[] = "answer";
static char gBuffer[16];
static int32_t gInitialized;

BASALT_EXPORT(_initialize) void basalt_reactor_initialize(void) { gInitialized = 1; }

BASALT_EXPORT(basalt_abi_version) int32_t basalt_abi_version(void) { return BASALT_ABI_VERSION; }

BASALT_EXPORT(basalt_init) void basalt_init(void) {
    basalt_log(BASALT_LOG_INFO, kHello, 5);
    basalt_event_on("plugin.enable", 13, BASALT_PRIORITY_NORMAL, gInitialized);
}

BASALT_EXPORT(basalt_callback) void basalt_callback(int32_t token, int32_t ctx) {
    if (token == 1) {
        int64_t value = basalt_config_i64(kKey, 6, -1);
        int32_t length = basalt_config_str(kKey, 6, gBuffer, 16);
        basalt_log((int32_t)(value + ctx), gBuffer, length);
    } else if (token == 2) {
        for (;;) {
        }
    } else if (token == 3) {
        __builtin_trap();
    }
}
