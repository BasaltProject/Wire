#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/LegacyClicks.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;

constexpr std::int32_t kHorseScreen = 0x1E;
constexpr std::int32_t kContainerClose = 0x12;
constexpr std::int32_t kContainerContent = 0x13;
constexpr std::int32_t kContainerSlot = 0x15;
constexpr std::int32_t kLogin = 0x24;
constexpr std::int32_t kOpenScreen = 0x2D;
constexpr std::int32_t kRespawn = 0x39;

constexpr std::int32_t kCursorWindow = -1;
constexpr std::int32_t kInventoryDirect = -2;
constexpr std::size_t kMaxSlots = 256;
constexpr std::int32_t kMaxChanged = 128;
constexpr std::int32_t kPlayerSlots = 36;
constexpr std::int32_t kFirstMainSlot = 9;
constexpr std::int32_t kFirstHotbarSlot = 36;
constexpr std::int32_t kOffhandSlot = 45;
constexpr std::int32_t kPickup = 0;
constexpr std::int32_t kQuickMove = 1;

std::vector<std::uint8_t> readStack(io::ByteBuffer& in) {
    io::ByteBuffer out;
    const std::uint8_t present = in.readByte();
    out.writeByte(present);
    if (present != 0) {
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeByte(in.readByte());
        if (const auto tag = readNamedNbt(in); tag && !in.failed()) {
            out.writeBytes(tag->raw);
        }
    }
    return out.toVector();
}

struct Head {
    std::int32_t item = -1;
    std::int32_t count = 0;
    std::size_t rest = 0;
};

Head headOf(std::span<const std::uint8_t> stack) {
    if (stack.empty() || stack[0] == 0) {
        return {};
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(std::vector<std::uint8_t>{stack.begin(), stack.end()});
    static_cast<void>(in.readByte());
    Head head;
    head.item = pv::varInt::read(in);
    head.count = static_cast<std::int8_t>(in.readByte());
    head.rest = in.readerIndex();
    return in.failed() ? Head{} : head;
}

} // namespace

std::int32_t inventoryMenuSlot(std::int32_t inventorySlot) {
    constexpr std::int32_t kHotbar = 9;
    constexpr std::int32_t kArmorFirst = 36;
    constexpr std::int32_t kOffhand = 40;
    constexpr std::int32_t kHelmetMenuSlot = 5;
    if (inventorySlot < kHotbar) {
        return kFirstHotbarSlot + inventorySlot;
    }
    if (inventorySlot < kArmorFirst) {
        return inventorySlot;
    }
    if (inventorySlot < kOffhand) {
        return kHelmetMenuSlot + (kOffhand - 1 - inventorySlot);
    }
    return inventorySlot == kOffhand ? kOffhandSlot : -1;
}

LegacyClicks::Window* LegacyClicks::window(std::int32_t id) {
    if (id == 0) {
        return &inventory_;
    }
    return id == open_.id ? &open_ : nullptr;
}

const LegacyClicks::Stack& LegacyClicks::at(const Window& window, std::int32_t slot) const {
    static const Stack kEmpty{0};
    return slot >= 0 && static_cast<std::size_t>(slot) < window.slots.size()
               ? window.slots[static_cast<std::size_t>(slot)]
               : kEmpty;
}

void LegacyClicks::set(Window& window, std::int32_t slot, Stack stack) {
    if (slot < 0 || static_cast<std::size_t>(slot) >= kMaxSlots) {
        return;
    }
    if (static_cast<std::size_t>(slot) >= window.slots.size()) {
        window.slots.resize(static_cast<std::size_t>(slot) + 1, Stack{0});
    }
    const auto size = static_cast<std::int32_t>(window.slots.size());
    // The player's own rows close every container, and the inventory view shows the same stacks afterwards.
    if (&window == &open_ && size >= kPlayerSlots && slot >= size - kPlayerSlots) {
        set(inventory_, kFirstMainSlot + slot - (size - kPlayerSlots), stack);
    }
    window.slots[static_cast<std::size_t>(slot)] = std::move(stack);
}

void LegacyClicks::clientbound(std::int32_t id, std::span<const std::uint8_t> body) {
    io::ByteBuffer in = io::ByteBuffer::wrap(std::vector<std::uint8_t>{body.begin(), body.end()});
    switch (id) {
    case kOpenScreen:
        open_ = Window{pv::varInt::read(in), {}};
        break;
    case kHorseScreen:
        open_ = Window{in.readByte(), {}};
        break;
    case kContainerClose:
        if (in.readByte() == open_.id) {
            open_ = Window{};
        }
        break;
    case kLogin:
    case kRespawn:
        open_ = Window{};
        break;
    case kContainerContent: {
        Window* target = window(in.readByte());
        const std::int32_t count = static_cast<std::uint16_t>(in.readShort());
        if (target == nullptr) {
            break;
        }
        target->slots.assign(std::min(static_cast<std::size_t>(count), kMaxSlots), Stack{0});
        for (std::int32_t slot = 0; slot < count && !in.failed(); ++slot) {
            Stack stack = readStack(in);
            if (!in.failed()) {
                set(*target, slot, std::move(stack));
            }
        }
        break;
    }
    case kContainerSlot: {
        const auto windowId = static_cast<std::int8_t>(in.readByte());
        const std::int16_t slot = in.readShort();
        Stack stack = readStack(in);
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
    default:
        break;
    }
}

LegacyClicks::Stack LegacyClicks::resolve(std::span<const std::uint8_t> claimed, std::span<const Stack> before) const {
    const Head wanted = headOf(claimed);
    if (wanted.item < 0 || wanted.count <= 0) {
        return Stack{0};
    }
    const auto sameItem = [&](const Stack& stack) { return headOf(stack).item == wanted.item; };
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
    if (source == nullptr) {
        return Stack{claimed.begin(), claimed.end()};
    }
    const Head head = headOf(*source);
    io::ByteBuffer out;
    out.writeByte(1);
    pv::varInt::write(out, wanted.item);
    out.writeByte(static_cast<std::uint8_t>(wanted.count));
    out.writeBytes(std::span{*source}.subspan(head.rest));
    return out.toVector();
}

bool LegacyClicks::click(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::uint8_t windowId = in.readByte();
    const std::int16_t slot = in.readShort();
    const std::uint8_t button = in.readByte();
    const std::int32_t mode = pv::varInt::read(in);
    const std::int32_t changedCount = pv::readCollectionSize(in, 1);
    std::vector<std::pair<std::int16_t, Stack>> changed;
    for (std::int32_t index = 0; index < changedCount && !in.failed(); ++index) {
        const std::int16_t where = in.readShort();
        Stack stack = readStack(in);
        if (index < kMaxChanged) {
            changed.emplace_back(where, std::move(stack));
        }
    }
    const Stack carried = readStack(in);
    if (in.failed()) {
        return false;
    }
    out.writeByte(windowId);
    out.writeShort(slot);
    out.writeByte(button);
    out.writeShort(0);
    pv::varInt::write(out, mode);

    Window* target = window(windowId);
    if (target == nullptr) {
        out.writeBytes(mode == kQuickMove ? Stack{0} : carried);
        return true;
    }
    const Stack clickedBefore = at(*target, slot);
    Stack clicked{0};
    if (mode == kPickup && slot >= 0) {
        clicked = clickedBefore;
    } else if (mode == kQuickMove && slot >= 0) {
        // parity: AbstractContainerMenu.doClick from 1.11 answers a whole move with an empty stack; 1.10 with the
        // stack.
        const auto after = std::ranges::find(changed, slot, &std::pair<std::int16_t, Stack>::first);
        const bool partial = after != changed.end() && headOf(after->second).item == headOf(clickedBefore).item &&
                             headOf(clickedBefore).item >= 0;
        if (backendVersion_ < protocolVersions::kV1_11 || partial) {
            clicked = clickedBefore;
        }
    }
    out.writeBytes(clicked);

    std::vector<Stack> before;
    before.reserve(changed.size() + 2);
    before.push_back(cursor_);
    for (const auto& [where, stack] : changed) {
        before.push_back(at(*target, where));
    }
    for (const auto& [where, stack] : changed) {
        std::vector<Stack> pool;
        pool.reserve(before.size() + 1);
        pool.push_back(at(*target, where));
        pool.insert(pool.end(), before.begin(), before.end());
        set(*target, where, resolve(stack, pool));
    }
    cursor_ = resolve(carried, before);
    return true;
}

} // namespace basalt::wire::multiversion::java
