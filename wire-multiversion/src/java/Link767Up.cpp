#include <basalt/wire/multiversion/java/Link767Up.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/multiversion/java/NewRegistries768.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

using protocol::ProtocolState;

constexpr std::int32_t kLoginGameProfile = 2;
constexpr std::int32_t kConfigRegistryData = 7;
constexpr std::int32_t kConfigFinish = 3;

constexpr std::int32_t kContainerClose = 18;
constexpr std::int32_t kContainerContent = 19;
constexpr std::int32_t kContainerData = 20;
constexpr std::int32_t kContainerSlot = 21;
constexpr std::int32_t kCooldown = 23;
constexpr std::int32_t kExplode = 32;
constexpr std::int32_t kHorseScreen = 35;
constexpr std::int32_t kLogin = 43;
constexpr std::int32_t kPlayerPosition = 64;
constexpr std::int32_t kRecipe = 65;
constexpr std::int32_t kRespawn = 71;
constexpr std::int32_t kPlaceGhostRecipe = 55;
constexpr std::int32_t kSetCarriedItem = 83;
constexpr std::int32_t kSetTime = 100;
constexpr std::int32_t kTeleportEntity = 112;
constexpr std::int32_t kUpdateRecipes = 119;

constexpr std::int32_t kEntityPositionSync768 = 32;
constexpr std::int32_t kSetHeldSlot768 = 99;
constexpr std::int32_t kSetCursorItem768 = 90;
constexpr std::int32_t kSetPlayerInventory768 = 102;
constexpr std::int32_t kBlockUpdate768 = 9;

constexpr std::int32_t kContainerClickServer = 14;
constexpr std::int32_t kContainerCloseServer = 15;
constexpr std::int32_t kClientInformationPlay = 10;
constexpr std::int32_t kClientInformationConfig = 0;
constexpr std::int32_t kMovePos = 26;
constexpr std::int32_t kMovePosRot = 27;
constexpr std::int32_t kMoveRot = 28;
constexpr std::int32_t kAcceptTeleportation = 0;
constexpr std::int32_t kMoveStatusOnly = 29;
constexpr std::int32_t kPlaceRecipeServer = 34;
constexpr std::int32_t kPlayerInput = 38;
constexpr std::int32_t kRecipeBookSeen = 41;
constexpr std::int32_t kUseItemOn = 56;

constexpr std::int32_t kSeaLevel = 63;
constexpr float kImpulse = 0.98F;
constexpr std::int32_t kInteractionDestroy = 1;
constexpr std::int32_t kInteractionDestroyWithDecay = 2;

bool renamedAttribute(std::string& name) {
    for (const std::string_view prefix : {"minecraft:generic.", "minecraft:player.", "minecraft:zombie."}) {
        if (name.starts_with(prefix)) {
            name = "minecraft:" + name.substr(prefix.size());
            return true;
        }
    }
    if (name == "minecraft:horse.jump_strength") {
        name = "minecraft:jump_strength";
        return true;
    }
    return false;
}

void renameInEnchantment(nbt::NbtTag& tag) {
    if (auto* compound = tag.get<nbt::NbtCompound>()) {
        for (const std::string& key : std::vector<std::string>(compound->names())) {
            nbt::NbtTag* child = compound->find(key);
            if (child == nullptr) {
                continue;
            }
            if (auto* text = child->get<std::string>()) {
                if (key == "attribute") {
                    renamedAttribute(*text);
                } else if (key == "type" && *text == "minecraft:damage_item") {
                    *text = "minecraft:change_item_damage";
                }
            } else {
                renameInEnchantment(*child);
            }
        }
    } else if (auto* list = tag.get<nbt::NbtList>()) {
        for (std::size_t index = 0; index < list->size(); ++index) {
            renameInEnchantment((*list)[index]);
        }
    }
}

void rewriteEnchantments(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    auto data = protocol::configuration::RegistryDataPacket::read(in);
    if (!data.has_value() || data->registryId != "minecraft:enchantment") {
        return;
    }
    for (protocol::configuration::RegistryEntry& entry : data->entries) {
        if (entry.value) {
            renameInEnchantment(*entry.value);
        }
    }
    io::ByteBuffer out;
    data->write(out);
    packet.body = out.toVector();
}

void widenContainerByte(MutablePacket& packet) {
    if (packet.body.empty()) {
        return;
    }
    io::ByteBuffer out;
    protocol::varInt::write(out, static_cast<std::int8_t>(packet.body.front()));
    out.writeBytes(std::span<const std::uint8_t>{packet.body}.subspan(1));
    packet.body = out.toVector();
}

void narrowContainerVarInt(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t container = protocol::varInt::read(in);
    if (in.failed()) {
        return;
    }
    io::ByteBuffer out;
    out.writeByte(static_cast<std::uint8_t>(container));
    out.writeBytes(in.readable());
    packet.body = out.toVector();
}

void rewriteSlot(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const auto container = static_cast<std::int8_t>(in.readByte());
    const std::int32_t stateId = protocol::varInt::read(in);
    const std::int16_t slot = in.readShort();
    if (in.failed()) {
        return;
    }
    io::ByteBuffer out;
    if (container == -1) {
        packet.overrideId(kSetCursorItem768);
    } else if (container == -2) {
        packet.overrideId(kSetPlayerInventory768);
        protocol::varInt::write(out, slot);
    } else {
        protocol::varInt::write(out, container);
        protocol::varInt::write(out, stateId);
        out.writeShort(slot);
    }
    out.writeBytes(in.readable());
    packet.body = out.toVector();
}

void rewriteTime(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int64_t gameTime = in.readLong();
    std::int64_t dayTime = in.readLong();
    if (in.failed()) {
        return;
    }
    const bool ticking = dayTime >= 0;
    if (!ticking) {
        dayTime = -dayTime;
    }
    io::ByteBuffer out;
    out.writeLong(gameTime);
    out.writeLong(dayTime);
    protocol::writeBoolean(out, ticking);
    packet.body = out.toVector();
}

void rewritePosition(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const double x = in.readDouble();
    const double y = in.readDouble();
    const double z = in.readDouble();
    const float yaw = in.readFloat();
    const float pitch = in.readFloat();
    const std::uint8_t relative = in.readByte();
    const std::int32_t teleportId = protocol::varInt::read(in);
    if (in.failed()) {
        return;
    }
    std::int32_t flags = relative & 0x1F;
    for (std::int32_t axis = 0; axis < 3; ++axis) {
        if ((relative & (1 << axis)) != 0) {
            flags |= 1 << (5 + axis);
        }
    }
    io::ByteBuffer out;
    protocol::varInt::write(out, teleportId);
    out.writeDouble(x);
    out.writeDouble(y);
    out.writeDouble(z);
    for (int index = 0; index < 3; ++index) {
        out.writeDouble(0.0);
    }
    out.writeFloat(yaw);
    out.writeFloat(pitch);
    out.writeInt(flags);
    packet.body = out.toVector();
}

void rewriteTeleportEntity(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entity = protocol::varInt::read(in);
    const double x = in.readDouble();
    const double y = in.readDouble();
    const double z = in.readDouble();
    const float yaw = static_cast<std::int8_t>(in.readByte()) * 360.0F / 256.0F;
    const float pitch = static_cast<std::int8_t>(in.readByte()) * 360.0F / 256.0F;
    const bool onGround = in.readByte() != 0;
    if (in.failed()) {
        return;
    }
    io::ByteBuffer out;
    protocol::varInt::write(out, entity);
    out.writeDouble(x);
    out.writeDouble(y);
    out.writeDouble(z);
    for (int index = 0; index < 3; ++index) {
        out.writeDouble(0.0);
    }
    out.writeFloat(yaw);
    out.writeFloat(pitch);
    protocol::writeBoolean(out, onGround);
    packet.body = out.toVector();
    packet.overrideId(kEntityPositionSync768);
}

void rewriteLoginPlay(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    LoginFields login = LoginFields::read(in, false, false, false);
    if (in.failed()) {
        return;
    }
    login.spawn.seaLevel = kSeaLevel;
    io::ByteBuffer out;
    login.write(out, false, true, false);
    packet.body = out.toVector();
}

void rewriteRespawn(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    LoginFields::SpawnFields spawn = LoginFields::SpawnFields::read(in, false, false);
    const std::uint8_t dataToKeep = in.readByte();
    if (in.failed()) {
        return;
    }
    spawn.seaLevel = kSeaLevel;
    io::ByteBuffer out;
    spawn.write(out, true, false);
    out.writeByte(dataToKeep);
    packet.body = out.toVector();
}

void explosionToBlockUpdates(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const auto centerX = static_cast<std::int32_t>(std::floor(in.readDouble()));
    const auto centerY = static_cast<std::int32_t>(std::floor(in.readDouble()));
    const auto centerZ = static_cast<std::int32_t>(std::floor(in.readDouble()));
    static_cast<void>(in.readFloat());
    const std::int32_t count = protocol::varInt::read(in);
    if (in.failed() || count < 0 || count > 1 << 16) {
        return;
    }
    std::vector<std::int64_t> positions;
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        const std::int32_t x = centerX + static_cast<std::int8_t>(in.readByte());
        const std::int32_t y = centerY + static_cast<std::int8_t>(in.readByte());
        const std::int32_t z = centerZ + static_cast<std::int8_t>(in.readByte());
        positions.push_back(((static_cast<std::int64_t>(x) & 0x3FFFFFF) << 38) |
                            ((static_cast<std::int64_t>(z) & 0x3FFFFFF) << 12) |
                            (static_cast<std::int64_t>(y) & 0xFFF));
    }
    in.skip(12);
    const std::int32_t mode = protocol::varInt::read(in);
    if (in.failed() || (mode != kInteractionDestroy && mode != kInteractionDestroyWithDecay)) {
        return;
    }
    for (const std::int64_t position : positions) {
        io::ByteBuffer body;
        body.writeLong(position);
        protocol::varInt::write(body, 0);
        packet.extras.emplace_back(kBlockUpdate768, body.toVector());
    }
}

void flagsToGround(MutablePacket& packet) {
    if (!packet.body.empty()) {
        packet.body.back() &= 1;
    }
}

void inputToVehicle(MutablePacket& packet) {
    if (packet.body.empty()) {
        return;
    }
    const std::uint8_t flags = packet.body.front();
    const bool forward = (flags & 1) != 0;
    const bool backward = (flags & 2) != 0;
    const bool left = (flags & 4) != 0;
    const bool right = (flags & 8) != 0;
    io::ByteBuffer out;
    out.writeFloat(left ? kImpulse : (right ? -kImpulse : 0.0F));
    out.writeFloat(forward ? kImpulse : (backward ? -kImpulse : 0.0F));
    out.writeByte(static_cast<std::uint8_t>(((flags & 16) != 0 ? 1 : 0) | ((flags & 32) != 0 ? 2 : 0)));
    packet.body = out.toVector();
}

void dropBorderHit(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    static_cast<void>(protocol::varInt::read(in));
    in.skip(8);
    static_cast<void>(protocol::varInt::read(in));
    in.skip(12);
    if (in.failed() || in.readableBytes() < 3) {
        return;
    }
    const std::size_t prefix = packet.body.size() - in.readableBytes();
    packet.body.erase(packet.body.begin() + static_cast<std::ptrdiff_t>(prefix + 1));
}

} // namespace

bool Link767Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Login) {
        if (packet.id == kLoginGameProfile && !packet.body.empty()) {
            packet.body.pop_back();
        }
        return true;
    }
    if (state == ProtocolState::Configuration) {
        if (packet.id == kConfigRegistryData) {
            rewriteEnchantments(packet);
        } else if (packet.id == kConfigFinish) {
            const std::vector<std::uint8_t> finishBody = packet.body;
            const auto added = registriesAddedIn1_21_2();
            packet.overrideId(kConfigRegistryData);
            packet.body.assign(added[0].begin(), added[0].end());
            packet.extras.emplace_back(kConfigFinish, finishBody);
        }
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kLogin:
        rewriteLoginPlay(packet);
        break;
    case kRespawn:
        rewriteRespawn(packet);
        break;
    case kPlayerPosition:
        rewritePosition(packet);
        break;
    case kTeleportEntity:
        rewriteTeleportEntity(packet);
        break;
    case kSetCarriedItem:
        packet.overrideId(kSetHeldSlot768);
        break;
    case kSetTime:
        rewriteTime(packet);
        break;
    case kContainerSlot:
        rewriteSlot(packet);
        break;
    case kContainerContent:
    case kContainerClose:
    case kContainerData:
    case kHorseScreen:
        widenContainerByte(packet);
        break;
    case kExplode:
        explosionToBlockUpdates(packet);
        return false;
    case kCooldown:
    case kRecipe:
    case kUpdateRecipes:
    case kPlaceGhostRecipe:
        return false;
    default:
        break;
    }
    return true;
}

bool Link767Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration) {
        if (packet.id == kClientInformationConfig && !packet.body.empty()) {
            packet.body.pop_back();
        }
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kContainerCloseServer:
    case kContainerClickServer:
        narrowContainerVarInt(packet);
        break;
    case kClientInformationPlay:
        if (!packet.body.empty()) {
            packet.body.pop_back();
        }
        break;
    case kMovePosRot:
        flagsToGround(packet);
        if (dropNextMove_) {
            dropNextMove_ = false;
            return false;
        }
        break;
    case kAcceptTeleportation:
        // A 1.21 server would see the confirming position in the tick of the teleport and flag it as too fast.
        dropNextMove_ = true;
        break;
    case kMovePos:
    case kMoveRot:
    case kMoveStatusOnly:
        flagsToGround(packet);
        break;
    case kPlayerInput:
        inputToVehicle(packet);
        break;
    case kUseItemOn:
        dropBorderHit(packet);
        break;
    case kPlaceRecipeServer:
    case kRecipeBookSeen:
        return false;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
