#include "basalt_abi.h"

BASALT_EXPORT(basalt_abi_version) int32_t basalt_abi_version(void) { return BASALT_ABI_VERSION; }
BASALT_EXPORT(basalt_init) void basalt_init(void) {}

BASALT_EXPORT(basalt_callback) void basalt_callback(int32_t token, int32_t ctx) {
    if (token == 1) {
        int32_t sum = ctx;
        for (int i = 0; i < 8; ++i) {
            sum += basalt_event_player(sum);
        }
        basalt_task_cancel(sum);
    } else if (token == 2) {
        int32_t sum = 0;
        for (int32_t i = 0; i < ctx; ++i) {
            sum += basalt_event_player(sum);
        }
        basalt_task_cancel(sum);
    }
}
