#include "HostWorlds.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace::worlds {
namespace {
constexpr std::int32_t kLoad = 1;
constexpr std::int32_t kResend = 4;
constexpr std::int32_t kFirstSection = kMinY >> 4;
constexpr std::int32_t kSections = kHeight >> 4;

[[nodiscard]] bool has(std::int32_t flags, std::int32_t bit) { return (flags & bit) != 0; }

/// True when the chunk is usable; a LOAD flag loads it first.
[[nodiscard]] bool usable(SimWorld& sw, std::int32_t x, std::int32_t z, bool load) {
    if (loaded(sw, x, z)) {
        return true;
    }
    if (load) {
        sw.loaded.insert(chunkOf(x, z));
        return true;
    }
    return false;
}

void put(SimWorld& sw, const Pos& at, std::int32_t state) {
    if (state == 0) {
        sw.blocks.erase(at);
    } else {
        sw.blocks[at] = state;
    }
}

[[nodiscard]] std::string summary(std::span<const std::int32_t> states) {
    std::int64_t nonAir = 0;
    std::int64_t sum = 0;
    for (const std::int32_t state : states) {
        nonAir += state != 0 ? 1 : 0;
        sum += state;
    }
    return "nonair=" + std::to_string(nonAir) + " sum=" + std::to_string(sum);
}

[[nodiscard]] std::vector<std::int32_t> section(const SimWorld& sw, std::int32_t cx, std::int32_t sy, std::int32_t cz) {
    std::vector<std::int32_t> states(kSectionBlocks, 0);
    for (std::int32_t index = 0; index < kSectionBlocks; ++index) {
        const Pos at{cx * 16 + (index & 15), sy * 16 + (index >> 8), cz * 16 + ((index >> 4) & 15)};
        states[static_cast<std::size_t>(index)] = stateAt(sw, at);
    }
    return states;
}

void writeSection(SimWorld& sw, std::int32_t cx, std::int32_t sy, std::int32_t cz, std::span<const std::int32_t> states,
                  std::int32_t& changed) {
    for (std::int32_t index = 0; index < kSectionBlocks; ++index) {
        const std::int32_t state = states[static_cast<std::size_t>(index)];
        const Pos at{cx * 16 + (index & 15), sy * 16 + (index >> 8), cz * 16 + ((index >> 4) & 15)};
        if (state >= 0 && state != stateAt(sw, at)) {
            put(sw, at, state);
            ++changed;
        }
    }
}

void blockGet(void* u, Call& c) {
    Traced t(u, c, "world_block_get", "wiiiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    if (!usable(*t.sim, c.i32(1), c.i32(3), has(c.i32(4), kLoad))) {
        t.end(kErrAbsent);
        return;
    }
    t.endText(c.i32(5), c.i32(6), stateText(stateAt(*t.sim, {c.i32(1), c.i32(2), c.i32(3)})));
}

void blockSet(void* u, Call& c) {
    Traced t(u, c, "world_block_set", "wiiisi");
    const auto text = c.memory().string(c.i32(4), c.i32(5));
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const std::int32_t id = text ? stateId(*text) : -1;
    if (id < 0) {
        t.end(kErrInvalid);
    } else if (!loaded(*t.sim, c.i32(1), c.i32(3))) {
        t.end(kErrAbsent);
    } else {
        put(*t.sim, {c.i32(1), c.i32(2), c.i32(3)}, id);
        t.end(kOk);
    }
}

void stateGet(void* u, Call& c) {
    Traced t(u, c, "world_state_get", "wiiii");
    if (t.status != kOk) {
        t.end(t.status);
    } else if (!usable(*t.sim, c.i32(1), c.i32(3), has(c.i32(4), kLoad))) {
        t.end(kErrAbsent);
    } else {
        t.end(stateAt(*t.sim, {c.i32(1), c.i32(2), c.i32(3)}));
    }
}

void stateSet(void* u, Call& c) {
    Traced t(u, c, "world_state_set", "wiiiii");
    if (t.status != kOk) {
        t.end(t.status);
    } else if (stateText(c.i32(4)).empty()) {
        t.end(kErrInvalid);
    } else if (!loaded(*t.sim, c.i32(1), c.i32(3))) {
        t.end(kErrAbsent);
    } else {
        put(*t.sim, {c.i32(1), c.i32(2), c.i32(3)}, c.i32(4));
        t.end(kOk);
    }
}

void statesRead(void* u, Call& c) {
    Traced t(u, c, "world_states_read", "wxixi");
    const auto positions = t.ints(c.i32(1), c.i32(2) * 3);
    if (t.status != kOk || !positions) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    std::vector<std::int32_t> states;
    for (std::size_t at = 0; at + 2 < positions->size(); at += 3) {
        const Pos where{(*positions)[at], (*positions)[at + 1], (*positions)[at + 2]};
        states.push_back(usable(*t.sim, where[0], where[2], has(c.i32(4), kLoad)) ? stateAt(*t.sim, where) : -1);
    }
    if (!t.writeInts(c.i32(3), states)) {
        t.end(kErrInvalid);
        return;
    }
    c.ret(t.rec.done(c.i32(2), list(states)));
}

void statesWrite(void* u, Call& c) {
    Traced t(u, c, "world_states_write", "wxxii");
    const auto positions = t.ints(c.i32(1), c.i32(3) * 3);
    const auto states = t.ints(c.i32(2), c.i32(3));
    if (t.status != kOk || !positions || !states) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    std::int32_t written = 0;
    for (std::size_t index = 0; index < states->size(); ++index) {
        const Pos where{(*positions)[index * 3], (*positions)[index * 3 + 1], (*positions)[index * 3 + 2]};
        if (loaded(*t.sim, where[0], where[2])) {
            put(*t.sim, where, (*states)[index]);
            ++written;
        }
    }
    t.end(written);
}

void fill(void* u, Call& c) {
    Traced t(u, c, "world_fill", "wiiiiiiii");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const Pos low{std::min(c.i32(1), c.i32(4)), std::min(c.i32(2), c.i32(5)), std::min(c.i32(3), c.i32(6))};
    const Pos high{std::max(c.i32(1), c.i32(4)), std::max(c.i32(2), c.i32(5)), std::max(c.i32(3), c.i32(6))};
    const std::int64_t volume =
        static_cast<std::int64_t>(high[0] - low[0] + 1) * (high[1] - low[1] + 1) * (high[2] - low[2] + 1);
    if (volume > 16777216 || stateText(c.i32(7)).empty()) {
        t.end(kErrInvalid);
        return;
    }
    std::int32_t changed = 0;
    for (std::int32_t y = low[1]; y <= high[1]; ++y) {
        for (std::int32_t z = low[2]; z <= high[2]; ++z) {
            for (std::int32_t x = low[0]; x <= high[0]; ++x) {
                if (usable(*t.sim, x, z, has(c.i32(8), kLoad)) && stateAt(*t.sim, {x, y, z}) != c.i32(7)) {
                    put(*t.sim, {x, y, z}, c.i32(7));
                    ++changed;
                }
            }
        }
    }
    t.end(changed);
}

void sectionRead(void* u, Call& c) {
    Traced t(u, c, "world_section_read", "wiiiix");
    if (t.status != kOk) {
        t.end(t.status);
    } else if (!usable(*t.sim, c.i32(1) * 16, c.i32(3) * 16, has(c.i32(4), kLoad))) {
        t.end(kErrAbsent);
    } else {
        const std::vector<std::int32_t> states = section(*t.sim, c.i32(1), c.i32(2), c.i32(3));
        c.ret(t.writeInts(c.i32(5), states) ? t.rec.done(kSectionBlocks, summary(states)) : t.rec.done(kErrInvalid));
    }
}

void sectionWrite(void* u, Call& c) {
    Traced t(u, c, "world_section_write", "wiiixi");
    const auto states = t.ints(c.i32(4), kSectionBlocks);
    if (t.status != kOk || !states) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
    } else if (!loaded(*t.sim, c.i32(1) * 16, c.i32(3) * 16) || c.i32(2) < kFirstSection ||
               c.i32(2) >= kFirstSection + kSections) {
        t.end(kErrAbsent);
    } else {
        std::int32_t changed = 0;
        writeSection(*t.sim, c.i32(1), c.i32(2), c.i32(3), *states, changed);
        t.end(kOk);
    }
}

void chunkRead(void* u, Call& c) {
    Traced t(u, c, "world_chunk_read", "wiiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    if (!usable(*t.sim, c.i32(1) * 16, c.i32(2) * 16, has(c.i32(3), kLoad))) {
        t.end(kErrAbsent);
        return;
    }
    std::vector<std::int32_t> all;
    for (std::int32_t at = 0; at < kSections; ++at) {
        const std::vector<std::int32_t> part = section(*t.sim, c.i32(1), kFirstSection + at, c.i32(2));
        all.insert(all.end(), part.begin(), part.end());
    }
    const auto needed = static_cast<std::int32_t>(all.size());
    if (needed <= c.i32(5) && !t.writeInts(c.i32(4), all)) {
        t.end(kErrInvalid);
        return;
    }
    c.ret(needed <= c.i32(5) ? t.rec.done(needed, summary(all)) : t.rec.done(needed));
}

void chunkApply(void* u, Call& c) {
    Traced t(u, c, "world_chunk_apply", "wiixixix");
    const std::int32_t count = c.i32(4);
    const auto ys = t.ints(c.i32(3), count);
    const auto states = t.ints(c.i32(5), count < 0 || count > 64 ? -1 : count * kSectionBlocks);
    if (t.status != kOk || !ys || !states) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    if (!loaded(*t.sim, c.i32(1) * 16, c.i32(2) * 16)) {
        t.end(kErrAbsent);
        return;
    }
    std::int32_t changed = 0;
    std::vector<std::int32_t> previous;
    for (std::size_t at = 0; at < ys->size(); ++at) {
        const std::int32_t sy = (*ys)[at];
        if (sy < kFirstSection || sy >= kFirstSection + kSections) {
            t.end(kErrInvalid);
            return;
        }
        const std::vector<std::int32_t> before = section(*t.sim, c.i32(1), sy, c.i32(2));
        previous.insert(previous.end(), before.begin(), before.end());
        writeSection(*t.sim, c.i32(1), sy, c.i32(2),
                     std::span<const std::int32_t>{*states}.subspan(at * kSectionBlocks, kSectionBlocks), changed);
    }
    if (c.i32(7) != 0 && !t.writeInts(c.i32(7), previous)) {
        t.end(kErrInvalid);
        return;
    }
    c.ret(t.rec.done(changed, c.i32(7) != 0 ? "previous " + summary(previous) : std::string{}));
    static_cast<void>(has(c.i32(6), kResend));
}

void changesApply(void* u, Call& c) {
    Traced t(u, c, "world_changes_apply", "wxxii");
    const auto positions = t.ints(c.i32(1), c.i32(3) * 3);
    const auto states = t.ints(c.i32(2), c.i32(3));
    if (t.status != kOk || !positions || !states) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    std::int32_t changed = 0;
    for (std::size_t index = 0; index < states->size(); ++index) {
        const Pos where{(*positions)[index * 3], (*positions)[index * 3 + 1], (*positions)[index * 3 + 2]};
        if (loaded(*t.sim, where[0], where[2]) && stateAt(*t.sim, where) != (*states)[index]) {
            put(*t.sim, where, (*states)[index]);
            ++changed;
        }
    }
    t.end(changed);
}

[[nodiscard]] std::int32_t loadedCount(const SimWorld& sw, const std::vector<std::int32_t>& positions) {
    std::int32_t count = 0;
    for (std::size_t at = 0; at + 2 < positions.size(); at += 3) {
        count += loaded(sw, positions[at], positions[at + 2]) ? 1 : 0;
    }
    return count;
}

void blocksSettle(void* u, Call& c) {
    Traced t(u, c, "world_blocks_settle", "wxxii");
    const auto positions = t.ints(c.i32(1), c.i32(3) * 3);
    if (t.status != kOk || !positions) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    emitLine("world settle effects=" + std::to_string(c.i32(4)));
    t.end(loadedCount(*t.sim, *positions));
}

void blocksNotify(void* u, Call& c) {
    Traced t(u, c, "world_blocks_notify", "wxxi");
    const auto positions = t.ints(c.i32(1), c.i32(3) * 3);
    if (t.status != kOk || !positions) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    t.end(loadedCount(*t.sim, *positions));
}

void blocksReshape(void* u, Call& c) {
    Traced t(u, c, "world_blocks_reshape", "wxii");
    const auto positions = t.ints(c.i32(1), c.i32(2) * 3);
    if (t.status != kOk || !positions) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    t.end(loadedCount(*t.sim, *positions));
}

void stateValid(void* u, Call& c) {
    Traced t(u, c, "world_state_valid", "wiiii");
    if (t.status != kOk) {
        t.end(t.status);
    } else {
        t.end(loaded(*t.sim, c.i32(1), c.i32(3)) ? c.i32(4) : kErrAbsent);
    }
}

void tickSchedule(void* u, Call& c) {
    Traced t(u, c, "world_tick_schedule", "wiiiii");
    t.end(t.status != kOk ? t.status : (stateAt(*t.sim, {c.i32(1), c.i32(2), c.i32(3)}) != 0 ? 1 : 0));
}

void tickPending(void* u, Call& c) {
    Traced t(u, c, "world_tick_pending", "wiii");
    t.end(t.status != kOk ? t.status : 0);
}

void blockEntityGet(void* u, Call& c) {
    Traced t(u, c, "world_block_entity_get", "wiiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const auto found = t.sim->blockEntities.find({c.i32(1), c.i32(2), c.i32(3)});
    if (found == t.sim->blockEntities.end()) {
        t.end(kErrAbsent);
        return;
    }
    t.endBytes(c.i32(4), c.i32(5), found->second);
}

void blockEntityEdit(void* u, Call& c) {
    Traced t(u, c, "world_block_entity_edit", "wiiiiy");
    const auto nbt = t.bytes(c.i32(5), c.i32(6));
    if (t.status != kOk || !nbt) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    const Pos at{c.i32(2), c.i32(3), c.i32(4)};
    const bool chest = stateAt(*t.sim, at) == 6;
    if (c.i32(1) == 2) {
        t.end(t.sim->blockEntities.erase(at) > 0 ? kOk : kErrAbsent);
    } else if (c.i32(1) == 0 || c.i32(1) == 1) {
        if (!chest || (c.i32(1) == 0 && !t.sim->blockEntities.contains(at))) {
            t.end(kErrAbsent);
        } else {
            t.sim->blockEntities[at] = *nbt;
            t.end(kOk);
        }
    } else {
        t.end(kErrInvalid);
    }
}

void blockEntities(void* u, Call& c) {
    Traced t(u, c, "world_block_entities", "wiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    if (!loaded(*t.sim, c.i32(1) * 16, c.i32(2) * 16)) {
        t.end(kErrAbsent);
        return;
    }
    std::vector<std::int32_t> flat;
    for (const auto& [at, nbt] : t.sim->blockEntities) {
        if (chunkOf(at[0], at[2]) == Chunk{c.i32(1), c.i32(2)}) {
            flat.insert(flat.end(), at.begin(), at.end());
        }
    }
    t.endInts(c.i32(3), c.i32(4), flat);
}

void dataGet(void* u, Call& c) {
    Traced t(u, c, "world_data_get", "wiiiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const std::int32_t scope = c.i32(1);
    const std::vector<std::uint8_t>* data = nullptr;
    if (scope == 0) {
        data = &t.sim->pluginData;
    } else if (scope == 1 && loaded(*t.sim, c.i32(2) * 16, c.i32(4) * 16)) {
        const auto found = t.sim->chunkData.find({c.i32(2), c.i32(4)});
        data = found == t.sim->chunkData.end() ? nullptr : &found->second;
    } else if (scope == 2) {
        const auto found = t.sim->blockEntities.find({c.i32(2), c.i32(3), c.i32(4)});
        data = found == t.sim->blockEntities.end() ? nullptr : &found->second;
    } else if (scope != 1) {
        t.end(kErrInvalid);
        return;
    }
    if (data == nullptr) {
        t.end(kErrAbsent);
        return;
    }
    t.endBytes(c.i32(5), c.i32(6), *data);
}

void dataSet(void* u, Call& c) {
    Traced t(u, c, "world_data_set", "wiiiiy");
    const auto nbt = t.bytes(c.i32(5), c.i32(6));
    if (t.status != kOk || !nbt) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    switch (c.i32(1)) {
    case 0:
        t.sim->pluginData = *nbt;
        t.end(kOk);
        break;
    case 1:
        if (loaded(*t.sim, c.i32(2) * 16, c.i32(4) * 16)) {
            t.sim->chunkData[{c.i32(2), c.i32(4)}] = *nbt;
            t.end(kOk);
        } else {
            t.end(kErrAbsent);
        }
        break;
    case 2:
        if (t.sim->blockEntities.contains({c.i32(2), c.i32(3), c.i32(4)})) {
            t.sim->blockEntities[{c.i32(2), c.i32(3), c.i32(4)}] = *nbt;
            t.end(kOk);
        } else {
            t.end(kErrAbsent);
        }
        break;
    default:
        t.end(kErrInvalid);
    }
}
} // namespace
} // namespace basalt::wire::wasm::trace::worlds

namespace basalt::wire::wasm::trace {
void addWorldsBlocksImports(World& world, std::vector<HostImport>& out) {
    using namespace worlds;
    World* user = &world;
    out.insert(out.end(), {
                              {"world_block_get", blockGet, user},
                              {"world_block_set", blockSet, user},
                              {"world_state_get", stateGet, user},
                              {"world_state_set", stateSet, user},
                              {"world_states_read", statesRead, user},
                              {"world_states_write", statesWrite, user},
                              {"world_fill", fill, user},
                              {"world_section_read", sectionRead, user},
                              {"world_section_write", sectionWrite, user},
                              {"world_chunk_read", chunkRead, user},
                              {"world_chunk_apply", chunkApply, user},
                              {"world_changes_apply", changesApply, user},
                              {"world_blocks_settle", blocksSettle, user},
                              {"world_blocks_notify", blocksNotify, user},
                              {"world_blocks_reshape", blocksReshape, user},
                              {"world_state_valid", stateValid, user},
                              {"world_tick_schedule", tickSchedule, user},
                              {"world_tick_pending", tickPending, user},
                              {"world_block_entity_get", blockEntityGet, user},
                              {"world_block_entity_edit", blockEntityEdit, user},
                              {"world_block_entities", blockEntities, user},
                              {"world_data_get", dataGet, user},
                              {"world_data_set", dataSet, user},
                          });
}
} // namespace basalt::wire::wasm::trace
