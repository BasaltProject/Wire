#include "HostApi.hpp"

namespace basalt::wire::wasm::trace {
namespace {
void valueResult(void* u, Call& c) {
    World& world = worldOf(u);
    Context* context = nullptr;
    const std::int32_t ctx = c.i32(0);
    Rec rec("value_result");
    rec.i(ctx).cap(c.i32(2));
    const std::int32_t status = world.checkContext(ctx, ContextKind::Completion, context);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const std::int32_t result = putValue(c.memory(), c.i32(1), c.i32(2), *context->value);
    c.ret(result >= 0 && result <= c.i32(2) ? rec.done(result, canonical(*context->value)) : rec.done(result));
}

void requestCancel(void* u, Call& c) {
    const std::int32_t request = c.i32(0);
    c.ret(Rec("request_cancel").i(request).done(cancelRequest(worldOf(u), request) ? kOk : kErrAbsent));
}
} // namespace

void addCoreValuesImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"value_result", valueResult, user},
                              {"request_cancel", requestCancel, user},
                          });
}

} // namespace basalt::wire::wasm::trace
