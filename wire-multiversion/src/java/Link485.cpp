#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Link485.hpp>
#include <basalt/wire/multiversion/java/MerchantShapes.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

namespace basalt::wire::multiversion::java {

bool Link485::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state == protocol::ProtocolState::Play && packet.nativeId() == protocol::play::MerchantOffersPacket::kId) {
        merchantShapes::reshape(packet, merchantShapes::k1_14_3, merchantShapes::k1_14, &legacyStacks::copyNamed);
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
