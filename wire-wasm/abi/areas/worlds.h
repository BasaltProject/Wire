/* Area "worlds": worlds, blocks, sections and chunks, light, biomes, effects, structures and noise, time, weather, game
 * rules, world creation and the border. Server profile only.
 *
 * World handle. `world_by_name`, `world_default` and `world_list` give a positive i32 that stays valid for the life of
 * the plugin and answers STALE once that world was unloaded (a later world of the same name gets a new handle). A
 * handle is never a parameter of another area: those use a world name or a position triple.
 *
 * Threads. Every import here runs on the plugin thread, inside a callback or `basalt_init`, and touches the world the
 * way the owner of that world's region would. With the default scheduler (one region owning everything) that is the
 * tick thread and every call is direct. With regional scheduling a call is valid only for positions the calling region
 * owns; to work somewhere else, `world_at` queues a callback on the owner of that column. Block loops should use the
 * bulk imports (`world_states_read`, `world_states_write`, `world_fill`, the section and chunk calls) instead of one
 * import per block: one bulk call is one boundary crossing and one pass over the chunk.
 *
 * Bulk numerics. A block section is 4096 state ids as little-endian i32, ordered index = y * 256 + z * 16 + x with
 * x, y, z local to the section. A light section is 2048 bytes, two nibbles per byte (low nibble first), same order.
 * Biomes of a chunk are one i32 per 4x4x4 cell, index = (cellY * 4 + cellZ) * 4 + cellX from the world's lowest cell.
 * Position lists are flat i32 triples (x, y, z); chunk lists are flat i32 pairs (chunk x, chunk z). Every bulk buffer
 * is a plain `(int32_t *, count)` pair; where the host writes one, `cap` is in elements and the result is the number of
 * elements the full answer needs, written only when it fits.
 *
 * Block states. A block state is an i32 state id or text such as `minecraft:oak_stairs[facing=east,half=top]`;
 * `block_state_id` and `block_state_text` convert, `worlds_data` lists the blocks and their properties.
 *
 * Capabilities. `world_create`, `world_unload`, `world_delete` and `world_stored_put` need `worldgen`. Everything else
 * is open. A call that is denied returns BASALT_ERR_DENIED.
 *
 * Common results. INVALID is a malformed argument or handle, STALE a world that was unloaded, ABSENT a chunk or thing
 * that is not there, a name the world does not know, or a value that is unset.
 */
#ifndef BASALT_AREA_WORLDS_H
#define BASALT_AREA_WORLDS_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Flags shared by the calls that say they take them. Each is one bit and a call takes their sum: LOAD 1, NOTIFY 2,
 * RESEND 4, GENERATE 8, SAVE 16, DISK 32, GLOBAL 64, BREAK 128, FIRE 256, FLUID 512, INDIRECT 1024, ENTITIES 2048. */
#define BASALT_WORLD_LOAD 1       /* load (not generate) the chunk when it is not loaded */
#define BASALT_WORLD_NOTIFY 2     /* tell the players that watch the change */
#define BASALT_WORLD_RESEND 4     /* send the changed chunks again to their watchers */
#define BASALT_WORLD_GENERATE 8   /* generate the chunk when it does not exist */
#define BASALT_WORLD_SAVE 16      /* save before unloading */
#define BASALT_WORLD_DISK 32      /* also touch the copy kept on disk */
#define BASALT_WORLD_GLOBAL 64    /* reach every player of the world, not only those in range */
#define BASALT_WORLD_BREAK 128    /* an explosion breaks blocks */
#define BASALT_WORLD_FIRE 256     /* an explosion starts fires */
#define BASALT_WORLD_FLUID 512    /* a scheduled tick is the fluid's, not the block's */
#define BASALT_WORLD_INDIRECT 1024 /* shape updates reach the blocks next to the neighbours too */
#define BASALT_WORLD_ENTITIES 2048 /* include entities */

/* Effects a block change sets off, the bits of the `effects` argument, added together: NEIGHBORS 1, SHAPES 2, LIGHT 4,
 * ENTITIES 8, POI 16, TICKS 32, CLIENTS 64, PLACE 128, DEPARTURE 256, OUTPUT_SIGNAL 512. world_player_effect takes no
 * flag bits: its `op` and positional `args` are all it reads. */
#define BASALT_WORLD_EFFECT_NEIGHBORS 1  /* neighbours are told one of their neighbours changed */
#define BASALT_WORLD_EFFECT_SHAPES 2     /* blocks around reshape to fit (fences join, stairs turn) */
#define BASALT_WORLD_EFFECT_LIGHT 4
#define BASALT_WORLD_EFFECT_ENTITIES 8
#define BASALT_WORLD_EFFECT_POI 16
#define BASALT_WORLD_EFFECT_TICKS 32     /* the new block books its scheduled ticks */
#define BASALT_WORLD_EFFECT_CLIENTS 64
#define BASALT_WORLD_EFFECT_PLACE 128    /* the rest of `onPlace` (golems, pistons, dust) */
#define BASALT_WORLD_EFFECT_DEPARTURE 256 /* what the block that left sets off on its way out */
#define BASALT_WORLD_EFFECT_OUTPUT_SIGNAL 512 /* comparators around read the block again */

/* ---- Worlds ------------------------------------------------------------------------------------------------------ */

/* world_by_name: the handle of the world with this name, 0 when there is none. */
BASALT_IMPORT(world_by_name) int32_t basalt_world_by_name(const char *name, int32_t nlen);

/* world_default: the handle of the default world. */
BASALT_IMPORT(world_default) int32_t basalt_world_default(void);

/* world_list: writes the handles of every loaded world into `out` and returns the count the full list needs. */
BASALT_IMPORT(world_list) int32_t basalt_world_list(int32_t *out, int32_t cap);

/* world_name: the world's name as text, in the needed-length convention. STALE for an unloaded world. */
BASALT_IMPORT(world_name) int32_t basalt_world_name(int32_t world, char *out, int32_t cap);

/* world_get: one property of the world by name, as a value. ABSENT for an unknown name, STALE for an unloaded world.
 * The names come from the host's own table, which `world_get(world, "properties")` lists as a list of maps
 * {name, type, writable}. They include: name, dimension (`minecraft:overworld`), seed, min_height, height, sea_level,
 * view_distance (writable), time (age in ticks), day_time (0 to 23999, writable), raining and thundering (bool),
 * difficulty (string, writable), difficulty_locked (bool, writable), spawn (list of x, y, z, writable), folder,
 * player_count, block_entity_count, tickable_block_entity_count, block_version, forced_chunks (flat list of x, z),
 * weather_times (map rain, thunder, clear in ticks, writable), clock (map total_ticks, rate, paused, markers),
 * spawn_rules (map limits, ticks_per by mob category, allow_monsters, allow_animals; writable), rules (list of game
 * rule names), plugin_data (bytes, the NBT plugins keep on the world, writable) and `tunable.<key>` (a number, writable;
 * the keys are void_damage, void_damage_amount, void_damage_offset and simulation_distance). Result: the length of the
 * encoded value the full answer needs. */
BASALT_IMPORT(world_get)
int32_t basalt_world_get(int32_t world, const char *key, int32_t klen, char *out, int32_t cap);

/* world_set: sets a writable property; the value is encoded as the property's type says. Two extra keys take effect
 * and are not readable: `weather` (map raining, thundering, bools) and `clock_rate` (float 0.00001 to 1000).
 * INVALID for a value of the wrong type or out of range, ABSENT for an unknown or read-only name. OK on success. */
BASALT_IMPORT(world_set)
int32_t basalt_world_set(int32_t world, const char *key, int32_t klen, const char *value, int32_t vlen);

/* world_rule_get: a game rule as text ("true", "false" or a number). ABSENT for a rule the world does not know. The
 * names and types are listed by `worlds_data("game_rules")`. */
BASALT_IMPORT(world_rule_get)
int32_t basalt_world_rule_get(int32_t world, const char *name, int32_t nlen, char *out, int32_t cap);

/* world_rule_set: sets a game rule from text. INVALID for a value that does not fit the rule's type, ABSENT for an
 * unknown rule. */
BASALT_IMPORT(world_rule_set)
int32_t basalt_world_rule_set(int32_t world, const char *name, int32_t nlen, const char *value, int32_t vlen);

/* world_create: creates a world from a map {name, dimension?, type?, seed? (i64)} and returns its handle, or ABSENT
 * when the server cannot create it. `dimension` is `overworld` (the default), `nether` or `end`, with or without the
 * `minecraft:` prefix (`the_nether` and `the_end` also work), in any case; `type` is `normal` (the default), `flat`,
 * `void`, `amplified`, `large_biomes` or `debug`. An unknown dimension or type is ABSENT, not an overworld. Needs
 * `worldgen`. INVALID for a missing name or a name that already exists. Making the world is slow server work and is
 * not charged to the turn budget, so call it alone in a turn. */
BASALT_IMPORT(world_create) int32_t basalt_world_create(const char *spec, int32_t slen);

/* world_unload: takes a world that holds no player out of play, saving it first with BASALT_WORLD_SAVE. ABSENT when it
 * is not loaded, INVALID for the default world or a world with players. Every handle of it turns STALE. Needs
 * `worldgen`. */
BASALT_IMPORT(world_unload) int32_t basalt_world_unload(const char *name, int32_t nlen, int32_t flags);

/* world_delete: deletes an unloaded, non-default world from the server's storage, by name only, never a path. ABSENT
 * for a name that is not a known unloaded world. Needs `worldgen`. */
BASALT_IMPORT(world_delete) int32_t basalt_world_delete(const char *name, int32_t nlen);

/* world_at: runs the plugin's callback `token` on the owner of the column (x, z) of this world, later, and returns a
 * request handle (the completion value is null; see core_values.h). With the default scheduler the owner is the plugin
 * thread and the callback runs on the next turn. Use it to touch positions the calling region does not own. */
BASALT_IMPORT(world_at) int32_t basalt_world_at(int32_t world, double x, double z, int32_t token);

/* ---- Blocks ------------------------------------------------------------------------------------------------------ */

/* world_block_get: the block state at the position as text with its properties. ABSENT when the chunk is not loaded
 * and BASALT_WORLD_LOAD is not in `flags`. Needed-length convention. */
BASALT_IMPORT(world_block_get)
int32_t basalt_world_block_get(int32_t world, int32_t x, int32_t y, int32_t z, int32_t flags, char *out, int32_t cap);

/* world_block_set: puts the block (text with optional properties) at the position, telling watchers with
 * BASALT_WORLD_NOTIFY. INVALID for text that is not a block state, ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_block_set)
int32_t basalt_world_block_set(int32_t world, int32_t x, int32_t y, int32_t z, const char *block, int32_t blen,
                               int32_t flags);

/* world_state_get: the state id at the position, or ABSENT as for `world_block_get`. */
BASALT_IMPORT(world_state_get)
int32_t basalt_world_state_get(int32_t world, int32_t x, int32_t y, int32_t z, int32_t flags);

/* world_state_set: puts a state id at the position (BASALT_WORLD_NOTIFY in `flags`). INVALID for an id that is not a
 * state, ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_state_set)
int32_t basalt_world_state_set(int32_t world, int32_t x, int32_t y, int32_t z, int32_t state, int32_t flags);

/* world_states_read: for `count` position triples at `positions`, writes one state id each (-1 where the chunk is not
 * loaded and BASALT_WORLD_LOAD is not set) into `out` and returns count. Bulk. */
BASALT_IMPORT(world_states_read)
int32_t basalt_world_states_read(int32_t world, const int32_t *positions, int32_t count, int32_t *out, int32_t flags);

/* world_states_write: writes `states[i]` at triple `i` of `positions` for `count` entries; returns how many were
 * written. BASALT_WORLD_NOTIFY tells watchers. Bulk. */
BASALT_IMPORT(world_states_write)
int32_t basalt_world_states_write(int32_t world, const int32_t *positions, const int32_t *states, int32_t count,
                                  int32_t flags);

/* world_fill: fills the box between two corners (inclusive, in any order) with one state and returns how many blocks
 * changed. INVALID over 16777216 blocks. Chunks that are not loaded are skipped unless BASALT_WORLD_LOAD. Bulk. */
BASALT_IMPORT(world_fill)
int32_t basalt_world_fill(int32_t world, int32_t x1, int32_t y1, int32_t z1, int32_t x2, int32_t y2, int32_t z2,
                          int32_t state, int32_t flags);

/* world_section_read: the 4096 state ids of one section into `out` (room for 4096 i32). ABSENT when the chunk is not
 * loaded and BASALT_WORLD_LOAD is not set. Result 4096. */
BASALT_IMPORT(world_section_read)
int32_t basalt_world_section_read(int32_t world, int32_t chunk_x, int32_t section_y, int32_t chunk_z, int32_t flags,
                                  int32_t *out);

/* world_section_write: replaces one section from 4096 state ids. ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_section_write)
int32_t basalt_world_section_write(int32_t world, int32_t chunk_x, int32_t section_y, int32_t chunk_z,
                                   const int32_t *states, int32_t flags);

/* world_chunk_read: every section of a chunk, lowest first, 4096 ids each; the result is the number of i32 the full
 * answer needs (sections * 4096), written only when it fits `cap`. ABSENT as for sections. */
BASALT_IMPORT(world_chunk_read)
int32_t basalt_world_chunk_read(int32_t world, int32_t chunk_x, int32_t chunk_z, int32_t flags, int32_t *out,
                                int32_t cap);

/* world_chunk_apply: writes `count` whole sections of one chunk: `section_ys` lists the section indexes and `states`
 * holds 4096 ids per section (a negative id keeps the block). With BASALT_WORLD_RESEND the chunk is sent again. When
 * `previous` is not 0 it receives the 4096 ids that were there for each section. Returns the blocks changed, INVALID
 * for an index out of the world's height, ABSENT when the chunk is not loaded. Bulk. */
BASALT_IMPORT(world_chunk_apply)
int32_t basalt_world_chunk_apply(int32_t world, int32_t chunk_x, int32_t chunk_z, const int32_t *section_ys,
                                 int32_t count, const int32_t *states, int32_t flags, int32_t *previous);

/* world_changes_apply: writes states at position triples, then runs `effects` (BASALT_WORLD_EFFECT_*) for the blocks
 * that changed. Returns how many changed. Bulk. */
BASALT_IMPORT(world_changes_apply)
int32_t basalt_world_changes_apply(int32_t world, const int32_t *positions, const int32_t *states, int32_t count,
                                   int32_t effects);

/* world_blocks_settle: runs `effects` for blocks already written at position triples; `previous` (or 0) holds the
 * state each had before. Returns how many positions were loaded. */
BASALT_IMPORT(world_blocks_settle)
int32_t basalt_world_blocks_settle(int32_t world, const int32_t *positions, const int32_t *previous, int32_t count,
                                   int32_t effects);

/* world_blocks_notify: tells the block at each position triple that a neighbour changed; `sources` (or 0) holds the
 * position triple of the neighbour that did. Returns how many positions were loaded. */
BASALT_IMPORT(world_blocks_notify)
int32_t basalt_world_blocks_notify(int32_t world, const int32_t *positions, const int32_t *sources, int32_t count);

/* world_blocks_reshape: lets the blocks around each position triple reshape to fit it; BASALT_WORLD_INDIRECT widens
 * it. Returns how many positions were reached. */
BASALT_IMPORT(world_blocks_reshape)
int32_t basalt_world_blocks_reshape(int32_t world, const int32_t *positions, int32_t count, int32_t flags);

/* world_state_valid: the state id `state` would settle to if it were put at the position (a fence joining, stairs
 * turning), written nowhere. ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_state_valid)
int32_t basalt_world_state_valid(int32_t world, int32_t x, int32_t y, int32_t z, int32_t state);

/* world_tick_schedule: books a tick of the block (or its fluid with BASALT_WORLD_FLUID) `delay` ticks away. 1 when
 * booked, 0 when nothing there ticks. */
BASALT_IMPORT(world_tick_schedule)
int32_t basalt_world_tick_schedule(int32_t world, int32_t x, int32_t y, int32_t z, int32_t delay, int32_t flags);

/* world_tick_pending: 1 when a tick of the block at the position is already booked, else 0. */
BASALT_IMPORT(world_tick_pending) int32_t basalt_world_tick_pending(int32_t world, int32_t x, int32_t y, int32_t z);

/* world_block_entity_get: the block entity at the position as a named NBT compound (id, x, y, z and its data) in
 * bytes, needed-length convention. ABSENT when the position holds none. */
BASALT_IMPORT(world_block_entity_get)
int32_t basalt_world_block_entity_get(int32_t world, int32_t x, int32_t y, int32_t z, char *out, int32_t cap);

/* world_block_entity_edit: `op` 0 replaces the block entity data from the NBT compound, 1 creates one (empty NBT:
 * defaults), 2 removes it without touching the block (NBT ignored). ABSENT when the block owns no block entity or, for
 * 2, there was none. */
BASALT_IMPORT(world_block_entity_edit)
int32_t basalt_world_block_entity_edit(int32_t world, int32_t op, int32_t x, int32_t y, int32_t z, const char *nbt,
                                       int32_t nlen);

/* world_block_entities: the positions (flat triples) of the block entities of a loaded chunk; result is the number of
 * i32 the full list needs. ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_block_entities)
int32_t basalt_world_block_entities(int32_t world, int32_t chunk_x, int32_t chunk_z, int32_t *out, int32_t cap);

/* world_data_get: what plugins keep with the world as a named NBT compound in bytes. `scope` 0 is the world (x, y, z
 * ignored), 1 a loaded chunk (x, z are the chunk coordinates), 2 a block entity at x, y, z. ABSENT when there is none
 * or the chunk is not loaded. Needed-length convention. */
BASALT_IMPORT(world_data_get)
int32_t basalt_world_data_get(int32_t world, int32_t scope, int32_t x, int32_t y, int32_t z, char *out, int32_t cap);

/* world_data_set: replaces that data from an NBT compound. ABSENT as for `world_data_get` or when the NBT is rejected. */
BASALT_IMPORT(world_data_set)
int32_t basalt_world_data_set(int32_t world, int32_t scope, int32_t x, int32_t y, int32_t z, const char *nbt,
                              int32_t nlen);

/* ---- Chunks ------------------------------------------------------------------------------------------------------ */

/* world_chunk_status: 0 not loaded, 1 asked for and on its way, 2 loaded, 3 loaded and ticking. */
BASALT_IMPORT(world_chunk_status) int32_t basalt_world_chunk_status(int32_t world, int32_t chunk_x, int32_t chunk_z);

/* world_chunk_load: loads the chunk now (BASALT_WORLD_GENERATE creates a missing one) and returns 1 when it is loaded
 * afterwards, else 0. May take a long time for a generated chunk; prefer `world_chunk_request`. */
BASALT_IMPORT(world_chunk_load)
int32_t basalt_world_chunk_load(int32_t world, int32_t chunk_x, int32_t chunk_z, int32_t flags);

/* world_chunk_unload: saves and drops a chunk nobody uses. 1 when dropped, 0 when a player still sees it. */
BASALT_IMPORT(world_chunk_unload) int32_t basalt_world_chunk_unload(int32_t world, int32_t chunk_x, int32_t chunk_z);

/* world_chunk_request: asks for a chunk without waiting (BASALT_WORLD_GENERATE creates it, a lower `priority` goes
 * first) and returns a request handle. Later basalt_callback(token, ctx) runs on the world's owner and `value_result`
 * answers `true` when the chunk is loaded, `false` when it never was. Cancel with `request_cancel`. */
BASALT_IMPORT(world_chunk_request)
int32_t basalt_world_chunk_request(int32_t world, int32_t chunk_x, int32_t chunk_z, int32_t flags, int32_t priority,
                                   int32_t token);

/* world_chunks_loaded: the loaded chunks as flat (x, z) pairs; result is the number of i32 the full list needs. */
BASALT_IMPORT(world_chunks_loaded) int32_t basalt_world_chunks_loaded(int32_t world, int32_t *out, int32_t cap);

/* world_chunk_hold: keeps `count` chunks (flat pairs) loaded for the holder `tag` until released. ABSENT when the world does not support held chunks. */
BASALT_IMPORT(world_chunk_hold)
int32_t basalt_world_chunk_hold(int32_t world, const char *tag, int32_t tlen, const int32_t *chunks, int32_t count);

/* world_chunk_release: lets go of the chunks (flat pairs) the holder held; a negative `count` releases everything the
 * holder held. Returns OK. */
BASALT_IMPORT(world_chunk_release)
int32_t basalt_world_chunk_release(int32_t world, const char *tag, int32_t tlen, const int32_t *chunks, int32_t count);

/* world_chunk_held: the chunks (flat pairs) held for the holder `tag`. Result is the number of i32 the full list needs;
 * `world_call` "chunk_holders" answers the other way round, the holders of one chunk. */
BASALT_IMPORT(world_chunk_held)
int32_t basalt_world_chunk_held(int32_t world, const char *tag, int32_t tlen, int32_t *out, int32_t cap);

/* world_chunks_resend: sends the chunks (flat pairs) again to every player of the world near them. */
BASALT_IMPORT(world_chunks_resend) int32_t basalt_world_chunks_resend(int32_t world, const int32_t *chunks, int32_t count);

/* world_stored_get: the chunk exactly as stored on disk, a named NBT compound in bytes. ABSENT when the world has no
 * such chunk. Needed-length convention. */
BASALT_IMPORT(world_stored_get)
int32_t basalt_world_stored_get(int32_t world, int32_t chunk_x, int32_t chunk_z, char *out, int32_t cap);

/* world_stored_put: writes raw NBT as the stored chunk and flushes its region. ABSENT when the NBT is rejected or
 * the chunk cannot be replaced (it is loaded). Needs `worldgen`. */
BASALT_IMPORT(world_stored_put)
int32_t basalt_world_stored_put(int32_t world, int32_t chunk_x, int32_t chunk_z, const char *nbt, int32_t nlen);

/* world_stored_list: the chunks (flat pairs) stored in one region file. Result is the number of i32 the full list
 * needs. */
BASALT_IMPORT(world_stored_list)
int32_t basalt_world_stored_list(int32_t world, int32_t region_x, int32_t region_z, int32_t *out, int32_t cap);

/* ---- Light ------------------------------------------------------------------------------------------------------- */

/* world_light_at: the light 0 to 15 at the position, `layer` 0 block light and 1 sky light, brought up to date. ABSENT
 * when the chunk is not loaded. */
BASALT_IMPORT(world_light_at)
int32_t basalt_world_light_at(int32_t world, int32_t x, int32_t y, int32_t z, int32_t layer);

/* world_light_read: a section's light as 2048 bytes. ABSENT when the chunk is not loaded (and BASALT_WORLD_LOAD is not
 * set) or has no light yet. Needed-length convention. */
BASALT_IMPORT(world_light_read)
int32_t basalt_world_light_read(int32_t world, int32_t chunk_x, int32_t section_y, int32_t chunk_z, int32_t layer,
                                int32_t flags, char *out, int32_t cap);

/* world_light_write: replaces a section's light from 2048 bytes; it stands until blocks around change. INVALID for
 * another length. */
BASALT_IMPORT(world_light_write)
int32_t basalt_world_light_write(int32_t world, int32_t chunk_x, int32_t section_y, int32_t chunk_z, int32_t layer,
                                 const char *nibbles, int32_t nlen);

/* world_light_check: works the light of chunks (flat pairs) out again off the tick thread and sends them. With a
 * non-zero `token` it returns a request handle and later calls basalt_callback(token, ctx), where `value_result` is the
 * number of chunks that were loaded; with token 0 it returns OK and does not wait. */
BASALT_IMPORT(world_light_check)
int32_t basalt_world_light_check(int32_t world, const int32_t *chunks, int32_t count, int32_t token);

/* ---- Biomes ------------------------------------------------------------------------------------------------------ */

/* world_biome_get: the biome id at the position (`minecraft:plains`). ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_biome_get)
int32_t basalt_world_biome_get(int32_t world, int32_t x, int32_t y, int32_t z, char *out, int32_t cap);

/* world_biome_set: sets the biome of the 4x4x4 cell at the position. ABSENT for an unknown biome or unloaded chunk. */
BASALT_IMPORT(world_biome_set)
int32_t basalt_world_biome_set(int32_t world, int32_t x, int32_t y, int32_t z, const char *biome, int32_t blen);

/* world_biomes_read: a chunk's biomes as one i32 per cell, an index into `worlds_data("biomes")`. Result is the number
 * of i32 the full answer needs. ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_biomes_read)
int32_t basalt_world_biomes_read(int32_t world, int32_t chunk_x, int32_t chunk_z, int32_t *out, int32_t cap);

/* world_biomes_write: writes biome indexes in the same order (a negative index keeps the cell) and shows the chunk
 * again to its watchers. ABSENT when the chunk is not loaded. */
BASALT_IMPORT(world_biomes_write)
int32_t basalt_world_biomes_write(int32_t world, int32_t chunk_x, int32_t chunk_z, const int32_t *ids, int32_t count);

/* ---- Effects ----------------------------------------------------------------------------------------------------- */

/* world_sound: plays a sound id at a point to the players in range (16 blocks scaled by `volume`). `category` is one of
 * master, music, record, weather, block, hostile, neutral, player, ambient, voice, ui; empty means master. INVALID for
 * an unknown category. */
BASALT_IMPORT(world_sound)
int32_t basalt_world_sound(int32_t world, const char *sound, int32_t slen, const char *category, int32_t clen,
                           double x, double y, double z, float volume, float pitch, int64_t seed);

/* world_particle: shows a particle from a map {type (`minecraft:flame`), x, y, z, count? (1), offset_x, offset_y,
 * offset_z, speed (a number for all three axes) or speed_x, speed_y, speed_z, randomization? (0, 1, 2), force? (bool),
 * data? (list of numbers the particle's options read in order: a state id, a colour, a scale), item? (item id),
 * player? (name of the one player who sees it)}. INVALID for a missing type, ABSENT when the world has no effects. */
BASALT_IMPORT(world_particle) int32_t basalt_world_particle(int32_t world, const char *spec, int32_t slen);

/* world_level_event: a level event (a sound or burst of particles the client knows by number) at a block; with
 * BASALT_WORLD_GLOBAL it reaches every player. */
BASALT_IMPORT(world_level_event)
int32_t basalt_world_level_event(int32_t world, int32_t type, int32_t x, int32_t y, int32_t z, int32_t data,
                                 int32_t flags);

/* world_block_event: a block event (a chest lid, a bell, a note) sent to the players watching the block. */
BASALT_IMPORT(world_block_event)
int32_t basalt_world_block_event(int32_t world, int32_t x, int32_t y, int32_t z, int32_t first, int32_t second);

/* world_explode: an explosion of `power` at a point, from the entity id `source` (-1 none); BASALT_WORLD_BREAK and
 * BASALT_WORLD_FIRE gate block damage and fires. 1 when it happened. */
BASALT_IMPORT(world_explode)
int32_t basalt_world_explode(int32_t world, double x, double y, double z, float power, int32_t flags, int32_t source);

/* world_lightning: a lightning bolt at a point; returns the new bolt's entity id, or ABSENT when the spawn was
 * cancelled. */
BASALT_IMPORT(world_lightning) int32_t basalt_world_lightning(int32_t world, double x, double y, double z);

/* world_game_event: raises a game event (`minecraft:block_place`) that sculk sensors in range hear; `source` is an
 * entity uuid or empty. INVALID for an empty name. */
BASALT_IMPORT(world_game_event)
int32_t basalt_world_game_event(int32_t world, const char *event, int32_t elen, double x, double y, double z,
                                const char *source, int32_t slen);

/* world_player_effect: a view-only effect for one player (a sound, particle, level event, a block shown as another,
 * a sign text, the player's own time, weather or border). `player` is a core player handle. `op` is one of
 * sound_at, sound_entity, stop_sound, particle_at, world_event, set_player_time, reset_player_time,
 * set_player_weather, block_change, block_damage, sign_text, open_sign, game_event, border_set, border_reset,
 * block_entity, post_effects, set_post_effects, entity_event, and the read-only properties player_time,
 * player_time_relative, player_weather, view_distance, simulation_distance (empty `args` reads, one value writes). `args` is a list of values, the positional arguments of that op (for
 * example sound_at: sound, category, x, y, z, volume, pitch, seed). The answer is a value (null when the op answers
 * nothing). ABSENT for an op that is not in the list, INVALID for arguments of the wrong type, STALE for a player who
 * left. */
BASALT_IMPORT(world_player_effect)
int32_t basalt_world_player_effect(int32_t player, const char *op, int32_t olen, const char *args, int32_t alen,
                                   char *out, int32_t cap);

/* ---- Border ------------------------------------------------------------------------------------------------------ */

/* world_border_get: the border as a map {center_x, center_z, size, target_size, lerp_ticks, damage_amount,
 * damage_buffer, warning_distance, warning_time}. */
BASALT_IMPORT(world_border_get) int32_t basalt_world_border_get(int32_t world, char *out, int32_t cap);

/* world_border_set: applies the keys of that map that are present, plus `seconds` (f64): the size moves over that long
 * when positive. INVALID for a value out of range. */
BASALT_IMPORT(world_border_set) int32_t basalt_world_border_set(int32_t world, const char *spec, int32_t slen);

/* ---- Long tail by name ------------------------------------------------------------------------------------------- */

/* world_call: one operation of the world by name with a value argument and a value result, for what is too rare for an
 * import of its own. `args` is a map. `world_call(world, "ops", {})` returns the list of operation names the host
 * knows, generated from its table. Operations: save; use_item (x, y, z, face (0 to 5), item, yaw, pitch); survives
 * (x, y, z, state); bone_meal; break_block (x, y, z, tool?, enchantments?, drops?); drops (x, y, z, tool?, enchantments?,
 * by_entity?); signal (x, y, z, face, direct?); own_signal; analog_signal; fluid (x, y, z); fluid_flow; highest (x, z,
 * kind: surface, ocean_floor or motion_blocking); random_tick; grow_tree (x, y, z, sapling); place_tree; place_feature
 * and feature_ids; pois (types?, x, y, z, radius, occupancy); climate (x, y, z, in quarts); humidity and temperature;
 * beacon_scan; lightning_target (x, z); raids, raid_control, raid_bar, set_raid; sculk, hive, vault, creaking_heart;
 * dragon_fight, dragon_fight_action, dragon_fight_bar, place_end_gateway; random_sequence (id, kind, argument);
 * inhabited_time (chunk_x, chunk_z, value?); force_chunk (chunk_x, chunk_z, forced?); move_to_time_marker (marker);
 * chunk_holders (chunk_x, chunk_z, all?); break_speed (player name, x, y, z); structures (chunk_x, chunk_z; the structure
 * starts of a loaded chunk); locate_structure (structure, x, z, cells?, skip_existing?); locate_biome (biomes, x, y, z,
 * radius?, step?). Result: the length of the encoded value the
 * full answer needs. ABSENT for an unknown operation, INVALID for a bad argument, DENIED for an operation that needs a
 * capability. */
BASALT_IMPORT(world_call)
int32_t basalt_world_call(int32_t world, const char *op, int32_t olen, const char *args, int32_t alen, char *out,
                          int32_t cap);

/* ---- Structures -------------------------------------------------------------------------------------------------- */

/* structure_list: a value list with the id of every structure template (game data, saved and in memory). */
BASALT_IMPORT(structure_list) int32_t basalt_structure_list(char *out, int32_t cap);

/* structure_get: the template as an unnamed NBT compound in the structure file format, in bytes. ABSENT for an id that
 * is not known. */
BASALT_IMPORT(structure_get)
int32_t basalt_structure_get(const char *id, int32_t ilen, char *out, int32_t cap);

/* structure_put: keeps the template under the id (and in the world's generated folder with BASALT_WORLD_DISK).
 * INVALID for malformed NBT. */
BASALT_IMPORT(structure_put)
int32_t basalt_structure_put(const char *id, int32_t ilen, const char *nbt, int32_t nlen, int32_t flags);

/* structure_remove: forgets the template (and deletes its generated file with BASALT_WORLD_DISK). ABSENT when it was
 * not there. */
BASALT_IMPORT(structure_remove) int32_t basalt_structure_remove(const char *id, int32_t ilen, int32_t flags);

/* structure_file: the path of the generated file that holds the template, as text (the id when it is only in memory).
 * ABSENT when the id is not known. */
BASALT_IMPORT(structure_file)
int32_t basalt_structure_file(const char *id, int32_t ilen, char *out, int32_t cap);

/* structure_capture: the blocks (and with BASALT_WORLD_ENTITIES the entities) of a box as a template, written to
 * `out` as an unnamed NBT compound. `world` names the world through its handle. ABSENT when the box is not loaded. */
BASALT_IMPORT(structure_capture)
int32_t basalt_structure_capture(int32_t world, int32_t x, int32_t y, int32_t z, int32_t size_x, int32_t size_y,
                                 int32_t size_z, int32_t flags, char *out, int32_t cap);

/* structure_scan: the blocks and jigsaws of a template as a placement would put them, with the world left alone.
 * `spec` is a map {x, y, z, rotation (0 to 3), mirror (0 to 2), pivot_x, pivot_z, block? (only this block id),
 * absolute? (bool)}. The result is a map {size: [x, y, z], blocks: [{x, y, z, state, nbt (bytes)}], jigsaws: [{x, y,
 * z, front, top, rollable, name, pool, target, final_state, placement_priority, selection_priority}]}. */
BASALT_IMPORT(structure_scan)
int32_t basalt_structure_scan(const char *nbt, int32_t nlen, const char *spec, int32_t slen, char *out, int32_t cap);

/* structure_place: places a template given as NBT; `spec` is a map {world (name), x, y, z, rotation, mirror,
 * integrity? (f64), seed? (i64), entities? (bool), palette? (-1 picks at random)}. The loaded chunks it covers change.
 * ABSENT when the placement failed, INVALID for malformed NBT or a bad spec. */
BASALT_IMPORT(structure_place)
int32_t basalt_structure_place(const char *nbt, int32_t nlen, const char *spec, int32_t slen);

/* ---- Data fixing ------------------------------------------------------------------------------------------------- */

/* datafix_call: the native data fixer. `op` is one of supports (type; answers a bool), types (answers a list of
 * type names), steps (type; answers [{version, sub_version, name}]), missing (type, from, to; answers the first step
 * that cannot run, or an empty string), legacy_state (id; the 1.13 {name, properties} of a `block << 4 | data` id),
 * modern_state (id; the current {name, properties}), upgrade (type, nbt (bytes), from, to; answers the fixed NBT as
 * bytes) and step (type, name, version_key (version * 10 + sub), nbt; answers the fixed NBT as bytes). `args` is a map
 * with those keys. ABSENT for an unknown op or type, INVALID for a bad argument or unreadable NBT. */
BASALT_IMPORT(datafix_call)
int32_t basalt_datafix_call(const char *op, int32_t olen, const char *args, int32_t alen, char *out, int32_t cap);

/* ---- Noise and world generation data ----------------------------------------------------------------------------- */

/* noise_query: evaluates terrain data for a seed without generating a chunk. `spec` is a map {op, settings
 * (`minecraft:overworld`), seed (i64), ...}. ops: shape (answers {min_y, height, sea_level, default_block,
 * default_fluid, legacy_random_source, disable_mob_generation}); density (function: `final_density`, `continents`,
 * `erosion`, `depth`, `ridges`, `temperature`, `vegetation`; x, y, z; answers a float); density_tree (json, x, y, z);
 * noise (id, x, y, z as floats); noise_range (id; answers [min, max]); height (x, z, ocean_floor?; answers an int);
 * column (x, z; answers bytes of i32 state ids, bottom first). ABSENT when the settings, function or noise are unknown. */
BASALT_IMPORT(noise_query)
int32_t basalt_noise_query(const char *spec, int32_t slen, char *out, int32_t cap);

/* noise_create: builds a noise and returns its handle (valid for the plugin; STALE after `noise_release`). `spec` is a
 * map {id? (a game data noise such as `minecraft:jagged`) or parameters? {base_amplitude, base_octave, octave_count,
 * normalize (0 none, 1 on, 2 legacy), amplitudes (list)}, seed: {legacy (bool), low (i64), high (i64), nether?
 * (bool)}}. ABSENT for an unknown id. */
BASALT_IMPORT(noise_create) int32_t basalt_noise_create(const char *spec, int32_t slen);

/* noise_sample: the noise value at a point written to `out`. STALE for a released handle. */
BASALT_IMPORT(noise_sample) int32_t basalt_noise_sample(int32_t noise, double x, double y, double z, double *out);

/* noise_info: the smallest and largest value the noise can return, as a value [min, max]. */
BASALT_IMPORT(noise_info) int32_t basalt_noise_info(int32_t noise, char *out, int32_t cap);

/* noise_release: drops the noise; its handle turns STALE. */
BASALT_IMPORT(noise_release) int32_t basalt_noise_release(int32_t noise);

/* ---- Tables of the game data --------------------------------------------------------------------------------------- */

/* block_state_id: the state id of text such as `minecraft:oak_stairs[facing=east]`; ABSENT for text that is not one. */
BASALT_IMPORT(block_state_id) int32_t basalt_block_state_id(const char *text, int32_t tlen);

/* block_state_text: the text of a state id; ABSENT for an id that is not a state. Needed-length convention. */
BASALT_IMPORT(block_state_text) int32_t basalt_block_state_text(int32_t state, char *out, int32_t cap);

/* worlds_data: read-only tables of the game data by name, generated from the server's registries. `query` is the
 * table, `args` is a value (a map, or null) with what it needs. Tables: blocks (list of {id, first_state,
 * state_count}); block_properties (block: id; list of {name, values}); state_flags (state; an int of bits: 1 blocks
 * motion, 2 solid render, 4 full cube, 8 replaceable, 16 has fluid, 32 has a collision shape, 64 conducts redstone,
 * 128 ticks randomly, 256 is air, bits 9 to 12 the light emission, bits 13 to 16 the sky light opacity, 262144 has a
 * block entity); block_hardness (block); block_info (state; a list of 11 numbers, not a map: friction, speed factor,
 * jump factor, push reaction (0 normal, 1 destroy, 2 block, 3 ignore, 4 push only), map colour as 0xRRGGBB, ignite
 * odds, burn odds, note block instrument (an index into the instruments of the game data), ignited by lava, signal
 * source, analog output (the last three 0 or 1); an empty list for an unknown state); block_shape (state; collision
 * boxes); block_outline (state, x, z); block_visual (state); block_entity_types; block_sounds (state);
 * block_instrument (state); tool_facts (item?, state); transform_state (state, rotation 0 to 3, mirror 0 to 2);
 * poi_type (state); poi_tickets (type); game_rules (list of {id, type, default}); game_event_frequency (event);
 * data_version; fix_nbt (kind, nbt bytes, from_version); fix_name (kind, text, from_version); biomes (list of ids in
 * registry order); items; entity_types; tags (registry); registry (registry; list of ids); registry_data (registry,
 * id; the entry as NBT bytes); `tables` lists the names above. ABSENT for a table or entry the server does not have,
 * INVALID for missing arguments. Result: the length of the encoded value the full answer needs. */
BASALT_IMPORT(worlds_data)
int32_t basalt_worlds_data(const char *query, int32_t qlen, const char *args, int32_t alen, char *out, int32_t cap);

#ifdef __cplusplus
}
#endif
#endif
