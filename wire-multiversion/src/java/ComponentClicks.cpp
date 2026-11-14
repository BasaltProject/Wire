#include <basalt/wire/multiversion/java/ComponentClicks.hpp>
#include <basalt/wire/multiversion/java/LegacyClicks.hpp>
#include <basalt/wire/multiversion/java/Stack769.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <utility>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;

constexpr std::int32_t kContainerClose = 18;
constexpr std::int32_t kContainerContent = 19;
constexpr std::int32_t kContainerSlot = 21;
constexpr std::int32_t kHorseScreen = 36;
constexpr std::int32_t kLogin = 44;
constexpr std::int32_t kOpenScreen = 53;
constexpr std::int32_t kRespawn = 76;
constexpr std::int32_t kSetCursorItem = 90;
constexpr std::int32_t kSetPlayerInventory = 102;

constexpr std::int32_t kCursorWindow = -1;
constexpr std::int32_t kInventoryDirect = -2;
constexpr std::size_t kMaxSlots = 256;
constexpr std::int32_t kMaxChanged = 128;
constexpr std::int32_t kPlayerSlots = 36;
constexpr std::int32_t kFirstMainSlot = 9;

/// The bytes of one stack in the server's own shape, measured by reading it.
std::vector<std::uint8_t> readStack(io::ByteBuffer& in, std::span<const std::uint8_t> body) {
    const std::size_t start = in.readerIndex();
    io::ByteBuffer scratch;
    if (!stack769::convert(in, scratch) || in.failed()) {
        in.markFailed();
        return {};
    }
    return {body.begin() + static_cast<std::ptrdiff_t>(start),
            body.begin() + static_cast<std::ptrdiff_t>(in.readerIndex())};
}

struct Head {
    std::int32_t item = -1;
    std::size_t rest = 0;
};

Head headOf(std::span<const std::uint8_t> stack) {
    io::ByteBuffer in = io::ByteBuffer::wrap(std::vector<std::uint8_t>{stack.begin(), stack.end()});
    if (pv::varInt::read(in) <= 0) {
        return {};
    }
    const std::size_t rest = in.readerIndex();
    const std::int32_t item = pv::varInt::read(in);
    return in.failed() ? Head{} : Head{item, rest};
}

struct Hashed {
    std::int32_t item = -1;
    std::int32_t count = 0;
};

// parity: HashedStack, whose components only the client can check against its hashes.
Hashed readHashed(io::ByteBuffer& in) {
    if (!pv::readBoolean(in)) {
        return {};
    }
    Hashed hashed{pv::varInt::read(in), pv::varInt::read(in)};
    const std::int32_t added = pv::readCollectionSize(in, 5);
    for (std::int32_t index = 0; index < added && !in.failed(); ++index) {
        static_cast<void>(pv::varInt::read(in));
        static_cast<void>(in.readInt());
    }
    const std::int32_t removed = pv::readCollectionSize(in, 1);
    for (std::int32_t index = 0; index < removed && !in.failed(); ++index) {
        static_cast<void>(pv::varInt::read(in));
    }
    return hashed;
}

void writeNamed(const Hashed& stack, io::ByteBuffer& out) {
    if (stack.item < 0 || stack.count <= 0) {
        out.writeByte(0);
        return;
    }
    out.writeByte(1);
    pv::varInt::write(out, stack.item);
    out.writeByte(static_cast<std::uint8_t>(stack.count));
    out.writeByte(0);
}

} // namespace

ComponentClicks::Window* ComponentClicks::window(std::int32_t id) {
    if (id == 0) {
        return &inventory_;
    }
    return id == open_.id ? &open_ : nullptr;
}

const ComponentClicks::Stack& ComponentClicks::at(const Window& window, std::int32_t slot) const {
    static const Stack kEmpty{0};
    return slot >= 0 && static_cast<std::size_t>(slot) < window.slots.size()
               ? window.slots[static_cast<std::size_t>(slot)]
               : kEmpty;
}

void ComponentClicks::set(Window& window, std::int32_t slot, Stack stack) {
    if (slot < 0 || static_cast<std::size_t>(slot) >= kMaxSlots) {
        return;
    }
    if (static_cast<std::size_t>(slot) >= window.slots.size()) {
        window.slots.resize(static_cast<std::size_t>(slot) + 1, Stack{0});
    }
    const auto size = static_cast<std::int32_t>(window.slots.size());
    if (&window == &open_ && size >= kPlayerSlots && slot >= size - kPlayerSlots) {
        set(inventory_, kFirstMainSlot + slot - (size - kPlayerSlots), stack);
    }
    window.slots[static_cast<std::size_t>(slot)] = std::move(stack);
}

void ComponentClicks::clientbound(std::int32_t id, std::span<const std::uint8_t> body) {
    io::ByteBuffer in = io::ByteBuffer::wrap(std::vector<std::uint8_t>{body.begin(), body.end()});
    switch (id) {
    case kOpenScreen:
    case kHorseScreen:
        open_ = Window{pv::varInt::read(in), {}};
        break;
    case kContainerClose:
        if (pv::varInt::read(in) == open_.id) {
            open_ = Window{};
        }
        break;
    case kLogin:
    case kRespawn:
        open_ = Window{};
        break;
    case kContainerContent: {
        Window* target = window(pv::varInt::read(in));
        static_cast<void>(pv::varInt::read(in));
        const std::int32_t count = pv::readCollectionSize(in);
        if (target == nullptr) {
            break;
        }
        target->slots.assign(std::min(static_cast<std::size_t>(count), kMaxSlots), Stack{0});
        for (std::int32_t slot = 0; slot < count && !in.failed(); ++slot) {
            Stack stack = readStack(in, body);
            if (!in.failed()) {
                set(*target, slot, std::move(stack));
            }
        }
        if (Stack carried = readStack(in, body); !in.failed()) {
            cursor_ = std::move(carried);
        }
        break;
    }
    case kContainerSlot: {
        const std::int32_t windowId = pv::varInt::read(in);
        static_cast<void>(pv::varInt::read(in));
        const std::int16_t slot = in.readShort();
        Stack stack = readStack(in, body);
        if (in.failed()) {
            break;
        }
        if (windowId == kCursorWindow) {
            cursor_ = std::move(stack);
        } else if (windowId == kInventoryDirect) {
            set(inventory_, inventoryMenuSlot(slot), std::move(stack));
        } else if (Window* target = window(windowId)) {
            set(*target, slot, std::move(stack));
        }
        break;
    }
    case kSetCursorItem:
        if (Stack stack = readStack(in, body); !in.failed()) {
            cursor_ = std::move(stack);
        }
        break;
    case kSetPlayerInventory: {
        const std::int32_t slot = pv::varInt::read(in);
        if (Stack stack = readStack(in, body); !in.failed()) {
            set(inventory_, inventoryMenuSlot(slot), std::move(stack));
        }
        break;
    }
    default:
        break;
    }
}

ComponentClicks::Stack ComponentClicks::resolve(std::int32_t item, std::int32_t count,
                                                std::span<const Stack> before) const {
    if (item < 0 || count <= 0) {
        return Stack{0};
    }
    const auto sameItem = [&](const Stack& stack) { return headOf(stack).item == item; };
    const Stack* source = nullptr;
    if (const auto found = std::ranges::find_if(before, sameItem); found != before.end()) {
        source = &*found;
    } else {
        for (const Window* view : {&open_, &inventory_}) {
            if (const auto known = std::ranges::find_if(view->slots, sameItem); known != view->slots.end()) {
                source = &*known;
                break;
            }
        }
    }
    io::ByteBuffer out;
    pv::varInt::write(out, count);
    if (source == nullptr) {
        pv::varInt::write(out, item);
        pv::varInt::write(out, 0);
        pv::varInt::write(out, 0);
    } else {
        out.writeBytes(std::span{*source}.subspan(headOf(*source).rest));
    }
    return out.toVector();
}

bool ComponentClicks::click(std::int32_t windowId, io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::int32_t changedCount = pv::readCollectionSize(in, 3);
    std::vector<std::pair<std::int16_t, Hashed>> changed;
    for (std::int32_t index = 0; index < changedCount && !in.failed(); ++index) {
        const std::int16_t where = in.readShort();
        const Hashed stack = readHashed(in);
        if (index < kMaxChanged) {
            changed.emplace_back(where, stack);
        }
    }
    const Hashed carried = readHashed(in);
    if (in.failed()) {
        return false;
    }
    Window* target = window(windowId);
    std::vector<Stack> before;
    before.reserve(changed.size() + 1);
    before.push_back(cursor_);
    for (const auto& [where, stack] : changed) {
        before.push_back(target == nullptr ? Stack{0} : at(*target, where));
    }
    pv::varInt::write(out, static_cast<std::int32_t>(changed.size()));
    for (const auto& [where, stack] : changed) {
        std::vector<Stack> pool;
        pool.reserve(before.size() + 1);
        pool.push_back(target == nullptr ? Stack{0} : at(*target, where));
        pool.insert(pool.end(), before.begin(), before.end());
        Stack resolved = resolve(stack.item, stack.count, pool);
        out.writeShort(where);
        out.writeBytes(resolved);
        if (target != nullptr) {
            set(*target, where, std::move(resolved));
        }
    }
    cursor_ = resolve(carried.item, carried.count, before);
    out.writeBytes(cursor_);
    return true;
}

bool ComponentClicks::namedClick(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::int32_t changed = std::min(pv::readCollectionSize(in, 3), kMaxChanged);
    pv::varInt::write(out, changed);
    for (std::int32_t index = 0; index < changed && !in.failed(); ++index) {
        out.writeShort(in.readShort());
        writeNamed(readHashed(in), out);
    }
    writeNamed(readHashed(in), out);
    return !in.failed();
}

} // namespace basalt::wire::multiversion::java
