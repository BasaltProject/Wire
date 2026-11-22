#include <basalt/wire/wasm/Instance.hpp>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using namespace basalt::wire::wasm;

namespace {
std::int64_t gCalls = 0;

void eventPlayer(void*, Call& call) {
    ++gCalls;
    call.ret(call.i32(0) + 1);
}

void taskCancel(void*, Call& call) { call.ret(0); }

double nowNanos() {
    return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 64;
    }
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    auto engine = Engine::create();
    auto module = Module::compile(*engine, bytes);
    if (!module) {
        std::fprintf(stderr, "%s\n", module.error().message.c_str());
        return 1;
    }
    const HostImport imports[] = {{"event_player", eventPlayer, nullptr}, {"task_cancel", taskCancel, nullptr}};
    auto instance = Instance::create(*module, InstanceOptions{imports, {}});
    if (!instance) {
        std::fprintf(stderr, "%s\n", instance.error().message.c_str());
        return 1;
    }
    (void)instance->init();
    for (int i = 0; i < 20000; ++i) {
        (void)instance->callback(0, i);
        (void)instance->callback(1, i);
    }
    constexpr int kEnter = 2'000'000;
    constexpr int kEvents = 500'000;
    constexpr int kCrossings = 10'000'000;
    double start = nowNanos();
    for (int i = 0; i < kEnter; ++i) {
        (void)instance->callback(0, i);
    }
    const double enter = (nowNanos() - start) / kEnter;
    start = nowNanos();
    for (int i = 0; i < kEvents; ++i) {
        (void)instance->callback(1, i);
    }
    const double event = (nowNanos() - start) / kEvents;
    start = nowNanos();
    (void)instance->callback(2, kCrossings);
    const double cross = (nowNanos() - start) / kCrossings;
    std::printf("%s: host->guest %.1f ns, event (8 host calls) %.1f ns, guest->host %.1f ns\n",
                std::string{engine->backend()}.c_str(), enter, event, cross);
    return 0;
}
