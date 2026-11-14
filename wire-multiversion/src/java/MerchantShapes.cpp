#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/MerchantShapes.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <vector>

namespace basalt::wire::multiversion::java::merchantShapes {
namespace {

namespace play = protocol::play;

constexpr std::uint8_t kEmptyFlag = 0;

[[nodiscard]] play::WireItemStack costStack(const play::MerchantCost& cost) {
    play::WireItemStack stack;
    stack.itemId = cost.itemId;
    stack.count = cost.count;
    return stack;
}

} // namespace

void toFlaggedStacks(MutablePacket& packet, std::int32_t older) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const auto offers = play::MerchantOffersPacket::read(input);
    if (!offers.has_value()) {
        return;
    }
    io::ByteBuffer output;
    protocol::varInt::write(output, offers->containerId);
    protocol::varInt::write(output, static_cast<std::int32_t>(offers->offers.size()));
    for (const play::MerchantOffer& offer : offers->offers) {
        legacyStacks::write(output, costStack(offer.costA), older);
        legacyStacks::write(output, offer.result, older);
        if (offer.costB.has_value()) {
            legacyStacks::write(output, costStack(*offer.costB), older);
        } else {
            legacyStacks::write(output, std::nullopt, older);
        }
        protocol::writeBoolean(output, offer.outOfStock);
        output.writeInt(offer.uses);
        output.writeInt(offer.maxUses);
        output.writeInt(offer.experience);
        output.writeInt(offer.specialPrice);
        output.writeFloat(offer.priceMultiplier);
        output.writeInt(offer.demand);
    }
    protocol::varInt::write(output, offers->level);
    protocol::varInt::write(output, offers->experience);
    protocol::writeBoolean(output, offers->showProgress);
    protocol::writeBoolean(output, offers->canRestock);
    packet.body = output.toVector();
}

bool reshape(io::ByteBuffer& input, io::ByteBuffer& output, const Layout& from, const Layout& to, StackCopy copy) {
    const std::int32_t window = from.varIntWindow ? protocol::varInt::read(input) : input.readInt();
    if (to.varIntWindow) {
        protocol::varInt::write(output, window);
    } else {
        output.writeInt(window);
    }
    const std::int32_t count = from.varIntCount ? protocol::readCollectionSize(input) : input.readByte();
    if (to.varIntCount) {
        protocol::varInt::write(output, count);
    } else {
        output.writeByte(static_cast<std::uint8_t>(count));
    }
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        if (!copy(input, output) || !copy(input, output)) {
            return false;
        }
        io::ByteBuffer second;
        const bool present = from.optionalSecond ? protocol::readBoolean(input) : input.peekByte() != kEmptyFlag;
        if (from.optionalSecond && present && !copy(input, second)) {
            return false;
        }
        if (!from.optionalSecond && !copy(input, second)) {
            return false;
        }
        if (to.optionalSecond) {
            protocol::writeBoolean(output, present);
            if (present) {
                output.writeBytes(second.readable());
            }
        } else if (present) {
            output.writeBytes(second.readable());
        } else {
            output.writeByte(kEmptyFlag);
        }
        output.writeByte(input.readByte());
        output.writeInt(input.readInt());
        output.writeInt(input.readInt());
        if (from.pricing) {
            const std::int32_t experience = input.readInt();
            const std::int32_t specialPrice = input.readInt();
            const float multiplier = input.readFloat();
            if (to.pricing) {
                output.writeInt(experience);
                output.writeInt(specialPrice);
                output.writeFloat(multiplier);
            }
        }
        if (from.demand) {
            const std::int32_t demand = input.readInt();
            if (to.demand) {
                output.writeInt(demand);
            }
        }
    }
    if (from.villager) {
        const std::int32_t level = protocol::varInt::read(input);
        const std::int32_t experience = protocol::varInt::read(input);
        const std::uint8_t regular = input.readByte();
        if (to.villager) {
            protocol::varInt::write(output, level);
            protocol::varInt::write(output, experience);
            output.writeByte(regular);
        }
    }
    if (from.restock) {
        const std::uint8_t restock = input.readByte();
        if (to.restock) {
            output.writeByte(restock);
        }
    }
    return !input.failed();
}

void reshape(MutablePacket& packet, const Layout& from, const Layout& to, StackCopy copy) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    if (reshape(input, output, from, to, copy)) {
        packet.body = output.toVector();
    }
}

} // namespace basalt::wire::multiversion::java::merchantShapes
