#include <basalt/wire/plugins/PluginValue.hpp>

#include <array>
#include <charconv>
#include <cstdlib>
#include <string_view>
#include <system_error>

namespace basalt::wire::plugins {

std::optional<std::string> encodeValue(lua::LuaArgs& lua, std::int32_t index) {
    if (lua.isBoolean(index))
        return lua.toBoolean(index) ? "b1" : "b0";
    if (lua.isInteger(index))
        return "n" + std::to_string(lua.toInteger(index));
    if (lua.isNumber(index)) {
        std::array<char, 40> buffer{};
        const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), lua.toNumber(index));
        return "n" + std::string{buffer.data(), end};
    }
    if (lua.isString(index))
        return "s" + lua.toText(index);
    return std::nullopt;
}

void pushValue(lua::LuaArgs& lua, const std::string& stored) {
    if (stored.empty()) {
        lua.pushNil();
    } else if (stored[0] == 's') {
        lua.pushString(std::string_view{stored}.substr(1));
    } else if (stored[0] == 'b') {
        lua.pushBoolean(stored.size() > 1 && stored[1] == '1');
    } else {
        const std::string_view number = std::string_view{stored}.substr(1);
        std::int64_t whole = 0;
        const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), whole);
        if (error == std::errc{} && end == number.data() + number.size()) {
            lua.pushInteger(whole);
        } else {
            lua.pushNumber(std::strtod(std::string{number}.c_str(), nullptr));
        }
    }
}

} // namespace basalt::wire::plugins
