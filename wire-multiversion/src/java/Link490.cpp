#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Link490.hpp>
#include <basalt/wire/multiversion/java/MerchantShapes.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

namespace basalt::wire::multiversion::java {

bool Link490::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state == protocol::ProtocolState::Play && packet.nativeId() == protocol::play::MerchantOffersPacket::kId) {
        merchantShapes::reshape(packet, merchantShapes::k1_14_4, merchantShapes::k1_14_3, &legacyStacks::copyNamed);
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
