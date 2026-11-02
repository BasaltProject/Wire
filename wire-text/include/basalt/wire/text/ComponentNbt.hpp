#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::wire::text::componentNbt {

[[nodiscard]] nbt::NbtTag serialize(const Component& component);

/// The pre-1.21.5 shape of click and hover events (`clickEvent`/`hoverEvent`), for older clients.
[[nodiscard]] nbt::NbtTag serializeLegacy(const Component& component);

/// While alive on this thread, `serialize` writes the pre-1.21.5 event shape. Lets a version link re-encode a packet
/// whose writer knows nothing about the client's version.
class ScopedLegacyEvents {
public:
    ScopedLegacyEvents() noexcept;
    ~ScopedLegacyEvents();
    ScopedLegacyEvents(const ScopedLegacyEvents&) = delete;
    ScopedLegacyEvents& operator=(const ScopedLegacyEvents&) = delete;

private:
    bool previous_;
};

[[nodiscard]] Component deserialize(const nbt::NbtTag& tag);

} // namespace basalt::wire::text::componentNbt
