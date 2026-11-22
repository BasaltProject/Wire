#ifndef BASALT_AREA_CORE_VALUES_H
#define BASALT_AREA_CORE_VALUES_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Completions. A call that takes a token and returns a request handle finishes later with
 * basalt_callback(token, ctx); during that callback ctx answers value_result. A request handle is a positive i32 that
 * is STALE once it completed or was cancelled. Callbacks run on the owner of what they touch and must not block. */

/* value_result: writes the completion's value (the encoding of basalt_values.h) into `out` and returns the length the
 * full answer needs, written only when it fits in `cap`. INVALID for a ctx that is not a completion, STALE once the
 * callback returned. */
BASALT_IMPORT_SHARED(value_result) int32_t basalt_value_result(int32_t ctx, char *out, int32_t cap);

/* request_cancel: drops a pending request so its callback never runs. OK, or ABSENT when the handle is unknown,
 * completed or already cancelled. */
BASALT_IMPORT_SHARED(request_cancel) int32_t basalt_request_cancel(int32_t request);

#ifdef __cplusplus
}
#endif
#endif
