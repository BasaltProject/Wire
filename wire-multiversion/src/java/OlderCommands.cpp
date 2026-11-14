#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/OlderCommands.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace basalt::wire::multiversion::java::olderCommands {
namespace {

namespace play = protocol::play;
namespace type = play::argumentType;
namespace versions = protocolVersions;
using Parser = play::ArgumentParser;

// The three parsers 1.19.3 folded into the registry ones, numbered past this server's own.
constexpr std::int32_t kMobEffect = type::kCount;
constexpr std::int32_t kItemEnchantment = type::kCount + 1;
constexpr std::int32_t kEntitySummon = type::kCount + 2;

// parity: ArgumentTypeInfos, in 26.3 registration order.
constexpr std::array<std::string_view, type::kCount + 3> kNames = {
    "brigadier:bool",
    "brigadier:float",
    "brigadier:double",
    "brigadier:integer",
    "brigadier:long",
    "brigadier:string",
    "entity",
    "game_profile",
    "block_pos",
    "column_pos",
    "vec3",
    "vec2",
    "block_state",
    "block_predicate",
    "item_stack",
    "item_predicate",
    "color",
    "hex_color",
    "component",
    "style",
    "message",
    "nbt_compound_tag",
    "nbt_tag",
    "nbt_path",
    "objective",
    "objective_criteria",
    "operation",
    "particle",
    "angle",
    "rotation",
    "scoreboard_slot",
    "score_holder",
    "swizzle",
    "team",
    "item_slot",
    "item_slots",
    "resource_location",
    "function",
    "entity_anchor",
    "int_range",
    "float_range",
    "dimension",
    "gamemode",
    "time",
    "resource_or_tag",
    "resource_or_tag_key",
    "resource",
    "resource_key",
    "resource_selector",
    "template_mirror",
    "template_rotation",
    "heightmap",
    "loot_table",
    "loot_predicate",
    "loot_modifier",
    "context_float_provider",
    "context_int_provider",
    "slot_source",
    "dialog",
    "feature",
    "swing_animation",
    "uuid",
    "mob_effect",
    "item_enchantment",
    "entity_summon",
};

// Each numbered version's parsers, as the ids above, in the order that version numbers them.
constexpr std::array<std::int8_t, 48> k1_19 = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                                               16, 18, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
                                               34, 36, 62, 37, 38, 39, 40, 63, 64, 41, 43, 44, 46, 49, 50, 61};
constexpr std::array<std::int8_t, 48> k1_19_3 = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                                                 16, 18, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
                                                 34, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 50, 61};
constexpr std::array<std::int8_t, 49> k1_19_4 = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                                 18, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 36,
                                                 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 50, 51, 61};
constexpr std::array<std::int8_t, 50> k1_20_3 = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                                 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
                                                 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 50, 51, 61};
constexpr std::array<std::int8_t, 54> k1_20_5 = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 50, 51, 52, 53, 54, 61};
constexpr std::array<std::int8_t, 55> k1_21_5 = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 61};
constexpr std::array<std::int8_t, 57> k1_21_6 = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 58, 61};

std::span<const std::int8_t> numbering(std::int32_t farEnd) {
    if (farEnd >= versions::kV1_21_6) {
        return k1_21_6;
    }
    if (farEnd >= versions::kV1_21_5) {
        return k1_21_5;
    }
    if (farEnd >= versions::kV1_20_5) {
        return k1_20_5;
    }
    if (farEnd >= versions::kV1_20_3) {
        return k1_20_3;
    }
    if (farEnd >= versions::kV1_19_4) {
        return k1_19_4;
    }
    return farEnd >= versions::kV1_19_3 ? std::span<const std::int8_t>{k1_19_3} : std::span<const std::int8_t>{k1_19};
}

Parser text(Parser::StringKind kind) { return Parser{type::kBrigadierString, Parser::StringProperties{kind}}; }

Parser registryParser(const Parser& parser, std::int32_t farEnd,
                      const std::function<bool(const std::string&)>& missing) {
    const auto* registry = parser.as<Parser::RegistryProperties>();
    const std::string name = registry != nullptr ? registry->registry : std::string{};
    if (farEnd < versions::kV1_19_3) {
        if (name == "minecraft:mob_effect") {
            return Parser{kMobEffect};
        }
        if (name == "minecraft:enchantment") {
            return Parser{kItemEnchantment};
        }
        if (name == "minecraft:entity_type") {
            return Parser{kEntitySummon};
        }
    }
    const bool key = parser.registryId() == type::kResourceKey || parser.registryId() == type::kResourceOrTagKey;
    if (missing(name) || (key && farEnd < versions::kV1_19_3) || farEnd < versions::kV1_18_2) {
        return Parser{type::kResourceLocation};
    }
    return parser;
}

/// The nearest parser the far end has, still in this server's numbering.
Parser older(const Parser& parser, std::int32_t farEnd, const std::function<bool(const std::string&)>& missing) {
    using Kind = Parser::StringKind;
    switch (parser.registryId()) {
    case type::kHexColor:
        return farEnd < versions::kV1_21_6 ? text(Kind::SingleWord) : parser;
    case type::kDialog:
        return farEnd < versions::kV1_21_6 ? Parser{type::kResourceLocation} : parser;
    case type::kResourceSelector:
        if (const auto* registry = parser.as<Parser::RegistryProperties>();
            farEnd < versions::kV1_21_5 || registry == nullptr || missing(registry->registry)) {
            return text(Kind::QuotablePhrase);
        }
        return parser;
    case type::kLootTable:
    case type::kLootPredicate:
    case type::kLootModifier:
        return farEnd < versions::kV1_20_5 ? Parser{type::kResourceLocation} : parser;
    case type::kItemSlots:
        return farEnd < versions::kV1_20_5 ? Parser{type::kItemSlot} : parser;
    case type::kStyle:
        return farEnd < versions::kV1_20_3 ? text(Kind::QuotablePhrase) : parser;
    case type::kHeightmap:
        return farEnd < versions::kV1_19_4 ? text(Kind::SingleWord) : parser;
    case type::kGamemode:
        return farEnd < versions::kV1_19_3 ? text(Kind::SingleWord) : parser;
    case type::kTemplateMirror:
    case type::kTemplateRotation:
        return farEnd < versions::kV1_19 ? text(Kind::SingleWord) : parser;
    case type::kTime:
        if (farEnd < versions::kV1_14) {
            return Parser{type::kBrigadierInteger, Parser::Bounds<std::int32_t>{}};
        }
        return farEnd < versions::kV1_19_4 ? Parser{type::kTime} : parser;
    case type::kUuid:
        return farEnd < versions::kV1_16 ? Parser{type::kGameProfile} : parser;
    case type::kAngle:
        return farEnd < versions::kV1_16_2 ? text(Kind::SingleWord) : parser;
    case type::kNbtTag:
        return farEnd < versions::kV1_14 ? text(Kind::GreedyPhrase) : parser;
    case type::kColumnPos:
        return farEnd < versions::kV1_13_1 ? Parser{type::kVec2} : parser;
    case type::kDimension:
        return farEnd < versions::kV1_13_1 ? text(Kind::SingleWord) : parser;
    case type::kResource:
    case type::kResourceOrTag:
    case type::kResourceKey:
    case type::kResourceOrTagKey:
        return registryParser(parser, farEnd, missing);
    case type::kContextFloatProvider:
    case type::kContextIntProvider:
    case type::kSlotSource:
    case type::kFeature:
    case type::kSwingAnimation:
        return text(Kind::QuotablePhrase);
    default:
        return parser;
    }
}

std::string nameOf(std::int32_t id, std::int32_t farEnd) {
    const std::string_view name = kNames[static_cast<std::size_t>(id)];
    if (id == type::kNbtCompoundTag && farEnd < versions::kV1_14) {
        return "minecraft:nbt";
    }
    return name.starts_with("brigadier:") ? std::string{name} : "minecraft:" + std::string{name};
}

void writeNode(io::ByteBuffer& out, const play::CommandNodeData& node, std::int32_t farEnd,
               const std::function<bool(const std::string&)>& missing) {
    constexpr std::uint8_t kExecutable = 4;
    constexpr std::uint8_t kRedirect = 8;
    constexpr std::uint8_t kSuggestions = 16;
    const bool argument = node.kind == play::CommandNodeData::Kind::Argument && node.parser.has_value();
    auto flags = static_cast<std::uint8_t>(node.kind);
    flags |= node.executable ? kExecutable : 0;
    flags |= node.redirect.has_value() ? kRedirect : 0;
    flags |= argument && node.suggestionId.has_value() ? kSuggestions : 0;
    out.writeByte(flags);
    protocol::varInt::write(out, static_cast<std::int32_t>(node.children.size()));
    for (const std::int32_t child : node.children) {
        protocol::varInt::write(out, child);
    }
    if (node.redirect.has_value()) {
        protocol::varInt::write(out, *node.redirect);
    }
    if (node.kind != play::CommandNodeData::Kind::Root) {
        protocol::writeString(out, node.name.value_or(""));
    }
    if (!argument) {
        return;
    }
    const Parser target = older(*node.parser, farEnd, missing);
    std::int32_t id = target.registryId();
    if (farEnd >= versions::kV1_19) {
        const std::span<const std::int8_t> order = numbering(farEnd);
        const auto found = std::ranges::find(order, static_cast<std::int8_t>(id));
        id = found == order.end() ? type::kBrigadierString : static_cast<std::int32_t>(found - order.begin());
        const Parser numbered =
            found == order.end() ? text(Parser::StringKind::SingleWord) : Parser{id, target.properties()};
        numbered.write(out);
    } else {
        protocol::writeString(out, nameOf(id, farEnd));
        io::ByteBuffer properties;
        Parser{0, target.properties()}.write(properties);
        out.writeBytes(properties.readable().subspan(1));
    }
    if (node.suggestionId.has_value()) {
        protocol::writeString(out, *node.suggestionId);
    }
}

} // namespace

std::vector<std::uint8_t> write(const play::CommandsPacket& commands, std::int32_t farEnd,
                                const std::function<bool(const std::string&)>& missing) {
    io::ByteBuffer out;
    protocol::varInt::write(out, static_cast<std::int32_t>(commands.nodes.size()));
    for (const play::CommandNodeData& node : commands.nodes) {
        writeNode(out, node, farEnd, missing);
    }
    protocol::varInt::write(out, commands.rootIndex);
    return out.toVector();
}

} // namespace basalt::wire::multiversion::java::olderCommands
