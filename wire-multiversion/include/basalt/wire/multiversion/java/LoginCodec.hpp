#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// Adds to the registry codec of a pre-1.20.2 login the registries that server never sent but a newer client needs.
void completeLoginCodec(std::int32_t backendProtocol, nbt::NbtCompound& codec);

} // namespace basalt::wire::multiversion::java
