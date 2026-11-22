#include "HostWorlds.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace::worlds {
namespace {
constexpr std::int32_t kLoad = 1;
constexpr std::int32_t kGenerate = 8;
constexpr std::int32_t kLightBytes = 2048;
constexpr std::int32_t kCells = (kHeight >> 2) * 16;
const std::vector<std::string> kBiomes{"minecraft:plains", "minecraft:desert", "minecraft:forest"};

[[nodiscard]] bool has(std::int32_t flags, std::int32_t bit) { return (flags & bit) != 0; }

[[nodiscard]] bool brought(SimWorld& sw, std::int32_t cx, std::int32_t cz, bool generate) {
    if (sw.loaded.contains({cx, cz})) {
        return true;
    }
    if (generate || sw.stored.contains({cx, cz})) {
        sw.loaded.insert({cx, cz});
        return true;
    }
    return false;
}

[[nodiscard]] std::vector<std::int32_t> flatten(const std::set<Chunk>& chunks) {
    std::vector<std::int32_t> flat;
    for (const Chunk& chunk : chunks) {
        flat.push_back(chunk.first);
        flat.push_back(chunk.second);
    }
    return flat;
}

void chunkStatus(void* u, Call& c) {
    Traced t(u, c, "world_chunk_status", "wii");
    t.end(t.status != kOk ? t.status : (t.sim->loaded.contains({c.i32(1), c.i32(2)}) ? 2 : 0));
}

void chunkLoad(void* u, Call& c) {
    Traced t(u, c, "world_chunk_load", "wiii");
    t.end(t.status != kOk ? t.status : (brought(*t.sim, c.i32(1), c.i32(2), has(c.i32(3), kGenerate)) ? 1 : 0));
}

void chunkUnload(void* u, Call& c) {
    Traced t(u, c, "world_chunk_unload", "wii");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const Chunk chunk{c.i32(1), c.i32(2)};
    const bool held = std::ranges::any_of(t.sim->held, [&](const auto& entry) { return entry.second.contains(chunk); });
    t.end(!held && chunk != Chunk{0, 0} && t.sim->loaded.erase(chunk) > 0 ? 1 : 0);
}

void chunkRequest(void* u, Call& c) {
    Traced t(u, c, "world_chunk_request", "wiiiii");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const std::int32_t request = beginRequest(t.world, c.i32(5));
    answerLater(t.world, request, Value{brought(*t.sim, c.i32(1), c.i32(2), has(c.i32(3), kGenerate))});
    t.end(request);
}

void chunksLoaded(void* u, Call& c) {
    Traced t(u, c, "world_chunks_loaded", "wO");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    t.endInts(c.i32(1), c.i32(2), flatten(t.sim->loaded));
}

void chunkHold(void* u, Call& c) {
    Traced t(u, c, "world_chunk_hold", "wsxi");
    const auto tag = c.memory().string(c.i32(1), c.i32(2));
    const auto flat = t.ints(c.i32(3), c.i32(4) * 2);
    if (t.status != kOk || !tag || !flat) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    for (std::size_t at = 0; at + 1 < flat->size(); at += 2) {
        t.sim->held[std::string{*tag}].insert({(*flat)[at], (*flat)[at + 1]});
        t.sim->loaded.insert({(*flat)[at], (*flat)[at + 1]});
    }
    t.end(kOk);
}

void chunkRelease(void* u, Call& c) {
    Traced t(u, c, "world_chunk_release", "wsxi");
    const auto tag = c.memory().string(c.i32(1), c.i32(2));
    if (t.status != kOk || !tag) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    if (c.i32(4) < 0) {
        t.sim->held.erase(std::string{*tag});
        t.end(kOk);
        return;
    }
    const auto flat = t.ints(c.i32(3), c.i32(4) * 2);
    for (std::size_t at = 0; flat && at + 1 < flat->size(); at += 2) {
        t.sim->held[std::string{*tag}].erase({(*flat)[at], (*flat)[at + 1]});
    }
    t.end(flat ? kOk : kErrInvalid);
}

void chunkHeld(void* u, Call& c) {
    Traced t(u, c, "world_chunk_held", "wso");
    const auto tag = c.memory().string(c.i32(1), c.i32(2));
    if (t.status != kOk || !tag) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    t.endInts(c.i32(3), c.i32(4), flatten(t.sim->held[std::string{*tag}]));
}

void chunksResend(void* u, Call& c) {
    Traced t(u, c, "world_chunks_resend", "wxi");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    emitLine("world resend chunks=" + std::to_string(c.i32(2)));
    t.end(kOk);
}

void storedGet(void* u, Call& c) {
    Traced t(u, c, "world_stored_get", "wiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const auto found = t.sim->stored.find({c.i32(1), c.i32(2)});
    if (found == t.sim->stored.end()) {
        t.end(kErrAbsent);
        return;
    }
    t.endBytes(c.i32(3), c.i32(4), found->second);
}

void storedPut(void* u, Call& c) {
    Traced t(u, c, "world_stored_put", "wiiy");
    const auto nbt = t.bytes(c.i32(3), c.i32(4));
    if (t.status != kOk || !nbt) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    if (t.sim->loaded.contains({c.i32(1), c.i32(2)})) {
        t.end(kErrAbsent);
        return;
    }
    t.sim->stored[{c.i32(1), c.i32(2)}] = *nbt;
    t.end(kOk);
}

void storedList(void* u, Call& c) {
    Traced t(u, c, "world_stored_list", "wiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    std::vector<std::int32_t> flat;
    for (const auto& [chunk, nbt] : t.sim->stored) {
        if ((chunk.first >> 5) == c.i32(1) && (chunk.second >> 5) == c.i32(2)) {
            flat.push_back(chunk.first);
            flat.push_back(chunk.second);
        }
    }
    t.endInts(c.i32(3), c.i32(4), flat);
}

void lightAt(void* u, Call& c) {
    Traced t(u, c, "world_light_at", "wiiii");
    if (t.status != kOk) {
        t.end(t.status);
    } else if (!loaded(*t.sim, c.i32(1), c.i32(3))) {
        t.end(kErrAbsent);
    } else if (c.i32(4) == 1) {
        t.end(c.i32(2) >= 64 ? 15 : 0);
    } else {
        t.end(stateAt(*t.sim, {c.i32(1), c.i32(2), c.i32(3)}) == 5 ? 15 : 0);
    }
}

void lightRead(void* u, Call& c) {
    Traced t(u, c, "world_light_read", "wiiiiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    if (!loaded(*t.sim, c.i32(1) * 16, c.i32(3) * 16) && !has(c.i32(5), kLoad)) {
        t.end(kErrAbsent);
        return;
    }
    t.sim->loaded.insert({c.i32(1), c.i32(3)});
    const std::vector<std::uint8_t> nibbles(kLightBytes, c.i32(4) == 1 && c.i32(2) >= 4 ? 0xFF : 0x00);
    const std::int32_t result = t.memory.put(
        c.i32(6), c.i32(7), std::string_view{reinterpret_cast<const char*>(nibbles.data()), nibbles.size()});
    c.ret(result >= 0 && result <= c.i32(7) ? t.rec.done(result, "sum=" + std::to_string(nibbles[0] * kLightBytes))
                                            : t.rec.done(result));
}

void lightWrite(void* u, Call& c) {
    Traced t(u, c, "world_light_write", "wiiiiy");
    const auto nibbles = t.bytes(c.i32(5), c.i32(6));
    if (t.status != kOk || !nibbles) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
    } else if (nibbles->size() != static_cast<std::size_t>(kLightBytes)) {
        t.end(kErrInvalid);
    } else {
        t.sim->lightWrites.push_back(*nibbles);
        t.end(kOk);
    }
}

void lightCheck(void* u, Call& c) {
    Traced t(u, c, "world_light_check", "wxii");
    const auto flat = t.ints(c.i32(1), c.i32(2) * 2);
    if (t.status != kOk || !flat) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    if (c.i32(3) == 0) {
        t.end(kOk);
        return;
    }
    std::int64_t count = 0;
    for (std::size_t at = 0; at + 1 < flat->size(); at += 2) {
        count += t.sim->loaded.contains({(*flat)[at], (*flat)[at + 1]}) ? 1 : 0;
    }
    const std::int32_t request = beginRequest(t.world, c.i32(3));
    answerLater(t.world, request, Value{count});
    t.end(request);
}

[[nodiscard]] Pos cellOf(std::int32_t x, std::int32_t y, std::int32_t z) { return {x >> 2, y >> 2, z >> 2}; }

void biomeGet(void* u, Call& c) {
    Traced t(u, c, "world_biome_get", "wiiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    if (!loaded(*t.sim, c.i32(1), c.i32(3))) {
        t.end(kErrAbsent);
        return;
    }
    const auto found = t.sim->biomeCells.find(cellOf(c.i32(1), c.i32(2), c.i32(3)));
    t.endText(c.i32(4), c.i32(5),
              kBiomes[found == t.sim->biomeCells.end() ? 0 : static_cast<std::size_t>(found->second)]);
}

void biomeSet(void* u, Call& c) {
    Traced t(u, c, "world_biome_set", "wiiis");
    const auto biome = c.memory().string(c.i32(4), c.i32(5));
    if (t.status != kOk || !biome) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    const auto found = std::ranges::find(kBiomes, *biome);
    if (found == kBiomes.end() || !loaded(*t.sim, c.i32(1), c.i32(3))) {
        t.end(kErrAbsent);
        return;
    }
    t.sim->biomeCells[cellOf(c.i32(1), c.i32(2), c.i32(3))] = static_cast<std::int32_t>(found - kBiomes.begin());
    t.end(kOk);
}

void biomesRead(void* u, Call& c) {
    Traced t(u, c, "world_biomes_read", "wiiO");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    if (!loaded(*t.sim, c.i32(1) * 16, c.i32(2) * 16)) {
        t.end(kErrAbsent);
        return;
    }
    std::vector<std::int32_t> ids(static_cast<std::size_t>(kCells), 0);
    for (const auto& [cell, id] : t.sim->biomeCells) {
        if (cell[0] >> 2 == c.i32(1) && cell[2] >> 2 == c.i32(2)) {
            const std::int32_t index = ((cell[1] - (kMinY >> 2)) * 4 + (cell[2] & 3)) * 4 + (cell[0] & 3);
            ids[static_cast<std::size_t>(index)] = id;
        }
    }
    std::int64_t sum = 0;
    for (const std::int32_t id : ids) {
        sum += id;
    }
    if (kCells <= c.i32(4) && !t.writeInts(c.i32(3), ids)) {
        t.end(kErrInvalid);
        return;
    }
    c.ret(kCells <= c.i32(4) ? t.rec.done(kCells, "sum=" + std::to_string(sum)) : t.rec.done(kCells));
}

void biomesWrite(void* u, Call& c) {
    Traced t(u, c, "world_biomes_write", "wiixi");
    const auto ids = t.ints(c.i32(3), c.i32(4));
    if (t.status != kOk || !ids) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    if (!loaded(*t.sim, c.i32(1) * 16, c.i32(2) * 16)) {
        t.end(kErrAbsent);
        return;
    }
    for (std::size_t index = 0; index < ids->size(); ++index) {
        if ((*ids)[index] >= 0) {
            const auto cx = static_cast<std::int32_t>(index % 4);
            const auto cz = static_cast<std::int32_t>((index / 4) % 4);
            const auto cy = static_cast<std::int32_t>(index / 16) + (kMinY >> 2);
            t.sim->biomeCells[{c.i32(1) * 4 + cx, cy, c.i32(2) * 4 + cz}] = (*ids)[index];
        }
    }
    t.end(kOk);
}
} // namespace
} // namespace basalt::wire::wasm::trace::worlds

namespace basalt::wire::wasm::trace {
void addWorldsChunksImports(World& world, std::vector<HostImport>& out) {
    using namespace worlds;
    World* user = &world;
    out.insert(out.end(), {
                              {"world_chunk_status", chunkStatus, user},   {"world_chunk_load", chunkLoad, user},
                              {"world_chunk_unload", chunkUnload, user},   {"world_chunk_request", chunkRequest, user},
                              {"world_chunks_loaded", chunksLoaded, user}, {"world_chunk_hold", chunkHold, user},
                              {"world_chunk_release", chunkRelease, user}, {"world_chunk_held", chunkHeld, user},
                              {"world_chunks_resend", chunksResend, user}, {"world_stored_get", storedGet, user},
                              {"world_stored_put", storedPut, user},       {"world_stored_list", storedList, user},
                              {"world_light_at", lightAt, user},           {"world_light_read", lightRead, user},
                              {"world_light_write", lightWrite, user},     {"world_light_check", lightCheck, user},
                              {"world_biome_get", biomeGet, user},         {"world_biome_set", biomeSet, user},
                              {"world_biomes_read", biomesRead, user},     {"world_biomes_write", biomesWrite, user},
                          });
}
} // namespace basalt::wire::wasm::trace
