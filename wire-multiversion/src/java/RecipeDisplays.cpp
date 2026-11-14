#include <basalt/wire/multiversion/IdMap.hpp>
#include <basalt/wire/multiversion/java/RecipeDisplays.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <algorithm>

namespace basalt::wire::multiversion::java::recipeDisplays {
namespace {

// parity: SlotDisplays.bootstrap, this server's numbering.
constexpr std::int32_t kEmpty = 0;
constexpr std::int32_t kAnyFuel = 1;
constexpr std::int32_t kWithAnyPotion = 2;
constexpr std::int32_t kOnlyWithComponent = 3;
constexpr std::int32_t kItem = 4;
constexpr std::int32_t kItemStack = 5;
constexpr std::int32_t kTag = 6;
constexpr std::int32_t kDyed = 7;
constexpr std::int32_t kSmithingTrim = 8;
constexpr std::int32_t kWithRemainder = 9;
constexpr std::int32_t kComposite = 10;

// parity: RecipeDisplays.bootstrap.
constexpr std::int32_t kShapeless = 0;
constexpr std::int32_t kShaped = 1;
constexpr std::int32_t kFurnace = 2;
constexpr std::int32_t kStonecutter = 3;
constexpr std::int32_t kSmithing = 4;

constexpr std::int32_t kMaxDepth = 16;

class Walker {
public:
    Walker(io::ByteBuffer& input, io::ByteBuffer& output, const ComposedTables& tables)
        : in_(input), out_(output), tables_(tables) {}

    void slot(int depth = 0) {
        if (depth > kMaxDepth) {
            in_.markFailed();
            return;
        }
        const std::int32_t type = protocol::varInt::read(in_);
        switch (type) {
        case kEmpty:
        case kAnyFuel:
            writeType(type);
            break;
        case kWithAnyPotion:
            keepIfKnown(kWithAnyPotion);
            slot(depth + 1);
            break;
        case kOnlyWithComponent: {
            const bool kept = keepIfKnown(kOnlyWithComponent);
            slot(depth + 1);
            const std::int32_t component = protocol::varInt::read(in_);
            if (kept) {
                protocol::varInt::write(out_, component);
            }
            break;
        }
        case kItem:
            item(protocol::varInt::read(in_));
            break;
        case kItemStack: {
            // The stack's components are this server's; the item alone is what an older book can draw.
            const std::int32_t itemId = protocol::varInt::read(in_);
            static_cast<void>(protocol::varInt::read(in_));
            skipComponents();
            item(itemId);
            break;
        }
        case kTag:
            tagAsName();
            break;
        case kDyed: {
            // Before 26.1 there was no dyed demo; the dyed item alone stands for it.
            if (keepIfKnown(kDyed)) {
                slot(depth + 1);
            } else {
                io::ByteBuffer dye;
                Walker{in_, dye, tables_}.slot(depth + 1);
            }
            slot(depth + 1);
            break;
        }
        case kSmithingTrim:
            writeType(kSmithingTrim);
            slot(depth + 1);
            slot(depth + 1);
            holder();
            break;
        case kWithRemainder:
            writeType(kWithRemainder);
            slot(depth + 1);
            slot(depth + 1);
            break;
        case kComposite: {
            writeType(kComposite);
            const std::int32_t count = protocol::varInt::read(in_);
            protocol::varInt::write(out_, count);
            for (std::int32_t i = 0; i < count && !in_.failed(); ++i) {
                slot(depth + 1);
            }
            break;
        }
        default:
            in_.markFailed();
            break;
        }
    }

    void recipe() {
        const std::int32_t type = protocol::varInt::read(in_);
        protocol::varInt::write(out_, type);
        switch (type) {
        case kShaped:
            protocol::varInt::write(out_, protocol::varInt::read(in_));
            protocol::varInt::write(out_, protocol::varInt::read(in_));
            [[fallthrough]];
        case kShapeless: {
            const std::int32_t count = protocol::varInt::read(in_);
            protocol::varInt::write(out_, count);
            for (std::int32_t i = 0; i < count && !in_.failed(); ++i) {
                slot();
            }
            slot();
            slot();
            break;
        }
        case kFurnace:
            slot();
            slot();
            slot();
            slot();
            protocol::varInt::write(out_, protocol::varInt::read(in_));
            out_.writeFloat(in_.readFloat());
            break;
        case kStonecutter:
            slot();
            slot();
            slot();
            break;
        case kSmithing:
            for (int i = 0; i < 5; ++i) {
                slot();
            }
            break;
        default:
            in_.markFailed();
            break;
        }
    }

    void holderSet() {
        const std::int32_t size = protocol::varInt::read(in_);
        if (size == 0) {
            protocol::varInt::write(out_, 0);
            protocol::writeString(out_, protocol::readString(in_));
            return;
        }
        std::vector<std::int32_t> kept;
        for (std::int32_t i = 0; i < size - 1 && !in_.failed(); ++i) {
            const std::int32_t mapped = tables_.item.map(protocol::varInt::read(in_));
            if (mapped != IdMap::kNone) {
                kept.push_back(mapped);
            }
        }
        protocol::varInt::write(out_, static_cast<std::int32_t>(kept.size()) + 1);
        for (const std::int32_t id : kept) {
            protocol::varInt::write(out_, id);
        }
    }

private:
    /// Before 26.3 a tag display was the tag's name alone; a set of items becomes a composite of them.
    void tagAsName() {
        const std::int32_t size = protocol::varInt::read(in_);
        if (size == 0) {
            writeType(kTag);
            protocol::writeString(out_, protocol::readString(in_));
            return;
        }
        writeType(kComposite);
        protocol::varInt::write(out_, std::max(size - 1, 0));
        for (std::int32_t i = 0; i < size - 1 && !in_.failed(); ++i) {
            item(protocol::varInt::read(in_));
        }
    }

    /// Writes the wrapper's type when the client has it; otherwise only what it wraps is shown.
    bool keepIfKnown(std::int32_t nativeType) {
        if (tables_.slotDisplay.map(nativeType) == IdMap::kNone) {
            return false;
        }
        writeType(nativeType);
        return true;
    }

    void writeType(std::int32_t nativeType) {
        const std::int32_t mapped = tables_.slotDisplay.map(nativeType);
        protocol::varInt::write(out_, mapped == IdMap::kNone ? nativeType : mapped);
    }

    void item(std::int32_t nativeItem) {
        const std::int32_t mapped = tables_.item.map(nativeItem);
        if (mapped == IdMap::kNone) {
            writeType(kEmpty);
            return;
        }
        writeType(kItem);
        protocol::varInt::write(out_, mapped);
    }

    void holder() {
        const std::int32_t id = protocol::varInt::read(in_);
        if (id == 0) {
            in_.markFailed();
            return;
        }
        protocol::varInt::write(out_, id);
    }

    void skipComponents() {
        const std::int32_t added = protocol::varInt::read(in_);
        const std::int32_t removed = protocol::varInt::read(in_);
        for (std::int32_t i = 0; i < added && !in_.failed(); ++i) {
            const std::int32_t id = protocol::varInt::read(in_);
            if (!protocol::play::ItemComponent::readPayload(in_, id, std::nullopt).has_value()) {
                in_.markFailed();
            }
        }
        for (std::int32_t i = 0; i < removed && !in_.failed(); ++i) {
            static_cast<void>(protocol::varInt::read(in_));
        }
    }

    io::ByteBuffer& in_;
    io::ByteBuffer& out_;
    const ComposedTables& tables_;
};

} // namespace

void rewriteBookAdd(MutablePacket& packet, const ComposedTables& tables) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    Walker walker{input, output, tables};
    const std::int32_t entries = protocol::varInt::read(input);
    protocol::varInt::write(output, entries);
    for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
        protocol::varInt::write(output, protocol::varInt::read(input));
        walker.recipe();
        protocol::varInt::write(output, protocol::varInt::read(input));
        protocol::varInt::write(output, protocol::varInt::read(input));
        const bool requirements = protocol::readBoolean(input);
        protocol::writeBoolean(output, requirements);
        if (requirements) {
            const std::int32_t count = protocol::varInt::read(input);
            protocol::varInt::write(output, count);
            for (std::int32_t k = 0; k < count && !input.failed(); ++k) {
                walker.holderSet();
            }
        }
        output.writeByte(input.readByte());
    }
    output.writeBytes(input.readable());
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

} // namespace basalt::wire::multiversion::java::recipeDisplays
