#include <basalt/wire/multiversion/java/Stack769.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java::stack769 {
namespace {

namespace pv = protocol;
using Buffer = io::ByteBuffer;
using Copier = void (*)(Buffer&, Buffer&);

constexpr std::int32_t kMaxDepth = 8;
constexpr std::int32_t kNoComponent = -1;

void vi(Buffer& in, Buffer& out) { pv::varInt::write(out, pv::varInt::read(in)); }

void byte1(Buffer& in, Buffer& out) { out.writeByte(in.readByte()); }

void int4(Buffer& in, Buffer& out) { out.writeInt(in.readInt()); }

void float4(Buffer& in, Buffer& out) { out.writeFloat(in.readFloat()); }

void double8(Buffer& in, Buffer& out) { out.writeDouble(in.readDouble()); }

void str(Buffer& in, Buffer& out) { pv::writeString(out, pv::readString(in, 32767 * 4)); }

void uuid16(Buffer& in, Buffer& out) { out.writeBytes(in.readSpan(16)); }

void nothing(Buffer&, Buffer&) {}

void nbtTag(Buffer& in, Buffer& out) { pv::writeAnonymousNbt(out, pv::readAnonymousNbt(in)); }

void text(Buffer& in, Buffer& out) {
    pv::writeAnonymousNbt(
        out, wire::text::componentNbt::serialize(wire::text::componentNbt::deserialize(pv::readAnonymousNbt(in))));
}

template <Copier Inner>
void opt(Buffer& in, Buffer& out) {
    const bool present = pv::readBoolean(in);
    pv::writeBoolean(out, present);
    if (present && !in.failed()) {
        Inner(in, out);
    }
}

template <Copier Inner>
void list(Buffer& in, Buffer& out) {
    const std::int32_t count = pv::readCollectionSize(in);
    pv::varInt::write(out, count);
    for (std::int32_t i = 0; i < count && !in.failed(); ++i) {
        Inner(in, out);
    }
}

template <Copier A, Copier B>
void pair(Buffer& in, Buffer& out) {
    A(in, out);
    B(in, out);
}

template <Copier A, Copier B, Copier C>
void triple(Buffer& in, Buffer& out) {
    A(in, out);
    B(in, out);
    C(in, out);
}

void holderSet(Buffer& in, Buffer& out) {
    const std::int32_t size = pv::varInt::read(in);
    pv::varInt::write(out, size);
    if (size == 0) {
        str(in, out);
        return;
    }
    for (std::int32_t i = 0; i < size - 1 && !in.failed(); ++i) {
        vi(in, out);
    }
}

template <Copier Direct>
void holder(Buffer& in, Buffer& out) {
    const std::int32_t id = pv::varInt::read(in);
    pv::varInt::write(out, id);
    if (id == 0) {
        Direct(in, out);
    }
}

void soundDirect(Buffer& in, Buffer& out) {
    str(in, out);
    opt<float4>(in, out);
}

void effectDetails(Buffer& in, Buffer& out) {
    vi(in, out);
    vi(in, out);
    byte1(in, out);
    byte1(in, out);
    byte1(in, out);
    if (pv::readBoolean(in)) {
        pv::writeBoolean(out, true);
        effectDetails(in, out);
    } else {
        pv::writeBoolean(out, false);
    }
}

void mobEffect(Buffer& in, Buffer& out) {
    vi(in, out);
    effectDetails(in, out);
}

void consumeEffect(Buffer& in, Buffer& out) {
    const std::int32_t type = pv::varInt::read(in);
    pv::varInt::write(out, type);
    switch (type) {
    case 0:
        list<mobEffect>(in, out);
        float4(in, out);
        break;
    case 1:
        holderSet(in, out);
        break;
    case 2:
        break;
    case 3:
        float4(in, out);
        break;
    case 4:
        holder<soundDirect>(in, out);
        break;
    default:
        in.markFailed();
        break;
    }
}

void stackCopy(Buffer& in, Buffer& out, std::int32_t depth);

void nestedStack(Buffer& in, Buffer& out) { stackCopy(in, out, kMaxDepth); }

void blockPredicate(Buffer& in, Buffer& out) {
    opt<holderSet>(in, out);
    if (pv::readBoolean(in)) {
        pv::writeBoolean(out, true);
        const std::int32_t count = pv::readCollectionSize(in);
        pv::varInt::write(out, count);
        for (std::int32_t i = 0; i < count && !in.failed(); ++i) {
            str(in, out);
            if (pv::readBoolean(in)) {
                pv::writeBoolean(out, true);
                str(in, out);
            } else {
                pv::writeBoolean(out, false);
                opt<str>(in, out);
                opt<str>(in, out);
            }
        }
    } else {
        pv::writeBoolean(out, false);
    }
    opt<nbtTag>(in, out);
}

void blockPredicateWithMatchers(Buffer& in, Buffer& out) {
    blockPredicate(in, out);
    pv::varInt::write(out, 0);
    pv::varInt::write(out, 0);
}

void adventurePredicate(Buffer& in, Buffer& out) {
    list<blockPredicateWithMatchers>(in, out);
    static_cast<void>(pv::readBoolean(in));
}

void enchantmentsNoTooltip(Buffer& in, Buffer& out) {
    list<pair<vi, vi>>(in, out);
    static_cast<void>(pv::readBoolean(in));
}

void unbreakable(Buffer& in, Buffer&) { static_cast<void>(pv::readBoolean(in)); }

void dyedColor(Buffer& in, Buffer& out) {
    int4(in, out);
    static_cast<void>(pv::readBoolean(in));
}

void attributeModifier(Buffer& in, Buffer& out) {
    vi(in, out);
    str(in, out);
    double8(in, out);
    vi(in, out);
    vi(in, out);
}

void attributeModifiers(Buffer& in, Buffer& out) {
    list<attributeModifier>(in, out);
    static_cast<void>(pv::readBoolean(in));
}

void customModelData(Buffer& in, Buffer& out) {
    list<float4>(in, out);
    list<byte1>(in, out);
    list<str>(in, out);
    list<int4>(in, out);
}

void toolRule(Buffer& in, Buffer& out) {
    holderSet(in, out);
    opt<float4>(in, out);
    opt<byte1>(in, out);
}

void tool(Buffer& in, Buffer& out) {
    list<toolRule>(in, out);
    float4(in, out);
    vi(in, out);
    out.writeByte(1);
}

void consumable(Buffer& in, Buffer& out) {
    float4(in, out);
    vi(in, out);
    holder<soundDirect>(in, out);
    byte1(in, out);
    list<consumeEffect>(in, out);
}

void equippable(Buffer& in, Buffer& out) {
    vi(in, out);
    holder<soundDirect>(in, out);
    opt<str>(in, out);
    opt<str>(in, out);
    opt<holderSet>(in, out);
    byte1(in, out);
    byte1(in, out);
    byte1(in, out);
    out.writeByte(0);
}

void food(Buffer& in, Buffer& out) {
    vi(in, out);
    float4(in, out);
    byte1(in, out);
}

void useCooldown(Buffer& in, Buffer& out) {
    float4(in, out);
    opt<str>(in, out);
}

void potionContents(Buffer& in, Buffer& out) {
    opt<vi>(in, out);
    opt<int4>(in, out);
    list<mobEffect>(in, out);
    opt<str>(in, out);
}

void filterableString(Buffer& in, Buffer& out) {
    str(in, out);
    opt<str>(in, out);
}

void filterableText(Buffer& in, Buffer& out) {
    text(in, out);
    opt<text>(in, out);
}

void writtenBook(Buffer& in, Buffer& out) {
    filterableString(in, out);
    str(in, out);
    vi(in, out);
    list<filterableText>(in, out);
    byte1(in, out);
}

void instrumentDirect(Buffer& in, Buffer& out) {
    holder<soundDirect>(in, out);
    float4(in, out);
    float4(in, out);
    nbtTag(in, out);
}

void instrument(Buffer& in, Buffer& out) {
    pv::writeBoolean(out, true);
    holder<instrumentDirect>(in, out);
}

void instrumentDirectOlder(Buffer& in, Buffer& out) {
    holder<soundDirect>(in, out);
    vi(in, out);
    float4(in, out);
}

void instrumentOlder(Buffer& in, Buffer& out) { holder<instrumentDirectOlder>(in, out); }

void jukeboxSong(Buffer& in, Buffer& out) {
    holder<soundDirect>(in, out);
    nbtTag(in, out);
    float4(in, out);
    vi(in, out);
}

void jukeboxPlayable(Buffer& in, Buffer& out) {
    const bool direct = pv::readBoolean(in);
    pv::writeBoolean(out, direct);
    if (direct) {
        holder<jukeboxSong>(in, out);
    } else {
        str(in, out);
    }
    static_cast<void>(pv::readBoolean(in));
}

void lodestone(Buffer& in, Buffer& out) {
    if (pv::readBoolean(in)) {
        pv::writeBoolean(out, true);
        str(in, out);
        out.writeLong(in.readLong());
    } else {
        pv::writeBoolean(out, false);
    }
    byte1(in, out);
}

void fireworkExplosion(Buffer& in, Buffer& out) {
    vi(in, out);
    list<int4>(in, out);
    list<int4>(in, out);
    byte1(in, out);
    byte1(in, out);
}

void fireworks(Buffer& in, Buffer& out) {
    vi(in, out);
    list<fireworkExplosion>(in, out);
}

void profileProperty(Buffer& in, Buffer& out) {
    str(in, out);
    str(in, out);
    opt<str>(in, out);
}

void profile(Buffer& in, Buffer& out) {
    opt<str>(in, out);
    opt<uuid16>(in, out);
    list<profileProperty>(in, out);
}

void bannerPatternDirect(Buffer& in, Buffer& out) {
    str(in, out);
    str(in, out);
}

void bannerLayer(Buffer& in, Buffer& out) {
    holder<bannerPatternDirect>(in, out);
    vi(in, out);
}

void bee(Buffer& in, Buffer& out) {
    nbtTag(in, out);
    vi(in, out);
    vi(in, out);
}

void stewEffect(Buffer& in, Buffer& out) {
    vi(in, out);
    vi(in, out);
}

void blockStateProperty(Buffer& in, Buffer& out) {
    str(in, out);
    str(in, out);
}

void trimHolder(Buffer& in, Buffer& out) {
    const std::int32_t id = pv::varInt::read(in);
    if (id == 0) {
        in.markFailed();
        return;
    }
    pv::varInt::write(out, id);
}

void trim(Buffer& in, Buffer& out) {
    trimHolder(in, out);
    trimHolder(in, out);
    static_cast<void>(pv::readBoolean(in));
}

thread_local Source gSource = Source::k1_21_4;

void intModelData(Buffer& in, Buffer&) { static_cast<void>(pv::varInt::read(in)); }

void foodOlder(Buffer& in, Buffer& out) {
    vi(in, out);
    float4(in, out);
    byte1(in, out);
    float4(in, out);
    nestedStack(in, out);
    list<pair<mobEffect, float4>>(in, out);
}

void potionContentsOlder(Buffer& in, Buffer& out) {
    opt<vi>(in, out);
    opt<int4>(in, out);
    list<mobEffect>(in, out);
}

struct Entry {
    std::int32_t nativeId;
    Copier copy;
};

// Indexed by the 1.21.4 data component id; nativeId is this build's, kNoComponent for a component it cannot carry.
constexpr std::array<Entry, 67> kEntries = {{
    {0, nbtTag},
    {1, vi},
    {2, vi},
    {3, vi},
    {4, unbreakable},
    {6, text},
    {9, text},
    {kNoComponent, str},
    {11, list<text>},
    {12, vi},
    {13, enchantmentsNoTooltip},
    {kNoComponent, adventurePredicate},
    {kNoComponent, adventurePredicate},
    {kNoComponent, attributeModifiers},
    {kNoComponent, customModelData},
    {kNoComponent, nothing},
    {kNoComponent, nothing},
    {19, vi},
    {20, nothing},
    {21, byte1},
    {kNoComponent, nbtTag},
    {kNoComponent, food},
    {kNoComponent, consumable},
    {kNoComponent, nestedStack},
    {kNoComponent, useCooldown},
    {kNoComponent, str},
    {kNoComponent, tool},
    {kNoComponent, vi},
    {kNoComponent, equippable},
    {kNoComponent, holderSet},
    {kNoComponent, nothing},
    {kNoComponent, str},
    {kNoComponent, list<consumeEffect>},
    {45, enchantmentsNoTooltip},
    {47, dyedColor},
    {kNoComponent, int4},
    {48, vi},
    {kNoComponent, nbtTag},
    {kNoComponent, vi},
    {kNoComponent, list<nestedStack>},
    {kNoComponent, list<nestedStack>},
    {kNoComponent, potionContents},
    {kNoComponent, list<stewEffect>},
    {kNoComponent, list<filterableString>},
    {kNoComponent, writtenBook},
    {kNoComponent, trim},
    {kNoComponent, nbtTag},
    {kNoComponent, nbtTag},
    {kNoComponent, nbtTag},
    {kNoComponent, nbtTag},
    {kNoComponent, instrument},
    {kNoComponent, vi},
    {kNoComponent, jukeboxPlayable},
    {kNoComponent, nbtTag},
    {kNoComponent, lodestone},
    {kNoComponent, fireworkExplosion},
    {kNoComponent, fireworks},
    {kNoComponent, profile},
    {kNoComponent, str},
    {kNoComponent, list<bannerLayer>},
    {kNoComponent, vi},
    {kNoComponent, list<vi>},
    {kNoComponent, list<nestedStack>},
    {kNoComponent, list<blockStateProperty>},
    {kNoComponent, list<bee>},
    {kNoComponent, nbtTag},
    {kNoComponent, nbtTag},
}};

constexpr std::array<std::int8_t, 57> kFrom1_21 = {
    0,  1,  2,  3,  4,  5,  6,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, -1, 26, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66};

constexpr std::size_t kFood = 21;
constexpr std::size_t kPotionContents = 41;
constexpr std::size_t kInstrument = 50;
constexpr std::size_t kCustomModelDataIndex = 14;

bool lookup(std::int32_t id, Entry& entry) {
    if (id < 0) {
        return false;
    }
    std::size_t index = static_cast<std::size_t>(id);
    if (gSource == Source::k1_21) {
        if (index >= kFrom1_21.size()) {
            return false;
        }
        if (kFrom1_21[index] < 0) {
            entry = Entry{kNoComponent, nothing};
            return true;
        }
        index = static_cast<std::size_t>(kFrom1_21[index]);
    }
    if (index >= kEntries.size()) {
        return false;
    }
    entry = kEntries[index];
    if (gSource != Source::k1_21_4) {
        if (index == kCustomModelDataIndex) {
            entry = Entry{kNoComponent, intModelData};
        }
    }
    if (gSource == Source::k1_21) {
        if (index == kFood) {
            entry = Entry{kNoComponent, foodOlder};
        } else if (index == kPotionContents) {
            entry = Entry{kNoComponent, potionContentsOlder};
        } else if (index == kInstrument) {
            entry = Entry{kNoComponent, instrumentOlder};
        }
    }
    return true;
}

std::int32_t idInSource(std::int32_t id) {
    if (gSource != Source::k1_21) {
        return id;
    }
    for (std::size_t i = 0; i < kFrom1_21.size(); ++i) {
        if (kFrom1_21[i] == id) {
            return static_cast<std::int32_t>(i);
        }
    }
    return kNoComponent;
}

void stackCopy(Buffer& in, Buffer& out, std::int32_t depth) {
    const std::int32_t count = pv::varInt::read(in);
    if (in.failed() || depth <= 0) {
        in.markFailed();
        return;
    }
    if (count <= 0) {
        pv::varInt::write(out, 0);
        return;
    }
    pv::varInt::write(out, count);
    vi(in, out);
    const std::int32_t added = pv::readCollectionSize(in, 2);
    const std::int32_t removed = pv::readCollectionSize(in);
    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> kept;
    for (std::int32_t i = 0; i < added && !in.failed(); ++i) {
        const std::int32_t id = pv::varInt::read(in);
        Entry entry{};
        if (!lookup(id, entry)) {
            in.markFailed();
            return;
        }
        Buffer payload;
        entry.copy(in, payload);
        if (entry.nativeId != kNoComponent) {
            kept.emplace_back(entry.nativeId, payload.toVector());
        }
    }
    std::vector<std::int32_t> gone;
    for (std::int32_t i = 0; i < removed && !in.failed(); ++i) {
        const std::int32_t id = pv::varInt::read(in);
        Entry entry{};
        if (lookup(id, entry) && entry.nativeId != kNoComponent) {
            gone.push_back(entry.nativeId);
        }
    }
    pv::varInt::write(out, static_cast<std::int32_t>(kept.size()));
    pv::varInt::write(out, static_cast<std::int32_t>(gone.size()));
    for (const auto& [id, payload] : kept) {
        pv::varInt::write(out, id);
        out.writeBytes(payload);
    }
    for (const std::int32_t id : gone) {
        pv::varInt::write(out, id);
    }
}

} // namespace

SourceScope::SourceScope(Source source) : previous_(gSource) { gSource = source; }

SourceScope::~SourceScope() { gSource = previous_; }

bool convert(Buffer& in, Buffer& out) {
    stackCopy(in, out, kMaxDepth);
    return !in.failed();
}

bool convertToServer(Buffer& in, Buffer& out) {
    const std::int32_t count = pv::varInt::read(in);
    if (in.failed()) {
        return false;
    }
    if (count <= 0) {
        pv::varInt::write(out, 0);
        return true;
    }
    pv::varInt::write(out, count);
    vi(in, out);
    const std::int32_t added = pv::readCollectionSize(in, 2);
    const std::int32_t removed = pv::readCollectionSize(in);
    static_cast<void>(removed);
    // Stops at the first component it cannot read: the proxy stored the rest without lengths.
    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> kept;
    bool lost = false;
    for (std::int32_t i = 0; i < added && !in.failed() && !lost; ++i) {
        const std::int32_t id = pv::varInt::read(in);
        Buffer payload;
        std::int32_t serverId = kNoComponent;
        switch (id) {
        case 0:
            nbtTag(in, payload);
            serverId = 0;
            break;
        case 1:
            vi(in, payload);
            serverId = 1;
            break;
        case 2:
            vi(in, payload);
            serverId = 2;
            break;
        case 3:
            vi(in, payload);
            serverId = 3;
            break;
        case 4:
            payload.writeByte(1);
            serverId = 4;
            break;
        case 6:
            text(in, payload);
            serverId = 5;
            break;
        case 9:
            text(in, payload);
            serverId = 6;
            break;
        case 11:
            list<text>(in, payload);
            serverId = 8;
            break;
        case 12:
            vi(in, payload);
            serverId = 9;
            break;
        case 13:
            list<pair<vi, vi>>(in, payload);
            payload.writeByte(1);
            serverId = 10;
            break;
        case 19:
            vi(in, payload);
            serverId = 17;
            break;
        case 20:
            serverId = 18;
            break;
        case 21:
            byte1(in, payload);
            serverId = 19;
            break;
        case 45:
            list<pair<vi, vi>>(in, payload);
            payload.writeByte(1);
            serverId = 33;
            break;
        case 47:
            int4(in, payload);
            payload.writeByte(1);
            serverId = 34;
            break;
        case 48:
            vi(in, payload);
            serverId = 36;
            break;
        default:
            lost = true;
            break;
        }
        if (!lost) {
            const std::int32_t sourceId = idInSource(serverId);
            if (sourceId != kNoComponent) {
                kept.emplace_back(sourceId, payload.toVector());
            }
        }
    }
    if (in.failed()) {
        return false;
    }
    pv::varInt::write(out, static_cast<std::int32_t>(kept.size()));
    pv::varInt::write(out, 0);
    for (const auto& [id, payload] : kept) {
        pv::varInt::write(out, id);
        out.writeBytes(payload);
    }
    in.skip(in.readableBytes());
    return true;
}

} // namespace basalt::wire::multiversion::java::stack769
