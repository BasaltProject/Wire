#pragma once

#include "HostValues.hpp"
#include "Text.hpp"
#include "World.hpp"

#include <basalt/wire/wasm/Abi.hpp>
#include <basalt/wire/wasm/Call.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::wasm::trace {

class Rec {
public:
    explicit Rec(std::string_view name) : text_(name) { text_ += '('; }

    Rec& str(std::optional<std::string_view> value) { return raw(value ? quote(*value) : "<unreadable>"); }

    Rec& i(std::int64_t value) { return raw(std::to_string(value)); }

    Rec& f64(double value) { return raw(number(value)); }

    Rec& f32(float value) { return raw(number(value)); }

    Rec& value(const Value& v) { return raw(canonical(v)); }

    Rec& value(const std::expected<Value, std::int32_t>& v) { return raw(v ? canonical(*v) : "<invalid>"); }

    Rec& cap(std::int32_t value) { return raw("cap=" + std::to_string(value)); }

    Rec& args(const std::optional<ArgList>& value) { return raw(describe(value)); }

    Rec& raw(std::string_view piece) {
        if (!text_.ends_with('(')) {
            text_ += ", ";
        }
        text_ += piece;
        return *this;
    }

    void doneVoid() { emit(")"); }

    std::int32_t done(std::int32_t result) {
        emit(") = " + std::to_string(result));
        return result;
    }

    std::int32_t done(std::int32_t result, std::string_view extra) {
        emit(") = " + std::to_string(result) + " " + std::string{extra});
        return result;
    }

    std::int64_t done(std::int64_t result) {
        emit(") = " + std::to_string(result));
        return result;
    }

    double done(double result) {
        emit(") = " + number(result));
        return result;
    }

    std::int32_t doneBuffer(const GuestMemory& memory, std::int32_t ptr, std::int32_t cap, std::string_view data) {
        const std::int32_t result = memory.put(ptr, cap, data);
        if (result >= 0 && static_cast<std::size_t>(result) == data.size() && result <= cap) {
            return done(result, quote(data));
        }
        return done(result);
    }

private:
    void emit(const std::string& tail) { emitLine("host " + text_ + tail); }

    std::string text_;
};

[[nodiscard]] inline World& worldOf(void* user) { return *static_cast<World*>(user); }

[[nodiscard]] inline std::int32_t checkPlayer(World& world, std::int32_t handle, Player*& out) {
    if (handle <= 0) {
        return kErrInvalid;
    }
    Player* player = world.playerByHandle(handle);
    if (player == nullptr || !player->online) {
        return kErrStale;
    }
    out = player;
    return kOk;
}

[[nodiscard]] std::vector<HostImport> buildImports(World& world);

} // namespace basalt::wire::wasm::trace
