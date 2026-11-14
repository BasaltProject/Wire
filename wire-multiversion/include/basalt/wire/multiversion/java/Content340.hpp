#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

/// What a 1.12.2 server names by id and data, as 1.13 names it after the flattening.
namespace basalt::wire::multiversion::java::content340 {

void upStack(io::ByteBuffer& in, io::ByteBuffer& out);

void downStack(io::ByteBuffer& in, io::ByteBuffer& out);

/// A 1.8 stack as 1.9 writes it: potion and spawn egg damage become tags, and back.
void upLegacyStack(io::ByteBuffer& in, io::ByteBuffer& out);

void downLegacyStack(io::ByteBuffer& in, io::ByteBuffer& out);

/// The 1.13 item a 1.12.2 id and damage name, or -1.
[[nodiscard]] std::int32_t upItem(std::int32_t id, std::int32_t damage);

/// The 1.13 state of a 1.12.2 `id << 4 | meta`.
[[nodiscard]] std::int32_t blockState(std::int32_t legacy);

/// The same for the other packing 1.12.2 sends, `id | meta << 12`.
[[nodiscard]] std::int32_t blockStateOfStateId(std::int32_t value);

/// The 1.13 particle for a 1.12.2 one, or -1.
[[nodiscard]] std::int32_t particleId(std::int32_t legacy);

/// Writes what follows a particle's id in 1.13, out of the numbers 1.12.2 sent with it.
void particleData(io::ByteBuffer& out, std::int32_t legacy, std::span<const std::int32_t> data,
                  std::span<const float> colour);

[[nodiscard]] std::string legacyToJson(std::string_view legacy);

/// A chat component with the translation keys 1.13 renamed under their new name, or empty when nothing changed.
[[nodiscard]] std::string renamedKeys(std::string_view json);

/// Copies one entity data value as 1.9 to 1.12.2 write it, untouched; false for a type they do not have.
[[nodiscard]] bool copyEntityValue(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t type);

} // namespace basalt::wire::multiversion::java::content340
