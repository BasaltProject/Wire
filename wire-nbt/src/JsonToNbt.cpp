#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <cstdint>

namespace basalt::wire::nbt {
namespace {

using common::JsonObject;
using common::JsonValue;

std::expected<NbtTag, JsonToNbtError> convertObject(const JsonObject& object) {
    NbtCompound compound;
    for (std::size_t i = 0; i < object.size(); ++i) {
        if (object.values()[i].isNull()) {
            continue;
        }
        auto converted = jsonToNbt(object.values()[i]);
        if (!converted) {
            return converted;
        }
        compound.set(object.names()[i], std::move(*converted));
    }
    return NbtTag::ofCompound(std::move(compound));
}

std::expected<NbtTag, JsonToNbtError> convertArray(const std::vector<JsonValue>& array) {
    if (array.empty()) {
        return NbtTag::ofList(NbtList{NbtType::End});
    }

    std::vector<NbtTag> elements;
    elements.reserve(array.size());
    for (const JsonValue& element : array) {
        auto converted = jsonToNbt(element);
        if (!converted) {
            return converted;
        }
        if (!elements.empty() && converted->type() != elements.front().type()) {
            return std::unexpected{JsonToNbtError::MixedArray};
        }
        elements.push_back(std::move(*converted));
    }
    const NbtType type = elements.front().type();
    return NbtTag::ofList(NbtList{type, std::move(elements)});
}

} // namespace

std::expected<NbtTag, JsonToNbtError> jsonToNbt(const JsonValue& value) {
    if (value.isNull()) {
        return std::unexpected{JsonToNbtError::NullHasNoEquivalent};
    }
    if (const auto* boolean = value.get<bool>()) {
        return NbtTag::ofBoolean(*boolean);
    }
    if (const auto* integer = value.get<std::int64_t>()) {
        if (*integer >= INT32_MIN && *integer <= INT32_MAX) {
            return NbtTag::ofInt(static_cast<std::int32_t>(*integer));
        }
        return NbtTag::ofLong(*integer);
    }
    if (const auto* number = value.get<double>()) {
        return NbtTag::ofDouble(*number);
    }
    if (const auto* text = value.get<std::string>()) {
        return NbtTag::ofString(*text);
    }
    if (const auto* array = value.get<std::vector<JsonValue>>()) {
        return convertArray(*array);
    }
    return convertObject(std::get<JsonObject>(value.storage()));
}

} // namespace basalt::wire::nbt
