#include "ItemComponentSkip.hpp"

#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <array>
#include <cstddef>

namespace basalt::wire::protocol::play::componentSkip {
namespace {

using Buffer = io::ByteBuffer;
using Skipper = void (*)(Buffer&, int);

constexpr int kMaxDepth = 64;
constexpr std::int32_t kMaxStringBytes = kDefaultMaxStringLength * 3;

void component(Buffer& buffer, std::int32_t id, int depth);

void none(Buffer&, int) {}

void vi(Buffer& buffer, int) { static_cast<void>(varInt::read(buffer)); }

void bool1(Buffer& buffer, int) { buffer.skip(1); }

void fixed4(Buffer& buffer, int) { buffer.skip(4); }

void fixed8(Buffer& buffer, int) { buffer.skip(8); }

void uuid(Buffer& buffer, int) { buffer.skip(16); }

void str(Buffer& buffer, int) {
    const std::int32_t length = varInt::read(buffer);
    if (length < 0 || length > kMaxStringBytes) {
        buffer.markFailed();
        return;
    }
    buffer.skip(static_cast<std::size_t>(length));
}

void nbt(Buffer& buffer, int) { static_cast<void>(readAnonymousNbt(buffer)); }

template <Skipper... Parts>
void seq(Buffer& buffer, int depth) {
    (Parts(buffer, depth), ...);
}

template <Skipper Inner>
void opt(Buffer& buffer, int depth) {
    if (readBoolean(buffer) && !buffer.failed()) {
        Inner(buffer, depth);
    }
}

template <Skipper Left, Skipper Right>
void either(Buffer& buffer, int depth) {
    const bool left = readBoolean(buffer);
    if (buffer.failed()) {
        return;
    }
    if (left) {
        Left(buffer, depth);
    } else {
        Right(buffer, depth);
    }
}

template <Skipper Inner>
void list(Buffer& buffer, int depth) {
    const std::int32_t count = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        Inner(buffer, depth);
    }
}

template <Skipper Direct>
void holder(Buffer& buffer, int depth) {
    if (varInt::read(buffer) == 0 && !buffer.failed()) {
        Direct(buffer, depth);
    }
}

void holderSet(Buffer& buffer, int depth) {
    const std::int32_t size = varInt::read(buffer);
    if (buffer.failed()) {
        return;
    }
    if (size == 0) {
        str(buffer, depth);
        return;
    }
    if (size < 0 || static_cast<std::size_t>(size - 1) > buffer.readableBytes()) {
        buffer.markFailed();
        return;
    }
    for (std::int32_t i = 0; i < size - 1 && !buffer.failed(); ++i) {
        vi(buffer, depth);
    }
}

void patch(Buffer& buffer, int depth) {
    const std::int32_t added = readCollectionSize(buffer, 2);
    const std::int32_t removed = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
        const std::int32_t id = varInt::read(buffer);
        if (!buffer.failed()) {
            component(buffer, id, depth);
        }
    }
    for (std::int32_t i = 0; i < removed && !buffer.failed(); ++i) {
        vi(buffer, depth);
    }
}

// parity: ItemStackTemplate.STREAM_CODEC, item and count with no empty marker.
void stackTemplate(Buffer& buffer, int depth) {
    if (depth >= kMaxDepth) {
        buffer.markFailed();
        return;
    }
    vi(buffer, depth);
    vi(buffer, depth);
    patch(buffer, depth + 1);
}

void typedComponent(Buffer& buffer, int depth) {
    const std::int32_t id = varInt::read(buffer);
    if (!buffer.failed()) {
        component(buffer, id, depth + 1);
    }
}

constexpr Skipper kSound = holder<seq<str, opt<fixed4>>>;

void effectDetails(Buffer& buffer, int depth) {
    seq<vi, vi, bool1, bool1, bool1>(buffer, depth);
    if (readBoolean(buffer) && !buffer.failed()) {
        if (depth >= kMaxDepth) {
            buffer.markFailed();
            return;
        }
        effectDetails(buffer, depth + 1);
    }
}

constexpr Skipper kMobEffect = seq<vi, effectDetails>;

void consumeEffect(Buffer& buffer, int depth) {
    switch (varInt::read(buffer)) {
    case 0:
        seq<list<kMobEffect>, fixed4>(buffer, depth);
        break;
    case 1:
        holderSet(buffer, depth);
        break;
    case 2:
        break;
    case 3:
        seq<fixed4, bool1>(buffer, depth);
        break;
    case 4:
        kSound(buffer, depth);
        break;
    default:
        buffer.markFailed();
        break;
    }
}

constexpr Skipper kPropertyMatcher = seq<str, either<str, seq<opt<str>, opt<str>>>>;
constexpr Skipper kPredicateSingle = seq<bool1, vi, nbt>;
constexpr Skipper kBlockPredicate =
    seq<opt<holderSet>, opt<list<kPropertyMatcher>>, opt<nbt>, list<typedComponent>, list<kPredicateSingle>>;

void attributeDisplay(Buffer& buffer, int depth) {
    // parity: ItemAttributeModifiers.Display.Type maps unknown ids to the empty default.
    if (varInt::read(buffer) == 2 && !buffer.failed()) {
        nbt(buffer, depth);
    }
}

constexpr Skipper kAttributeEntry = seq<vi, str, fixed8, vi, vi, attributeDisplay>;
constexpr Skipper kToolRule = seq<holderSet, opt<fixed4>, opt<bool1>>;
constexpr Skipper kDamageReduction = seq<fixed4, opt<holderSet>, fixed4, fixed4>;
constexpr Skipper kKineticCondition = seq<vi, fixed4, fixed4>;
constexpr Skipper kProperties = list<seq<str, str, opt<str>>>;
constexpr Skipper kProfile = seq<either<seq<uuid, str, kProperties>, seq<opt<str>, opt<uuid>, kProperties>>, opt<str>,
                                 opt<str>, opt<str>, opt<bool1>>;
constexpr Skipper kFireworkExplosion = seq<vi, list<fixed4>, list<fixed4>, bool1, bool1>;
constexpr Skipper kTrimMaterial = holder<seq<str, nbt>>;
constexpr Skipper kTrimPattern = holder<seq<str, nbt, bool1>>;
constexpr Skipper kResolvable = either<fixed4, str>;
constexpr Skipper kSignText = seq<nbt, nbt, nbt, nbt, opt<seq<nbt, nbt, nbt, nbt>>, vi, bool1>;

// Indexed by the 26.3 DataComponents registration order; persistent-only types travel as their NBT codec.
constexpr std::array<Skipper, 122> kSkippers = {
    nbt,                                                     // custom_data
    vi,                                                      // max_stack_size
    vi,                                                      // max_damage
    vi,                                                      // damage
    none,                                                    // unbreakable
    seq<bool1, bool1, fixed4>,                               // use_effects
    nbt,                                                     // custom_name
    fixed4,                                                  // minimum_attack_charge
    vi,                                                      // damage_type
    nbt,                                                     // item_name
    str,                                                     // item_model
    list<nbt>,                                               // lore
    vi,                                                      // rarity
    list<seq<vi, vi>>,                                       // enchantments
    list<kBlockPredicate>,                                   // can_place_on
    list<kBlockPredicate>,                                   // can_break
    list<kAttributeEntry>,                                   // attribute_modifiers
    seq<list<fixed4>, list<bool1>, list<str>, list<fixed4>>, // custom_model_data
    seq<bool1, list<vi>>,                                    // tooltip_display
    vi,                                                      // repair_cost
    none,                                                    // creative_slot_lock
    bool1,                                                   // enchantment_glint_override
    nbt,                                                     // intangible_projectile
    seq<vi, fixed4, bool1>,                                  // food
    seq<fixed4, vi, kSound, bool1, list<consumeEffect>>,     // consumable
    stackTemplate,                                           // use_remainder
    seq<fixed4, opt<str>>,                                   // use_cooldown
    holderSet,                                               // damage_resistant
    seq<list<kToolRule>, fixed4, vi, bool1>,                 // tool
    seq<vi, fixed4>,                                         // weapon
    seq<fixed4, fixed4, fixed4, fixed4, fixed4, fixed4>,     // attack_range
    vi,                                                      // enchantable
    seq<vi, kSound, opt<str>, opt<str>, opt<holderSet>, bool1, bool1, bool1, bool1, bool1, kSound>, // equippable
    holderSet,                                                                                      // repairable
    none,                                                                                           // glider
    str,                                                                                            // tooltip_style
    list<consumeEffect>,                                                                            // death_protection
    seq<fixed4, fixed4, list<kDamageReduction>, seq<fixed4, fixed4, fixed4>, opt<holderSet>, opt<kSound>,
        opt<kSound>>,                            // blocks_attacks
    seq<bool1, bool1, opt<kSound>, opt<kSound>>, // piercing_weapon
    seq<vi, vi, opt<kKineticCondition>, opt<kKineticCondition>, opt<kKineticCondition>, fixed4, fixed4, opt<kSound>,
        opt<kSound>>,                                                                    // kinetic_weapon
    seq<vi, vi>,                                                                         // attack_animation
    seq<vi, vi>,                                                                         // interact_animation
    vi,                                                                                  // additional_trade_cost
    vi,                                                                                  // block_transformer
    vi,                                                                                  // villager_food
    list<seq<vi, vi>>,                                                                   // stored_enchantments
    vi,                                                                                  // dye
    fixed4,                                                                              // dyed_color
    vi,                                                                                  // map_id
    nbt,                                                                                 // map_decorations
    vi,                                                                                  // map_post_processing
    list<stackTemplate>,                                                                 // charged_projectiles
    list<stackTemplate>,                                                                 // bundle_contents
    seq<opt<vi>, opt<fixed4>, list<kMobEffect>, opt<str>>,                               // potion_contents
    fixed4,                                                                              // potion_duration_scale
    list<seq<vi, vi>>,                                                                   // suspicious_stew_effects
    list<seq<str, opt<str>>>,                                                            // writable_book_content
    seq<str, opt<str>, str, vi, list<seq<nbt, opt<nbt>>>, bool1>,                        // written_book_content
    seq<kTrimMaterial, kTrimPattern>,                                                    // trim
    nbt,                                                                                 // debug_stick_state
    seq<vi, nbt>,                                                                        // entity_data
    nbt,                                                                                 // bucket_entity_data
    seq<vi, nbt>,                                                                        // block_entity_data
    holder<seq<kSound, fixed4, fixed4, vi, nbt>>,                                        // instrument
    kTrimMaterial,                                                                       // provides_trim_material
    vi,                                                                                  // ominous_bottle_amplifier
    holder<seq<kSound, nbt, fixed4, vi>>,                                                // jukebox_playable
    holderSet,                                                                           // provides_banner_patterns
    nbt,                                                                                 // recipes
    seq<opt<seq<str, fixed8>>, bool1>,                                                   // lodestone_tracker
    kFireworkExplosion,                                                                  // firework_explosion
    seq<vi, list<kFireworkExplosion>>,                                                   // fireworks
    kProfile,                                                                            // profile
    str,                                                                                 // note_block_sound
    list<seq<holder<seq<str, str>>, vi>>,                                                // banner_patterns
    vi,                                                                                  // base_color
    seq<opt<stackTemplate>, opt<stackTemplate>, opt<stackTemplate>, opt<stackTemplate>>, // pot_decorations
    list<opt<stackTemplate>>,                                                            // container
    list<seq<str, str>>,                                                                 // block_state
    list<seq<vi, nbt, vi, vi>>,                                                          // bees
    stackTemplate,                                                                       // sulfur_cube_content
    nbt,                                                                                 // lock
    nbt,                                                                                 // container_loot
    kSound,                                                                              // break_sound
    kResolvable,                                                                         // compostable
    seq<kResolvable, kResolvable>,                                                       // cooking_fuel
    seq<kResolvable, kResolvable>,                                                       // brewing_fuel
    seq<holderSet, fixed4>,                                                              // mob_visibility
    vi,                                                                                  // villager/variant
    vi,                                                                                  // wolf/variant
    vi,                                                                                  // wolf/sound_variant
    vi,                                                                                  // wolf/collar
    vi,                                                                                  // fox/variant
    vi,                                                                                  // salmon/size
    vi,                                                                                  // parrot/variant
    vi,                                                                                  // tropical_fish/pattern
    vi,                                                                                  // tropical_fish/base_color
    vi,                                                                                  // tropical_fish/pattern_color
    vi,                                                                                  // mooshroom/variant
    vi,                                                                                  // rabbit/variant
    vi,                                                                                  // pig/variant
    vi,                                                                                  // pig/sound_variant
    vi,                                                                                  // cow/variant
    vi,                                                                                  // cow/sound_variant
    vi,                                                                                  // chicken/variant
    vi,                                                                                  // chicken/sound_variant
    vi,                                                                                  // zombie_nautilus/variant
    vi,                                                                                  // frog/variant
    vi,                                                                                  // horse/variant
    holder<seq<vi, vi, str, opt<nbt>, opt<nbt>>>,                                        // painting/variant
    vi,                                                                                  // llama/variant
    vi,                                                                                  // axolotl/variant
    vi,                                                                                  // cat/variant
    vi,                                                                                  // cat/sound_variant
    vi,                                                                                  // cat/collar
    vi,                                                                                  // sheep/color
    vi,                                                                                  // shulker/color
    vi,                                                                                  // provides_pottery_pattern
    kSignText,                                                                           // sign_text_front
    kSignText,                                                                           // sign_text_back
    none,                                                                                // waxed
    vi,                                                                                  // cushion/color
};

void component(Buffer& buffer, std::int32_t id, int depth) {
    if (id < 0 || static_cast<std::size_t>(id) >= kSkippers.size() || depth >= kMaxDepth) {
        buffer.markFailed();
        return;
    }
    kSkippers[static_cast<std::size_t>(id)](buffer, depth);
}

} // namespace

bool skipPayload(io::ByteBuffer& buffer, std::int32_t id) {
    component(buffer, id, 0);
    return !buffer.failed();
}

} // namespace basalt::wire::protocol::play::componentSkip
