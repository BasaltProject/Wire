#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {

class UpEntityData final : public PacketContentRewriter {
public:
    UpEntityData(PacketContentRewriter& next, const ComposedTables& tables, std::int32_t backendVersion);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    struct Retyped {
        std::vector<std::uint8_t> spawn;
        std::vector<std::uint8_t> passengers;
        std::int32_t variant = 0;
    };

    struct Worn {
        std::int32_t entityId;
        protocol::play::SetEquipmentPacket::SlotType slot;
        std::int32_t item;
    };

    struct RawKind {
        std::int32_t older;
        std::int32_t native;
    };

    [[nodiscard]] bool entityData(MutablePacket& packet);
    [[nodiscard]] bool becomesCat(protocol::play::SetEntityDataPacket& data);
    void trackRetyped(const MutablePacket& packet);
    void respawn(MutablePacket& packet, std::int32_t entityId);

    [[nodiscard]] static std::span<const std::int32_t> rawKinds();
    [[nodiscard]] static std::optional<std::vector<std::uint8_t>> readRaw(std::int32_t kind, io::ByteBuffer& in);
    [[nodiscard]] static protocol::play::ParticleOptionShape nativeShape(std::int32_t particleId);
    [[nodiscard]] std::optional<std::optional<protocol::play::WireItemStack>> olderItem(io::ByteBuffer& in) const;
    [[nodiscard]] std::optional<protocol::play::SetEntityDataPacket::Value> olderParticle(io::ByteBuffer& in) const;
    [[nodiscard]] std::int32_t nativeIndex(std::int32_t nativeType, std::int32_t index) const;
    [[nodiscard]] protocol::play::SetEntityDataPacket::Value blockState(std::int32_t serializer,
                                                                        std::int32_t state) const;
    void retype(std::int32_t entityId, std::int32_t type, protocol::play::SetEntityDataPacket::Entry& entry);

    PacketContentRewriter* next_;
    const ComposedTables* tables_;
    std::int32_t backendVersion_;
    std::vector<std::int32_t> serializers_;
    std::int32_t olderItemSerializer_ = -1;
    std::int32_t olderTextSerializer_ = -1;
    std::int32_t olderOptionalTextSerializer_ = -1;
    std::int32_t olderBlockStateSerializer_ = -1;
    std::int32_t olderOptionalBlockStateSerializer_ = -1;
    std::int32_t olderCompoundSerializer_ = -1;
    std::int32_t olderParticleSerializer_ = -1;
    /// Serializers the native reader has no value for, copied as raw bytes.
    std::vector<RawKind> rawKinds_;
    std::unordered_map<std::int32_t, std::int32_t> types_;
    /// Boats from before 1.21.2 and ocelots from before 1.14, whose type now depends on their entity data.
    std::unordered_map<std::int32_t, Retyped> retyped_;
    std::int32_t respawn_ = -1;
    /// Everything an ocelot from before 1.14 has sent, for the cat it becomes when tamed.
    std::unordered_map<std::int32_t, std::vector<protocol::play::SetEntityDataPacket::Entry>> ocelots_;
    /// Saddles from before 1.21.5 and horse armour from before 1.14, entity data that is now equipment.
    std::vector<Worn> equipment_;
};

} // namespace basalt::wire::multiversion::java
