#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Link47Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;

constexpr std::int32_t kAddPlayer = 0x0C;
constexpr std::int32_t kAddEntity = 0x0E;
constexpr std::int32_t kAddMob = 0x0F;
constexpr std::int32_t kAddPainting = 0x10;
constexpr std::int32_t kAddOrb = 0x11;
constexpr std::int32_t kRemoveEntities = 0x13;
constexpr std::int32_t kMovePos = 0x15;
constexpr std::int32_t kMovePosRot = 0x17;
constexpr std::int32_t kTeleport = 0x18;
constexpr std::int32_t kEntityLink = 0x1B;
constexpr std::int32_t kEntityData = 0x1C;
constexpr std::int32_t kAddGlobal = 0x2C;
constexpr std::int32_t kEquipment = 0x04;
constexpr std::int32_t kPlayerInfo = 0x38;
constexpr std::uint8_t kPotionItemIndex = 6;
constexpr std::uint8_t kDragonPhaseIndex = 11;

constexpr std::int32_t kPassengers19 = 0x40;
constexpr std::int32_t kEquipment19 = 0x3C;
constexpr std::int32_t kEntityData19 = 0x39;
constexpr std::int32_t kBossBar19 = 0x0C;

constexpr std::int32_t kObject = 1000;
constexpr std::int32_t kPlayer = -2;
constexpr std::int32_t kArmorStand = kObject + 78;
constexpr std::int32_t kPotion = kObject + 73;
constexpr std::int32_t kItem = kObject + 2;
constexpr std::int32_t kBoat = kObject + 1;
constexpr std::int32_t kMinecart = kObject + 10;
constexpr std::int32_t kArrow = kObject + 60;
constexpr std::int32_t kWitherSkull = kObject + 66;
constexpr std::int32_t kItemFrame = kObject + 71;
constexpr std::int32_t kFirework = kObject + 76;
constexpr std::int32_t kDragon = 63;
constexpr std::int32_t kWither = 64;

constexpr double kFixedPoint = 32.0;
constexpr std::int16_t kMoveScale = 128;
constexpr std::uint8_t kEnd18 = 0x7F;
constexpr std::uint8_t kEnd19 = 0xFF;

enum Out : std::int8_t {
    kDrop = -1,
    kByte = 0,
    kVarInt = 1,
    kFloat = 2,
    kString = 3,
    kChat = 4,
    kSlot = 5,
    kBool = 6,
    kRotation = 7,
    kOptionalUuid = 11,
    kOptionalBlock = 12,
    kAge = 13
};

struct Rule {
    std::uint8_t from;
    std::int8_t to;
    Out type;
};

constexpr std::array<Rule, 5> kEntity = {
    {{0, 0, kByte}, {1, 1, kVarInt}, {2, 2, kString}, {3, 3, kBool}, {4, 4, kBool}}};
constexpr std::array<Rule, 4> kLiving = {{{6, 6, kFloat}, {7, 7, kVarInt}, {8, 8, kBool}, {9, 9, kVarInt}}};
constexpr std::array<Rule, 1> kMob = {{{15, 10, kByte}}};
constexpr std::array<Rule, 1> kAgeable = {{{12, 11, kAge}}};

struct KindRules {
    std::int32_t kind;
    std::span<const Rule> rules;
};

constexpr std::array<Rule, 7> kArmorStandRules = {{{10, 10, kByte},
                                                   {11, 11, kRotation},
                                                   {12, 12, kRotation},
                                                   {13, 13, kRotation},
                                                   {14, 14, kRotation},
                                                   {15, 15, kRotation},
                                                   {16, 16, kRotation}}};
constexpr std::array<Rule, 3> kPlayerRules = {{{10, 12, kByte}, {17, 10, kFloat}, {18, 11, kVarInt}}};
constexpr std::array<Rule, 5> kHorse = {
    {{16, 12, kByte}, {19, 13, kVarInt}, {20, 14, kVarInt}, {21, 15, kOptionalUuid}, {22, 16, kVarInt}}};
constexpr std::array<Rule, 1> kBat = {{{16, 11, kByte}}};
constexpr std::array<Rule, 3> kOcelot = {{{16, 12, kByte}, {17, 13, kOptionalUuid}, {18, 14, kVarInt}}};
constexpr std::array<Rule, 5> kWolf = {
    {{16, 12, kByte}, {17, 13, kOptionalUuid}, {18, 14, kFloat}, {19, 15, kBool}, {20, 16, kVarInt}}};
constexpr std::array<Rule, 1> kPig = {{{16, 12, kBool}}};
constexpr std::array<Rule, 1> kRabbit = {{{18, 12, kVarInt}}};
constexpr std::array<Rule, 1> kSheep = {{{16, 12, kByte}}};
constexpr std::array<Rule, 1> kVillager = {{{16, 12, kVarInt}}};
constexpr std::array<Rule, 2> kEnderman = {{{16, 11, kOptionalBlock}, {18, 12, kBool}}};
constexpr std::array<Rule, 3> kZombie = {{{12, 11, kBool}, {13, 12, kVarInt}, {14, 13, kBool}}};
constexpr std::array<Rule, 1> kClimber = {{{16, 11, kByte}}};
constexpr std::array<Rule, 3> kCreeper = {{{16, 11, kVarInt}, {17, 12, kBool}, {18, 13, kBool}}};
constexpr std::array<Rule, 1> kGhast = {{{16, 11, kBool}}};
constexpr std::array<Rule, 1> kSlime = {{{16, 11, kVarInt}}};
constexpr std::array<Rule, 1> kSkeleton = {{{13, 11, kVarInt}}};
constexpr std::array<Rule, 1> kWitch = {{{21, 11, kBool}}};
constexpr std::array<Rule, 1> kGolem = {{{16, 11, kByte}}};
constexpr std::array<Rule, 4> kWitherRules = {
    {{17, 11, kVarInt}, {18, 12, kVarInt}, {19, 13, kVarInt}, {20, 14, kVarInt}}};
constexpr std::array<Rule, 2> kGuardian = {{{16, 11, kByte}, {17, 12, kVarInt}}};
constexpr std::array<Rule, 3> kBoatRules = {{{17, 5, kVarInt}, {18, 6, kVarInt}, {19, 7, kFloat}}};
constexpr std::array<Rule, 9> kMinecartRules = {{{16, 11, kBool},
                                                 {17, 5, kVarInt},
                                                 {18, 6, kVarInt},
                                                 {19, 7, kFloat},
                                                 {20, 8, kVarInt},
                                                 {21, 9, kVarInt},
                                                 {22, 10, kBool},
                                                 {23, 11, kString},
                                                 {24, 12, kChat}}};
constexpr std::array<Rule, 1> kItemRules = {{{10, 5, kSlot}}};
constexpr std::array<Rule, 1> kArrowRules = {{{16, 5, kByte}}};
constexpr std::array<Rule, 1> kFireworkRules = {{{8, 5, kSlot}}};
constexpr std::array<Rule, 2> kItemFrameRules = {{{8, 5, kSlot}, {9, 6, kVarInt}}};
constexpr std::array<Rule, 1> kWitherSkullRules = {{{10, 5, kBool}}};

constexpr std::array<KindRules, 34> kKinds = {{
    {kArmorStand, kArmorStandRules},
    {kPlayer, kPlayerRules},
    {100, kHorse},
    {65, kBat},
    {98, kOcelot},
    {95, kWolf},
    {90, kPig},
    {101, kRabbit},
    {91, kSheep},
    {120, kVillager},
    {58, kEnderman},
    {54, kZombie},
    {57, kZombie},
    {61, kClimber},
    {52, kClimber},
    {59, kClimber},
    {50, kCreeper},
    {56, kGhast},
    {55, kSlime},
    {62, kSlime},
    {51, kSkeleton},
    {66, kWitch},
    {99, kGolem},
    {kWither, kWitherRules},
    {68, kGuardian},
    {kBoat, kBoatRules},
    {kMinecart, kMinecartRules},
    {kItem, kItemRules},
    {kArrow, kArrowRules},
    {kFirework, kFireworkRules},
    {kItemFrame, kItemFrameRules},
    {kWitherSkull, kWitherSkullRules},
    {kObject + 51, {}},
    {kObject + 90, {}},
}};

[[nodiscard]] bool living(std::int32_t kind) {
    return kind == kPlayer || kind == kArmorStand || (kind >= 48 && kind < kObject);
}

[[nodiscard]] bool ageable(std::int32_t kind) {
    constexpr std::array<std::int32_t, 10> kAgeables = {90, 91, 92, 93, 95, 96, 98, 100, 101, 120};
    return std::ranges::find(kAgeables, kind) != kAgeables.end();
}

[[nodiscard]] const Rule* ruleFor(std::int32_t kind, std::uint8_t index) {
    const auto search = [index](std::span<const Rule> rules) -> const Rule* {
        const auto found = std::ranges::find(rules, index, &Rule::from);
        return found == rules.end() ? nullptr : &*found;
    };
    if (const Rule* rule = search(kEntity)) {
        return rule;
    }
    if (living(kind)) {
        if (const Rule* rule = search(kLiving)) {
            return rule;
        }
    }
    if (living(kind) && kind != kPlayer && kind != kArmorStand) {
        if (const Rule* rule = search(kMob)) {
            return rule;
        }
    }
    if (ageable(kind)) {
        if (const Rule* rule = search(kAgeable)) {
            return rule;
        }
    }
    for (const KindRules& one : kKinds) {
        if (one.kind == kind) {
            return search(one.rules);
        }
    }
    return nullptr;
}

/// A 1.8 slot as 1.9 shapes it: the same bytes, with the potion and spawn egg damage moved into tags.
void copySlot(io::ByteBuffer& in, io::ByteBuffer& out) { content340::upLegacyStack(in, out); }

struct Value {
    std::int64_t number = 0;
    float decimal = 0.0F;
    std::string text;
    std::vector<std::uint8_t> raw;
};

[[nodiscard]] bool readValue(io::ByteBuffer& in, std::int32_t type, Value& value) {
    switch (type) {
    case 0:
        value.number = static_cast<std::int8_t>(in.readByte());
        return true;
    case 1:
        value.number = in.readShort();
        return true;
    case 2:
        value.number = in.readInt();
        return true;
    case 3:
        value.decimal = in.readFloat();
        return true;
    case 4:
        value.text = pv::readString(in);
        return true;
    case 5: {
        io::ByteBuffer slot;
        copySlot(in, slot);
        value.raw = slot.toVector();
        return true;
    }
    case 6:
    case 7: {
        const std::span<const std::uint8_t> bytes = in.readSpan(12);
        value.raw.assign(bytes.begin(), bytes.end());
        return true;
    }
    default:
        return false;
    }
}

using Players = std::unordered_map<std::string, std::array<std::uint8_t, 16>>;

/// An owner 1.8 wrote as a UUID in text, which is how vanilla 1.8 keeps it, or nothing.
[[nodiscard]] std::optional<std::array<std::uint8_t, 16>> parseUuid(std::string_view text) {
    constexpr std::size_t kTextLength = 36;
    if (text.size() != kTextLength) {
        return std::nullopt;
    }
    std::array<std::uint8_t, 16> uuid{};
    std::size_t nibble = 0;
    for (const char one : text) {
        if (one == '-') {
            continue;
        }
        const int digit = one >= '0' && one <= '9' ? one - '0' : one >= 'a' && one <= 'f' ? one - 'a' + 10 : -1;
        if (digit < 0 || nibble >= 32) {
            return std::nullopt;
        }
        uuid[nibble / 2] = static_cast<std::uint8_t>(uuid[nibble / 2] | (digit << (nibble % 2 == 0 ? 4 : 0)));
        ++nibble;
    }
    return nibble == 32 ? std::optional{uuid} : std::nullopt;
}

void writeValue(io::ByteBuffer& out, Out type, const Value& value, const Players& players) {
    switch (type) {
    case kByte:
        out.writeByte(static_cast<std::uint8_t>(value.number));
        break;
    case kVarInt:
        pv::varInt::write(out, static_cast<std::int32_t>(value.number));
        break;
    case kFloat:
        out.writeFloat(value.decimal);
        break;
    case kString:
        pv::writeString(out, value.text);
        break;
    case kChat:
        pv::writeString(out, content340::legacyToJson(value.text));
        break;
    case kBool:
        out.writeByte(value.number != 0 ? 1 : 0);
        break;
    case kAge:
        out.writeByte(value.number < 0 ? 1 : 0);
        break;
    case kOptionalUuid: {
        std::optional<std::array<std::uint8_t, 16>> owner = parseUuid(value.text);
        if (const auto found = players.find(value.text); !owner && found != players.end()) {
            owner = found->second;
        }
        out.writeByte(owner ? 1 : 0);
        if (owner) {
            out.writeBytes(*owner);
        }
        break;
    }
    case kOptionalBlock:
        pv::varInt::write(out, static_cast<std::int32_t>(value.number & 0xFFF) << 4);
        break;
    default:
        out.writeBytes(value.raw);
        break;
    }
}

[[nodiscard]] std::int32_t typeOut(Out type) { return type == kAge ? kBool : static_cast<std::int32_t>(type); }

[[nodiscard]] std::int64_t uuidHalf(std::int32_t entity, std::int64_t salt) {
    return static_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(entity)) << 16) ^
                                     static_cast<std::uint64_t>(salt));
}

void writeUuid(io::ByteBuffer& out, std::int32_t entity) {
    constexpr std::int64_t kVersionThree = 0x3000;
    constexpr std::int64_t kVariant = static_cast<std::int64_t>(0x8000000000000000ULL);
    out.writeLong(uuidHalf(entity, kVersionThree));
    out.writeLong(kVariant | entity);
}

void writeFixed(io::ByteBuffer& in, io::ByteBuffer& out, int count) {
    for (int axis = 0; axis < count; ++axis) {
        out.writeDouble(in.readInt() / kFixedPoint);
    }
}

} // namespace

bool Link47Up::entityData(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t entity, MutablePacket& packet) {
    const auto found = kinds_.find(entity);
    const std::int32_t kind = found == kinds_.end() ? kPlayer : found->second;
    const auto boss = bosses_.find(entity);
    while (!in.failed()) {
        const std::uint8_t header = in.readByte();
        if (header == kEnd18) {
            break;
        }
        const std::int32_t type = header >> 5;
        const auto index = static_cast<std::uint8_t>(header & 0x1F);
        Value value;
        if (!readValue(in, type, value)) {
            return false;
        }
        if (boss != bosses_.end() && (index == 2 || index == 6)) {
            io::ByteBuffer bar;
            bar.writeLong(boss->second.most);
            bar.writeLong(boss->second.least);
            if (index == 6) {
                pv::varInt::write(bar, 2);
                bar.writeFloat(std::clamp(value.decimal / boss->second.maxHealth, 0.0F, 1.0F));
            } else {
                pv::varInt::write(bar, 3);
                pv::writeString(bar, content340::legacyToJson(value.text));
            }
            packet.forwarded.emplace_back(kBossBar19, bar.toVector());
        }
        const Rule* rule = ruleFor(kind, index);
        if (rule == nullptr || rule->to < 0) {
            continue;
        }
        out.writeByte(static_cast<std::uint8_t>(rule->to));
        out.writeByte(static_cast<std::uint8_t>(typeOut(rule->type)));
        writeValue(out, rule->type, value, players_);
    }
    out.writeByte(kEnd19);
    return !in.failed();
}

bool Link47Up::spawn(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    const std::int32_t entity = pv::varInt::read(in);
    pv::varInt::write(out, entity);
    switch (packet.id) {
    case kAddEntity: {
        writeUuid(out, entity);
        const std::int32_t type = in.readByte();
        out.writeByte(static_cast<std::uint8_t>(type));
        writeFixed(in, out, 3);
        out.writeBytes(in.readSpan(2));
        const std::int32_t data = in.readInt();
        out.writeInt(data);
        const std::span<const std::uint8_t> velocity = data > 0 ? in.readSpan(6) : std::span<const std::uint8_t>{};
        if (data > 0) {
            out.writeBytes(velocity);
        } else {
            out.writeBytes(std::array<std::uint8_t, 6>{});
        }
        kinds_[entity] = kObject + type;
        if (kObject + type == kPotion) {
            io::ByteBuffer legacy;
            legacy.writeShort(373);
            legacy.writeByte(1);
            legacy.writeShort(static_cast<std::int16_t>(data));
            legacy.writeByte(0);
            io::ByteBuffer read = io::ByteBuffer::wrap(legacy.toVector());
            io::ByteBuffer meta;
            pv::varInt::write(meta, entity);
            meta.writeByte(kPotionItemIndex);
            meta.writeByte(kSlot);
            content340::upLegacyStack(read, meta);
            meta.writeByte(kEnd19);
            packet.forwarded.emplace_back(kEntityData19, meta.toVector());
        }
        break;
    }
    case kAddMob: {
        writeUuid(out, entity);
        const std::int32_t type = in.readByte();
        out.writeByte(static_cast<std::uint8_t>(type));
        writeFixed(in, out, 3);
        out.writeBytes(in.readSpan(3 + 6));
        kinds_[entity] = type;
        if (type == kWither || type == kDragon) {
            constexpr float kWitherHealth = 300.0F;
            constexpr float kDragonHealth = 200.0F;
            const Boss boss{uuidHalf(entity, 0x4000), static_cast<std::int64_t>(0x8000000000000000ULL) | entity,
                            type == kWither ? kWitherHealth : kDragonHealth};
            bosses_[entity] = boss;
            io::ByteBuffer bar;
            bar.writeLong(boss.most);
            bar.writeLong(boss.least);
            pv::varInt::write(bar, 0);
            pv::writeString(bar, type == kWither ? R"({"translate":"entity.WitherBoss.name"})"
                                                 : R"({"translate":"entity.EnderDragon.name"})");
            bar.writeFloat(1.0F);
            pv::varInt::write(bar, type == kWither ? 5 : 0);
            pv::varInt::write(bar, 0);
            bar.writeByte(0);
            packet.forwarded.emplace_back(kBossBar19, bar.toVector());
        }
        if (!entityData(in, out, entity, packet)) {
            return false;
        }
        if (type == kDragon) {
            // parity: EnderDragonPhase.HOLDING_PATTERN, the circling a 1.8 dragon does.
            io::ByteBuffer phase;
            pv::varInt::write(phase, entity);
            phase.writeByte(kDragonPhaseIndex);
            phase.writeByte(kVarInt);
            pv::varInt::write(phase, 0);
            phase.writeByte(kEnd19);
            packet.forwarded.emplace_back(kEntityData19, phase.toVector());
        }
        break;
    }
    case kAddPlayer: {
        out.writeBytes(in.readSpan(16));
        writeFixed(in, out, 3);
        out.writeBytes(in.readSpan(2));
        const std::int16_t held = in.readShort();
        kinds_[entity] = kPlayer;
        if (!entityData(in, out, entity, packet)) {
            return false;
        }
        if (held > 0) {
            io::ByteBuffer equipment;
            pv::varInt::write(equipment, entity);
            pv::varInt::write(equipment, 0);
            equipment.writeShort(held);
            equipment.writeByte(1);
            equipment.writeShort(0);
            equipment.writeByte(0);
            packet.forwarded.emplace_back(kEquipment19, equipment.toVector());
        }
        break;
    }
    case kAddPainting:
        writeUuid(out, entity);
        out.writeBytes(in.readable());
        break;
    case kAddOrb:
        writeFixed(in, out, 3);
        out.writeBytes(in.readable());
        break;
    case kAddGlobal:
        out.writeByte(in.readByte());
        writeFixed(in, out, 3);
        break;
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

void Link47Up::forget(std::int32_t entity, MutablePacket& packet) {
    kinds_.erase(entity);
    vehicles_.erase(entity);
    const auto boss = bosses_.find(entity);
    if (boss == bosses_.end()) {
        return;
    }
    io::ByteBuffer bar;
    bar.writeLong(boss->second.most);
    bar.writeLong(boss->second.least);
    pv::varInt::write(bar, 1);
    packet.forwarded.emplace_back(kBossBar19, bar.toVector());
    bosses_.erase(boss);
}

void Link47Up::rememberPlayers(const MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    if (pv::varInt::read(in) != 0) {
        return;
    }
    const std::int32_t count = pv::readCollectionSize(in, 17);
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        std::array<std::uint8_t, 16> uuid{};
        const std::span<const std::uint8_t> bytes = in.readSpan(16);
        if (in.failed()) {
            return;
        }
        std::ranges::copy(bytes, uuid.begin());
        std::string name = pv::readString(in);
        const std::int32_t properties = pv::readCollectionSize(in);
        for (std::int32_t property = 0; property < properties && !in.failed(); ++property) {
            static_cast<void>(pv::readString(in));
            static_cast<void>(pv::readString(in));
            if (pv::readBoolean(in)) {
                static_cast<void>(pv::readString(in));
            }
        }
        static_cast<void>(pv::varInt::read(in));
        static_cast<void>(pv::varInt::read(in));
        if (pv::readBoolean(in)) {
            static_cast<void>(pv::readString(in));
        }
        if (!in.failed()) {
            players_[std::move(name)] = uuid;
        }
    }
}

bool Link47Up::vehicle(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t rider = in.readInt();
    const std::int32_t mount = in.readInt();
    const bool leash = pv::readBoolean(in);
    if (in.failed()) {
        return false;
    }
    if (leash) {
        return true;
    }
    io::ByteBuffer out;
    if (mount < 0) {
        const auto previous = vehicles_.find(rider);
        if (previous == vehicles_.end()) {
            return false;
        }
        pv::varInt::write(out, previous->second);
        pv::varInt::write(out, 0);
        vehicles_.erase(previous);
    } else {
        vehicles_[rider] = mount;
        pv::varInt::write(out, mount);
        pv::varInt::write(out, 1);
        pv::varInt::write(out, rider);
    }
    packet.body = out.toVector();
    packet.overrideId(kPassengers19);
    return true;
}

bool Link47Up::entities(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kAddEntity:
    case kAddMob:
    case kAddPlayer:
    case kAddPainting:
    case kAddOrb:
    case kAddGlobal:
        return spawn(packet);
    case kEntityLink:
        return vehicle(packet);
    case kPlayerInfo:
        rememberPlayers(packet);
        return true;
    case kRemoveEntities: {
        const std::int32_t count = pv::readCollectionSize(in);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            forget(pv::varInt::read(in), packet);
        }
        return !in.failed();
    }
    case kEntityData: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        if (!entityData(in, out, entity, packet)) {
            return false;
        }
        break;
    }
    case kMovePos:
    case kMovePosRot:
        pv::varInt::write(out, pv::varInt::read(in));
        for (int axis = 0; axis < 3; ++axis) {
            out.writeShort(static_cast<std::int16_t>(static_cast<std::int8_t>(in.readByte()) * kMoveScale));
        }
        out.writeBytes(in.readable());
        break;
    case kTeleport:
        pv::varInt::write(out, pv::varInt::read(in));
        writeFixed(in, out, 3);
        out.writeBytes(in.readable());
        break;
    case kEquipment: {
        pv::varInt::write(out, pv::varInt::read(in));
        const std::int16_t slot = in.readShort();
        pv::varInt::write(out, slot == 0 ? 0 : slot + 1);
        copySlot(in, out);
        break;
    }
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
