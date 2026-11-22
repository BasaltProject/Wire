#pragma once

#include "Text.hpp"
#include "World.hpp"

#include <basalt/wire/wasm/Abi.hpp>
#include <basalt/wire/wasm/Memory.hpp>
#include <basalt/wire/wasm/Value.hpp>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::wasm::trace {

/// The text every language's trace shows for a value: null, true, 3, 1.5, "s", b"0aff", [1, 2], {"k": 1}.
[[nodiscard]] inline std::string canonical(const Value& value) {
    if (value.isNull()) {
        return "null";
    }
    if (const bool* flag = value.asBool()) {
        return *flag ? "true" : "false";
    }
    if (const std::int64_t* whole = value.asInt()) {
        return std::to_string(*whole);
    }
    if (const double* real = value.asFloat()) {
        return number(*real);
    }
    if (const std::string* text = value.asString()) {
        return quote(*text);
    }
    if (const Blob* blob = value.asBytes()) {
        constexpr std::string_view kDigits = "0123456789abcdef";
        std::string text = "b\"";
        for (const std::uint8_t byte : blob->data) {
            text += kDigits[byte >> 4U];
            text += kDigits[byte & 0xFU];
        }
        return text + '"';
    }
    std::string text;
    if (const Value::List* list = value.asList()) {
        text = "[";
        for (const Value& item : *list) {
            text += (text.size() > 1 ? ", " : "") + canonical(item);
        }
        return text + "]";
    }
    text = "{";
    for (const MapEntry& entry : *value.asMap()) {
        text += (text.size() > 1 ? ", " : "") + quote(entry.key) + ": " + canonical(entry.value);
    }
    return text + "}";
}

[[nodiscard]] inline std::expected<Value, std::int32_t> readValue(const GuestMemory& memory, std::int32_t ptr,
                                                                  std::int32_t len) {
    if (len < 0 || static_cast<std::size_t>(len) > kValueMaxBytes) {
        return std::unexpected(kErrInvalid);
    }
    const auto bytes = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(len));
    if (!bytes) {
        return std::unexpected(kErrInvalid);
    }
    std::expected<Value, std::string> value = decodeValue(*bytes);
    if (!value) {
        return std::unexpected(kErrInvalid);
    }
    return std::move(*value);
}

[[nodiscard]] inline std::int32_t putValue(const GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                                           const Value& value) {
    std::string encoded;
    if (!encodeValue(value, encoded)) {
        return kErrInvalid;
    }
    return memory.put(ptr, cap, encoded);
}

struct Request {
    std::int32_t token = 0;
    bool pending = true;
};

struct Requests {
    std::vector<Request> list;
};

[[nodiscard]] inline Requests& requests(World& world) { return world.areaState<Requests>("core_values"); }

/// The request handle (1, 2, ...) an area returns for a call that completes later.
[[nodiscard]] inline std::int32_t beginRequest(World& world, std::int32_t token) {
    Requests& all = requests(world);
    all.list.push_back({token, true});
    return static_cast<std::int32_t>(all.list.size());
}

inline bool requestPending(World& world, std::int32_t request) {
    Requests& all = requests(world);
    return request >= 1 && static_cast<std::size_t>(request) <= all.list.size() &&
           all.list[static_cast<std::size_t>(request - 1)].pending;
}

inline bool cancelRequest(World& world, std::int32_t request) {
    if (!requestPending(world, request)) {
        return false;
    }
    requests(world).list[static_cast<std::size_t>(request - 1)].pending = false;
    return true;
}

/// Calls `token` with a completion context that answers `value`.
inline std::int32_t deliverValue(World& world, std::int32_t token, Value value) {
    Context context;
    context.kind = ContextKind::Completion;
    context.value = std::move(value);
    return world.invoke(token, std::move(context));
}

/// Completes a pending request; false when it was cancelled or already completed.
inline bool completeRequest(World& world, std::int32_t request, Value value) {
    if (!requestPending(world, request)) {
        return false;
    }
    Request& entry = requests(world).list[static_cast<std::size_t>(request - 1)];
    entry.pending = false;
    return deliverValue(world, entry.token, std::move(value)) == kOk;
}

/// Answers a pending request at the end of the next tick instead of inside the call that made it.
inline void answerLater(World& world, std::int32_t request, Value value) {
    if (requestPending(world, request)) {
        world.tickEnd.push_back(
            [&world, request, value = std::move(value)] { static_cast<void>(completeRequest(world, request, value)); });
    }
}

} // namespace basalt::wire::wasm::trace
