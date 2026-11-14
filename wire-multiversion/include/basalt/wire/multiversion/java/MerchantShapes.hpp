#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::wire::multiversion::java::merchantShapes {

/// What a version's list of trades carries, from 1.13's plugin message to 1.20.4's packet.
struct Layout {
    bool varIntWindow = true;
    bool varIntCount = true;
    bool optionalSecond = false;
    bool pricing = true;
    bool demand = true;
    bool villager = true;
    bool restock = true;
};

inline constexpr Layout k1_19{};
inline constexpr Layout k1_14_4{true, false, true, true, true, true, true};
inline constexpr Layout k1_14_3{true, false, true, true, false, true, true};
inline constexpr Layout k1_14{true, false, true, true, false, true, false};
inline constexpr Layout k1_13{false, false, true, false, false, false, false};

/// Copies one stack, reshaping it on the way; false when the input is malformed.
using StackCopy = bool (*)(io::ByteBuffer& input, io::ByteBuffer& output);

/// The 1.13 channel that carried the trades before 1.14 gave them a packet, and its pre-1.13 name.
inline constexpr std::string_view kTraderList = "minecraft:trader_list";
inline constexpr std::string_view kLegacyTraderList = "MC|TrList";

/// Writes the native offers, renumbered already, as 1.20.4 has them with its flagged stacks.
void toFlaggedStacks(MutablePacket& packet, std::int32_t older);

/// Rewrites a list of trades from one layout to another; stacks are passed through `copy`.
[[nodiscard]] bool reshape(io::ByteBuffer& input, io::ByteBuffer& output, const Layout& from, const Layout& to,
                           StackCopy copy);

/// reshape over a whole packet body, leaving it untouched when it does not read.
void reshape(MutablePacket& packet, const Layout& from, const Layout& to, StackCopy copy);

} // namespace basalt::wire::multiversion::java::merchantShapes
