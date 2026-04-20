#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct lua_State;

namespace basalt::wire::lua {
class LuaArgs {
public:
    explicit LuaArgs(lua_State* state) : state_(state) {}

    [[nodiscard]] std::int32_t count() const;

    [[nodiscard]] bool isNil(std::int32_t index) const;
    [[nodiscard]] bool isFunction(std::int32_t index) const;
    [[nodiscard]] bool isTable(std::int32_t index) const;
    [[nodiscard]] bool isString(std::int32_t index) const;
    [[nodiscard]] bool isNumber(std::int32_t index) const;

    [[nodiscard]] std::string checkString(std::int32_t index) const;
    [[nodiscard]] std::int64_t checkInteger(std::int32_t index) const;
    [[nodiscard]] double checkNumber(std::int32_t index) const;

    [[nodiscard]] std::optional<std::string> stringOrNull(std::int32_t index) const;
    [[nodiscard]] std::int64_t integerOr(std::int32_t index, std::int64_t fallback) const;
    [[nodiscard]] double numberOr(std::int32_t index, double fallback) const;
    [[nodiscard]] bool booleanOr(std::int32_t index, bool fallback) const;

    [[nodiscard]] std::optional<std::string> fieldAsText(std::int32_t index, std::string_view key) const;

    void pushNil();
    void pushBoolean(bool value);
    void pushInteger(std::int64_t value);
    void pushNumber(double value);
    void pushString(std::string_view value);
    void pushTable();

    void setField(std::string_view key);

    void setIndex(std::int64_t index);

    void pushIndex(std::int32_t index, std::int64_t position);

    void pushField(std::int32_t index, std::string_view key);

    void pop(std::int32_t count);

    [[nodiscard]] std::int64_t length(std::int32_t index) const;

    [[nodiscard]] bool nextEntry(std::int32_t index);

    [[nodiscard]] bool isBoolean(std::int32_t index) const;
    [[nodiscard]] bool isInteger(std::int32_t index) const;
    [[nodiscard]] bool toBoolean(std::int32_t index) const;
    [[nodiscard]] std::int64_t toInteger(std::int32_t index) const;
    [[nodiscard]] double toNumber(std::int32_t index) const;
    [[nodiscard]] std::string toText(std::int32_t index) const;

    void setMetatable(std::int32_t index);

    void pushMetatable(std::int32_t index);

    void setGlobal(std::string_view name);

    [[nodiscard]] std::int32_t top() const;

    [[noreturn]] void raise(std::string_view message) const;

    [[nodiscard]] lua_State* state() const noexcept { return state_; }

private:
    lua_State* state_;
};
} // namespace basalt::wire::lua
