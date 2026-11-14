#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/BlockEntityTypes.hpp>
#include <basalt/wire/multiversion/java/StackComponents1_21_4.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityTypeIds.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java::stackComponents1_21_4 {
namespace {

namespace pv = protocol;
namespace play = protocol::play;
namespace versions = protocolVersions;
using Buffer = io::ByteBuffer;
using play::ItemComponent;

// Native DataComponentType ids, by 26.3 registration order.
constexpr std::int32_t kUnbreakable = 4;
constexpr std::int32_t kCustomName = 6;
constexpr std::int32_t kItemName = 9;
constexpr std::int32_t kLore = 11;
constexpr std::int32_t kEnchantments = 13;
constexpr std::int32_t kAttributeModifiers = 16;
constexpr std::int32_t kCustomModelData = 17;
constexpr std::int32_t kTooltipDisplay = 18;
constexpr std::int32_t kFood = 23;
constexpr std::int32_t kUseRemainder = 25;
constexpr std::int32_t kDamageResistant = 27;
constexpr std::int32_t kTool = 28;
constexpr std::int32_t kEquippable = 32;
constexpr std::int32_t kStoredEnchantments = 45;
constexpr std::int32_t kDyedColor = 47;
constexpr std::int32_t kChargedProjectiles = 51;
constexpr std::int32_t kBundleContents = 52;
constexpr std::int32_t kPotionContents = 53;
constexpr std::int32_t kWrittenBook = 57;
constexpr std::int32_t kTrim = 58;
constexpr std::int32_t kEntityData = 60;
constexpr std::int32_t kBlockEntityData = 62;
constexpr std::int32_t kInstrument = 63;
constexpr std::int32_t kJukeboxPlayable = 66;
constexpr std::int32_t kProfile = 72;
constexpr std::int32_t kContainer = 77;
constexpr std::int32_t kBees = 79;

// Components whose payload 1.20.5 to 1.21.4 read exactly as this server writes it.
constexpr std::array<std::int32_t, 36> kSameShape = {0,  1,  2,  3,  10, 12, 19, 20, 21, 22, 24, 26,
                                                     31, 33, 34, 35, 36, 48, 49, 50, 55, 56, 59, 61,
                                                     65, 68, 69, 70, 71, 73, 74, 75, 76, 78, 81, 82};

constexpr std::int32_t kMaxStringBytes = 32767 * 4;

void copyVi(Buffer& in, Buffer& out) { pv::varInt::write(out, pv::varInt::read(in)); }

void copyString(Buffer& in, Buffer& out) { pv::writeString(out, pv::readString(in, kMaxStringBytes)); }

void copyBytes(Buffer& in, Buffer& out, std::size_t count) { out.writeBytes(in.readSpan(count)); }

void copyHolderSet(Buffer& in, Buffer& out) {
    const std::int32_t size = pv::varInt::read(in);
    pv::varInt::write(out, size);
    if (size == 0) {
        copyString(in, out);
        return;
    }
    for (std::int32_t i = 0; i < size - 1 && !in.failed(); ++i) {
        copyVi(in, out);
    }
}

void copySound(Buffer& in, Buffer& out) {
    const std::int32_t id = pv::varInt::read(in);
    pv::varInt::write(out, id);
    if (id == 0) {
        copyString(in, out);
        const bool range = pv::readBoolean(in);
        pv::writeBoolean(out, range);
        if (range) {
            copyBytes(in, out, 4);
        }
    }
}

void skipSound(Buffer& in) {
    Buffer ignored;
    copySound(in, ignored);
}

void copyText(Buffer& in, Buffer& out) {
    const text::Component value = text::componentNbt::deserialize(pv::readAnonymousNbt(in));
    const text::componentNbt::ScopedLegacyEvents legacy;
    pv::writeAnonymousNbt(out, text::componentNbt::serialize(value));
}

class Converter {
public:
    Converter(std::int32_t farEnd, const Nested& nested, std::vector<std::int32_t> hidden)
        : farEnd_(farEnd), nested_(&nested), hidden_(std::move(hidden)) {}

    /// The far end's payload, or nothing when it has no way to read this component.
    std::optional<std::vector<std::uint8_t>> convert(std::int32_t id, Buffer& in) {
        Buffer out;
        if (!convertInto(id, in, out) || in.failed() || in.readableBytes() != 0) {
            return std::nullopt;
        }
        return out.toVector();
    }

private:
    void shown(Buffer& out, std::int32_t id) const { pv::writeBoolean(out, !std::ranges::contains(hidden_, id)); }

    bool convertInto(std::int32_t id, Buffer& in, Buffer& out) {
        if (std::ranges::contains(kSameShape, id)) {
            out.writeBytes(in.readable());
            in.skip(in.readableBytes());
            return true;
        }
        switch (id) {
        case kCustomName:
        case kItemName:
            copyText(in, out);
            return true;
        case kLore:
            list(in, out, [](Buffer& from, Buffer& to) { copyText(from, to); });
            return true;
        case kWrittenBook:
            copyString(in, out);
            optional(in, out, copyString);
            copyString(in, out);
            copyVi(in, out);
            list(in, out, [](Buffer& from, Buffer& to) {
                copyText(from, to);
                optional(from, to, copyText);
            });
            copyBytes(in, out, 1);
            return true;
        case kUnbreakable:
            shown(out, id);
            return true;
        case kEnchantments:
        case kStoredEnchantments:
            list(in, out, [](Buffer& from, Buffer& to) {
                copyVi(from, to);
                copyVi(from, to);
            });
            shown(out, id);
            return true;
        case kDyedColor:
            copyBytes(in, out, 4);
            shown(out, id);
            return true;
        case kAttributeModifiers:
            return attributeModifiers(in, out);
        case kCustomModelData:
            return customModelData(in, out);
        case kFood:
            return food(in, out);
        case kTool:
            list(in, out, [](Buffer& from, Buffer& to) {
                copyHolderSet(from, to);
                optional(from, to, [](Buffer& f, Buffer& t) { copyBytes(f, t, 4); });
                optional(from, to, [](Buffer& f, Buffer& t) { copyBytes(f, t, 1); });
            });
            copyBytes(in, out, 4);
            copyVi(in, out);
            static_cast<void>(pv::readBoolean(in));
            return true;
        case kEquippable:
            return farEnd_ >= versions::kV1_21_2 && equippable(in, out);
        case kDamageResistant:
            return farEnd_ >= versions::kV1_21_2 && damageResistant(in, out);
        case kPotionContents:
            return potion(in, out);
        case kTrim:
            return reference(in, out) && reference(in, out) && (shown(out, id), true);
        case kInstrument:
            return reference(in, out);
        case kJukeboxPlayable:
            pv::writeBoolean(out, true);
            return reference(in, out) && (shown(out, id), true);
        case kProfile:
            return profile(in, out);
        case kEntityData:
        case kBlockEntityData:
            return typedData(id, in, out);
        case kBees:
            list(in, out, [](Buffer& from, Buffer& to) {
                static_cast<void>(pv::varInt::read(from));
                pv::writeAnonymousNbt(to, pv::readAnonymousNbt(from));
                copyVi(from, to);
                copyVi(from, to);
            });
            return true;
        case kUseRemainder:
            return farEnd_ >= versions::kV1_21_2 && stack(in, out, false);
        case kChargedProjectiles:
        case kBundleContents:
            return stacks(in, out, false);
        case kContainer:
            return stacks(in, out, true);
        default:
            return false;
        }
    }

    template <typename Copy>
    static void list(Buffer& in, Buffer& out, Copy&& copy) {
        const std::int32_t count = pv::readCollectionSize(in);
        pv::varInt::write(out, count);
        for (std::int32_t i = 0; i < count && !in.failed(); ++i) {
            copy(in, out);
        }
    }

    template <typename Copy>
    static void optional(Buffer& in, Buffer& out, Copy&& copy) {
        const bool present = pv::readBoolean(in);
        pv::writeBoolean(out, present);
        if (present && !in.failed()) {
            copy(in, out);
        }
    }

    /// A registry reference; an inline definition changed its fields too often to pass on.
    static bool reference(Buffer& in, Buffer& out) {
        const std::int32_t id = pv::varInt::read(in);
        pv::varInt::write(out, id);
        return id != 0;
    }

    bool attributeModifiers(Buffer& in, Buffer& out) const {
        // 1.21 named a modifier by id; 1.20.5 by uuid and name, which this server no longer has.
        if (farEnd_ < versions::kV1_21) {
            return false;
        }
        list(in, out, [](Buffer& from, Buffer& to) {
            copyVi(from, to);
            copyString(from, to);
            copyBytes(from, to, 8);
            copyVi(from, to);
            copyVi(from, to);
            if (pv::varInt::read(from) == 2) {
                static_cast<void>(pv::readAnonymousNbt(from));
            }
        });
        shown(out, kAttributeModifiers);
        return true;
    }

    bool customModelData(Buffer& in, Buffer& out) const {
        if (farEnd_ >= versions::kV1_21_4) {
            out.writeBytes(in.readable());
            in.skip(in.readableBytes());
            return true;
        }
        // Before 1.21.4 the model data was one number, which the first float now carries.
        const std::int32_t floats = pv::readCollectionSize(in);
        float first = 0.0F;
        for (std::int32_t i = 0; i < floats && !in.failed(); ++i) {
            const float value = in.readFloat();
            first = i == 0 ? value : first;
        }
        in.skip(in.readableBytes());
        pv::varInt::write(out, static_cast<std::int32_t>(first));
        return true;
    }

    bool food(Buffer& in, Buffer& out) const {
        copyVi(in, out);
        copyBytes(in, out, 4);
        copyBytes(in, out, 1);
        if (farEnd_ < versions::kV1_21_2) {
            // parity: FoodProperties.DEFAULT_EAT_SECONDS, before consumable took the eating over.
            out.writeFloat(1.6F);
            if (farEnd_ >= versions::kV1_21) {
                pv::writeBoolean(out, false);
            }
            pv::varInt::write(out, 0);
        }
        return true;
    }

    static bool equippable(Buffer& in, Buffer& out) {
        copyVi(in, out);
        copySound(in, out);
        optional(in, out, copyString);
        optional(in, out, copyString);
        optional(in, out, copyHolderSet);
        copyBytes(in, out, 3);
        in.skip(2);
        skipSound(in);
        return true;
    }

    static bool damageResistant(Buffer& in, Buffer& out) {
        // 1.21.4 resisted one damage type tag, which is what a holder set naming a tag is.
        if (pv::varInt::read(in) != 0) {
            return false;
        }
        std::string tag = pv::readString(in, kMaxStringBytes);
        pv::writeString(out, tag);
        return true;
    }

    bool potion(Buffer& in, Buffer& out) const {
        optional(in, out, copyVi);
        optional(in, out, [](Buffer& from, Buffer& to) { copyBytes(from, to, 4); });
        list(in, out, [](Buffer& from, Buffer& to) {
            copyVi(from, to);
            for (bool more = true; more && !from.failed();) {
                copyVi(from, to);
                copyVi(from, to);
                copyBytes(from, to, 3);
                more = pv::readBoolean(from);
                pv::writeBoolean(to, more);
            }
        });
        if (farEnd_ >= versions::kV1_21_2) {
            optional(in, out, copyString);
        } else if (pv::readBoolean(in)) {
            static_cast<void>(pv::readString(in, kMaxStringBytes));
        }
        return true;
    }

    static bool profile(Buffer& in, Buffer& out) {
        std::optional<std::string> name;
        std::optional<std::span<const std::uint8_t>> uuid;
        if (pv::readBoolean(in)) {
            uuid = in.readSpan(16);
            name = pv::readString(in, kMaxStringBytes);
        } else {
            if (pv::readBoolean(in)) {
                name = pv::readString(in, kMaxStringBytes);
            }
            if (pv::readBoolean(in)) {
                uuid = in.readSpan(16);
            }
        }
        pv::writeBoolean(out, name.has_value());
        if (name.has_value()) {
            pv::writeString(out, *name);
        }
        pv::writeBoolean(out, uuid.has_value());
        if (uuid.has_value()) {
            out.writeBytes(*uuid);
        }
        list(in, out, [](Buffer& from, Buffer& to) {
            copyString(from, to);
            copyString(from, to);
            optional(from, to, copyString);
        });
        in.skip(in.readableBytes());
        return true;
    }

    static bool typedData(std::int32_t id, Buffer& in, Buffer& out) {
        const std::int32_t type = pv::varInt::read(in);
        nbt::NbtTag tag = pv::readAnonymousNbt(in);
        auto* compound = tag.get<nbt::NbtCompound>();
        const std::string_view name =
            id == kEntityData ? play::javaEntityTypeName(type) : blockEntityTypes::nativeName(type);
        if (compound == nullptr || name.empty()) {
            return false;
        }
        // Before 1.21.5 the type sat inside the data as its id.
        compound->putString("id",
                            name.starts_with("minecraft:") ? std::string{name} : "minecraft:" + std::string{name});
        pv::writeAnonymousNbt(out, tag);
        return true;
    }

    /// A stack held in a component: this server writes it as an ItemStackTemplate, item before count.
    bool stack(Buffer& in, Buffer& out, bool optionalStack) {
        play::WireItemStack held;
        held.itemId = pv::varInt::read(in);
        held.count = pv::varInt::read(in);
        const std::int32_t added = pv::readCollectionSize(in, 2);
        const std::int32_t removed = pv::readCollectionSize(in);
        for (std::int32_t i = 0; i < added && !in.failed(); ++i) {
            const std::int32_t component = pv::varInt::read(in);
            auto read = ItemComponent::readPayload(in, component, std::nullopt);
            if (!read.has_value()) {
                in.markFailed();
                return false;
            }
            held.components.push_back(*std::move(read));
        }
        for (std::int32_t i = 0; i < removed && !in.failed(); ++i) {
            held.removedComponents.push_back(pv::varInt::read(in));
        }
        const std::optional<play::WireItemStack> older = (*nested_)(held);
        if (!older.has_value()) {
            if (optionalStack) {
                pv::varInt::write(out, 0);
            }
            return optionalStack;
        }
        play::WireItemStack::writeOptional(out, older);
        return true;
    }

    bool stacks(Buffer& in, Buffer& out, bool optionalStacks) {
        const std::int32_t count = pv::readCollectionSize(in);
        Buffer items;
        std::int32_t kept = 0;
        for (std::int32_t i = 0; i < count && !in.failed(); ++i) {
            if (optionalStacks && !pv::readBoolean(in)) {
                pv::varInt::write(items, 0);
                ++kept;
                continue;
            }
            if (stack(in, items, optionalStacks)) {
                ++kept;
            }
        }
        pv::varInt::write(out, kept);
        out.writeBytes(items.readable());
        return true;
    }

    std::int32_t farEnd_;
    const Nested* nested_;
    std::vector<std::int32_t> hidden_;
};

} // namespace

bool rewrite(play::WireItemStack& item, std::int32_t farEnd, const Nested& nested) {
    bool hideTooltip = false;
    std::vector<std::int32_t> hidden;
    for (const ItemComponent& component : item.components) {
        if (component.id() != kTooltipDisplay) {
            continue;
        }
        Buffer payload;
        component.writePayload(payload);
        hideTooltip = pv::readBoolean(payload);
        const std::int32_t count = pv::readCollectionSize(payload);
        for (std::int32_t i = 0; i < count && !payload.failed(); ++i) {
            hidden.push_back(pv::varInt::read(payload));
        }
    }
    Converter converter{farEnd, nested, std::move(hidden)};
    std::vector<ItemComponent> converted;
    converted.reserve(item.components.size());
    for (const ItemComponent& component : item.components) {
        Buffer payload;
        component.writePayload(payload);
        auto bytes = converter.convert(component.id(), payload);
        if (bytes.has_value()) {
            converted.emplace_back(component.id(), ItemComponent::Raw{*std::move(bytes)});
        }
    }
    item.components = std::move(converted);
    return hideTooltip;
}

void nestedStacks(play::WireItemStack& item, std::int32_t farEnd, const Nested& nested) {
    constexpr std::array<std::int32_t, 4> kHoldsStacks = {kUseRemainder, kChargedProjectiles, kBundleContents,
                                                          kContainer};
    Converter converter{farEnd, nested, {}};
    std::vector<ItemComponent> converted;
    converted.reserve(item.components.size());
    for (ItemComponent& component : item.components) {
        if (!std::ranges::contains(kHoldsStacks, component.id())) {
            converted.push_back(std::move(component));
            continue;
        }
        Buffer payload;
        component.writePayload(payload);
        auto bytes = converter.convert(component.id(), payload);
        if (bytes.has_value()) {
            converted.emplace_back(component.id(), ItemComponent::Raw{*std::move(bytes)});
        }
    }
    item.components = std::move(converted);
}

std::int32_t hideTooltipId(std::int32_t farEnd) noexcept { return farEnd >= versions::kV1_21_2 ? 16 : 15; }

} // namespace basalt::wire::multiversion::java::stackComponents1_21_4
