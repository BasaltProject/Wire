#ifndef BASALT_AREA_ENTITIES_H
#define BASALT_AREA_ENTITIES_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Entities: spawning, finding, moving and driving every non-player entity, and the player as an entity.
 *
 * An `entity` handle is a positive i32 for one live entity or player. The plugin gets it from a spawn, a lookup, a
 * query or entity_of_player, and it lives until the entity leaves the world (death, removal, unloaded chunk, the
 * player logging out) or the plugin calls entity_release. After that every import but entity_alive and
 * entity_release answers STALE for it; a slot is never reused under the same handle value. The same entity found
 * twice answers the same handle. At most 65535 handles are live per plugin: past that, lookups and spawns answer
 * INVALID until entities die or handles are released.
 *
 * Other entities are named by their numeric id (what events and the "vehicle", "passengers", "target", "leash_holder"
 * properties carry, -1 for none): turn one into a handle with entity_by_id. A world is always its name, never a
 * handle. Players and their handles belong to the core; entity_of_player and entity_player convert between the two.
 *
 * Thread: every import reads or changes live world state and runs on the plugin thread (basalt_abi.h), inside a task,
 * a command, an event or a callback of this plugin. Imports that return a length follow the usual needed-length rule.
 *
 * Property names (entity_get, entity_set) and operation names (entity_call) are the server's own tables; list them with
 * entity_properties and entity_operations. A name the entity does not know answers ABSENT. Values use the encoding of
 * basalt_values.h: booleans, integers and floats as they are, strings as strings, nested records as maps, and the
 * "bytes" tag for NBT. Entity ids stay integers.
 */

/* ---- Handles ---------------------------------------------------------------------------------------------- */

/* entity_of_player: the entity handle of an online player (the same player always gives the same handle). Arguments:
 * a player handle. Result: the entity handle, STALE for a player that left, INVALID for a malformed handle. */
BASALT_IMPORT(entity_of_player) int32_t basalt_entity_of_player(int32_t player);

/* entity_player: the player handle of an entity that is a player. Result: the player handle, ABSENT when the entity
 * is not a player, STALE for a gone entity. */
BASALT_IMPORT(entity_player) int32_t basalt_entity_player(int32_t entity);

/* entity_release: forgets a handle so its slot can be reused. OK, or STALE when it was not live. The entity itself is
 * untouched. */
BASALT_IMPORT(entity_release) int32_t basalt_entity_release(int32_t entity);

/* entity_alive: 1 while the entity is in the world, 0 once it left (also for a handle kept past its death); INVALID
 * for a malformed handle. Never answers STALE. */
BASALT_IMPORT(entity_alive) int32_t basalt_entity_alive(int32_t entity);

/* entity_id: the numeric id the world and the events use for this entity. Result: the id (a positive i32), STALE or
 * INVALID. */
BASALT_IMPORT(entity_id) int32_t basalt_entity_id(int32_t entity);

/* ---- Finding ------------------------------------------------------------------------------------------------ */

/* entity_by_id: the entity with that numeric id in a world. Arguments: world name, id. Result: the handle, ABSENT when
 * no live entity of that world has the id, ABSENT for an unknown world. */
BASALT_IMPORT(entity_by_id) int32_t basalt_entity_by_id(const char *world, int32_t wlen, int32_t id);

/* entity_by_uuid: the live entity with that UUID text in any world. Result: the handle, ABSENT, INVALID for a text
 * that is not a UUID. */
BASALT_IMPORT(entity_by_uuid) int32_t basalt_entity_by_uuid(const char *uuid, int32_t ulen);

/* entity_query: the entities of a world that match a query, as an array of i32 handles written to `out` (count
 * entries fit in `cap` entries; the result is the number of handles, written only when they fit). Query value, all
 * keys optional (an empty query, len 0, matches every entity of the world): "type" (an entity type id, or a list of
 * them; "minecraft:" may be left out), "x" "y" "z" with "radius" (a sphere; all four needed), "min_x" "min_y" "min_z"
 * "max_x" "max_y" "max_z" (a box; all six needed), "chunk_x" "chunk_z" (the entities of that chunk, both needed),
 * "limit" (at most that many). Result: the count, ABSENT for an unknown world, INVALID for a bad query. */
BASALT_IMPORT(entity_query)
int32_t basalt_entity_query(const char *world, int32_t wlen, const char *query, int32_t qlen, int32_t *out, int32_t cap);

/* entity_types: every entity type id the server knows, as a value list of strings. Needed-length result. */
BASALT_IMPORT(entity_types) int32_t basalt_entity_types(char *out, int32_t cap);

/* entity_type_attributes: the attributes a new living entity of the type starts with, as a value map from attribute id
 * to its base value (an empty map for a type that is not living). Result: the needed length, INVALID for an unknown
 * type. */
BASALT_IMPORT(entity_type_attributes)
int32_t basalt_entity_type_attributes(const char *type, int32_t tlen, char *out, int32_t cap);

/* ---- Spawning and removing --------------------------------------------------------------------------------- */

/* entity_spawn: puts a new entity of `type` in the world at x, y, z. Options value (len 0 for none): "yaw" (f64,
 * default 0), "reason" (string for the entity.spawn event, default "plugin"). Result: the handle; ABSENT when the
 * entity.spawn event was cancelled or the server refused the spawn; INVALID for an unknown world or type. */
BASALT_IMPORT(entity_spawn)
int32_t basalt_entity_spawn(const char *world, int32_t wlen, const char *type, int32_t tlen, double x, double y, double z,
                            const char *opts, int32_t olen);

/* entity_spawn_nbt: brings back an entity from named NBT bytes (what entity_snapshot gave). `cause` is the reason the
 * entity.spawn event reports (empty means "plugin"). Result: the handle, ABSENT when a listener cancelled it, INVALID
 * for bad NBT or an unknown world. */
BASALT_IMPORT(entity_spawn_nbt)
int32_t basalt_entity_spawn_nbt(const char *world, int32_t wlen, const char *nbt, int32_t nlen, const char *cause,
                                int32_t clen);

/* entity_drop_item: puts an item on the ground with a velocity. The item is a value map: "id" (string, required) and
 * "count" (default 1), or "nbt" (the bytes of a whole saved stack, which replaces the other keys). Result: the handle of
 * the item entity, ABSENT when nothing was dropped, INVALID for a bad item or world. */
BASALT_IMPORT(entity_drop_item)
int32_t basalt_entity_drop_item(const char *world, int32_t wlen, const char *item, int32_t ilen, double x, double y, double z,
                                double vx, double vy, double vz);

/* entity_remove: takes the entity out of the world (a player is not removed: INVALID). OK, STALE. */
BASALT_IMPORT(entity_remove) int32_t basalt_entity_remove(int32_t entity);

/* entity_remove_uuid: takes the entity with that UUID out of the world. OK, ABSENT when no live entity has it,
 * INVALID for a text that is not a UUID or an unknown world. */
BASALT_IMPORT(entity_remove_uuid) int32_t basalt_entity_remove_uuid(const char *world, int32_t wlen, const char *uuid, int32_t ulen);

/* ---- Transform and motion ---------------------------------------------------------------------------------- */

/* entity_position: writes x, y, z as three f64 to `out` (24 bytes). OK, STALE, INVALID for a bad pointer. */
BASALT_IMPORT(entity_position) int32_t basalt_entity_position(int32_t entity, double *out);

/* entity_teleport: moves the entity to x, y, z. Options value (len 0 for none): "yaw" and "pitch" (f64, kept when
 * absent), "world" (a world name; a different world is moved to only for players, any other entity answers INVALID).
 * OK, STALE, INVALID, ABSENT for an unknown world, DENIED when a plugin event handler refused the teleport. */
BASALT_IMPORT(entity_teleport)
int32_t basalt_entity_teleport(int32_t entity, double x, double y, double z, const char *opts, int32_t olen);

/* entity_velocity: writes the velocity (blocks per tick) as three f64 to `out` (24 bytes). OK, STALE, INVALID for a
 * player (a player's motion belongs to its client). */
BASALT_IMPORT(entity_velocity) int32_t basalt_entity_velocity(int32_t entity, double *out);

/* entity_set_velocity: replaces the velocity. OK, STALE, INVALID when the entity cannot be pushed. */
BASALT_IMPORT(entity_set_velocity) int32_t basalt_entity_set_velocity(int32_t entity, double x, double y, double z);

/* entity_rotation: writes yaw and pitch in degrees as two f64 to `out` (16 bytes). OK, STALE, INVALID. */
BASALT_IMPORT(entity_rotation) int32_t basalt_entity_rotation(int32_t entity, double *out);

/* entity_set_rotation: turns the entity. OK, STALE, INVALID when it cannot be turned. */
BASALT_IMPORT(entity_set_rotation) int32_t basalt_entity_set_rotation(int32_t entity, double yaw, double pitch);

/* ---- Properties and operations by name --------------------------------------------------------------------- */

/* entity_get: one property by name, written as a value. Besides the table of entity_properties these base names
 * always exist: "id" "type" "world" "uuid" "alive" "x" "y" "z" "yaw" "pitch" "health" "on_ground" "age" "width"
 * "height", "pos" and "velocity" (maps with keys x, y, z); "custom_name" and the rest are in the table. A property that
 * does not apply to this kind of entity reads as null. Result: the needed length, ABSENT for an unknown name, STALE,
 * INVALID. */
BASALT_IMPORT(entity_get) int32_t basalt_entity_get(int32_t entity, const char *name, int32_t nlen, char *out, int32_t cap);

/* entity_set: writes one property by name from a value (a number, bool, string or list of strings as the property
 * wants; "velocity" takes a map with x, y, z). Of the base names "yaw", "pitch" and "velocity" are writable, and so is
 * "health" for every living entity; "custom_name" is writable through the table of entity_properties. OK, ABSENT for
 * an unknown name, INVALID for a read-only property or a value the entity refuses, STALE. */
BASALT_IMPORT(entity_set) int32_t basalt_entity_set(int32_t entity, const char *name, int32_t nlen, const char *value, int32_t vlen);

/* entity_call: runs one named operation (damage, kill, add_effect, attribute, equipment, mount, move_to, find_path,
 * add_goal, brain_set_memory, merge_nbt, ...; see entity_operations). `args` is a value list of the operation's
 * arguments in order (len 0 for none); the result is the operation's value written to `out`: a bool, number, string,
 * bytes (NBT), a list, or for operations that return rows a list of maps keyed by the columns of entity_operations.
 * Results that are ids (passengers, path nodes) are integers. Result: the needed length, ABSENT for an unknown
 * operation or one the entity cannot do, STALE, INVALID for a bad argument list. */
BASALT_IMPORT(entity_call)
int32_t basalt_entity_call(int32_t entity, const char *op, int32_t olen, const char *args, int32_t alen, char *out, int32_t cap);

/* entity_properties: the property names an entity answers, as a value list of maps with "name" (string) and
 * "writable" (bool), built from the server's real tables for this kind of entity (a player or any other entity).
 * Needed-length result, STALE, INVALID. */
BASALT_IMPORT(entity_properties) int32_t basalt_entity_properties(int32_t entity, char *out, int32_t cap);

/* entity_operations: the operation names entity_call accepts for this entity, as a value list of maps with "name"
 * (string) and "columns" (a list of strings, empty unless the operation returns rows). Needed-length result, STALE,
 * INVALID. */
BASALT_IMPORT(entity_operations) int32_t basalt_entity_operations(int32_t entity, char *out, int32_t cap);

/* ---- Saved form --------------------------------------------------------------------------------------------- */

/* entity_snapshot: the entity as the named NBT compound the world would save (id, Pos, Rotation, Motion, UUID, ...),
 * written as raw bytes (not a value). Needed-length result, STALE, INVALID for a player. */
BASALT_IMPORT(entity_snapshot) int32_t basalt_entity_snapshot(int32_t entity, char *out, int32_t cap);

/* entity_restore: applies saved NBT bytes to the live entity in place, as /data merge entity does. OK, STALE,
 * INVALID for a player or malformed NBT. */
BASALT_IMPORT(entity_restore) int32_t basalt_entity_restore(int32_t entity, const char *nbt, int32_t nlen);

/* ---- World spawn rules and pure mechanics ------------------------------------------------------------------- */

/* entity_spawn_rules: reads and optionally changes the natural spawning rules of a world. `rules` is a value (len 0 to
 * only read) with optional keys "limits" and "ticks_per" (maps from a mob category to an integer; a negative value
 * restores vanilla; categories monster, creature, ambient, axolotls, underground_water_creature, water_creature,
 * water_ambient, misc), "allow_monsters" and "allow_animals" (bool). The result map written to `out` holds the same
 * keys with what a plugin changed (categories left at vanilla are left out). Needed-length result, ABSENT for an
 * unknown world, INVALID for a bad value. */
BASALT_IMPORT(entity_spawn_rules)
int32_t basalt_entity_spawn_rules(const char *world, int32_t wlen, const char *rules, int32_t rlen, char *out, int32_t cap);

/* entity_mechanic: one of the server's stateless entity formulas by name, so plugins get vanilla's exact numbers. The
 * arguments are a value map and the answer a value, both per name:
 *   combat.after_armor {damage, armor, toughness, fraction_add?} -> f64
 *   combat.after_protection {damage, protection} -> f64
 *   combat.after_resistance {damage, amplifier} -> f64
 *   combat.after_difficulty {damage, difficulty} -> f64 ("peaceful", "easy", "normal", "hard")
 *   combat.scales_with_difficulty {scaling, caused_by_living_non_player?} -> bool
 *   combat.absorb {damage, absorption} -> {health, absorption}
 *   combat.fall_damage {distance, safe?} -> i64
 *   breeding.offspring_attribute {a, b, min, max, first, second, third} -> f64
 *   physics.clip {box, from, to} -> point or null; physics.collide {colliders, box, movement} -> point
 *   physics.step_up {colliders, grounded, grounded_offset_y, movement, max_up_step, reach_squared} -> point or null
 *   physics.rotate_towards_movement {motion, x_rot_o, y_rot_o, x_rot, y_rot, speed} -> map or null
 *   physics.closest_hit {candidates, from, to, margin} -> {index, point} or null
 *   physics.picked_hit {candidates, root, from, to, max} -> {index, point} or null
 *   physics.projectile_margin {tick_count} -> f64; physics.leash_angular_friction {on_ground, block_friction?, in_liquid?}
 *   physics.shulker_box {size, direction, progress_from, progress_to, position} -> box
 *   physics.leash_pull {self, holder, quad?, slack?, self_motion, holder_motion, known_motion} -> {torque, push} or null
 * A point is a list of three f64, a box a map with min_x min_y min_z max_x max_y max_z, a collider a box map plus
 * "shape" (i64), a candidate a box map plus "pick_radius", "pick_from_inside" and "root", a leash body a map with
 * position, width, height and yaw. Index results are 1-based. Needed-length result, ABSENT for an unknown name,
 * INVALID for a bad argument. Callable on any thread. */
BASALT_IMPORT(entity_mechanic)
int32_t basalt_entity_mechanic(const char *name, int32_t nlen, const char *args, int32_t alen, char *out, int32_t cap);

/* ---- Goals and behaviours ------------------------------------------------------------------------------------ */

/* Mob goals and brain behaviours that a plugin owns are added with entity_call ("add_goal" key, priority, flags and
 * "brain_add_behavior" activity, priority, key) and decided in a callback registered with event_on("entity.goal", ...)
 * and event_on("entity.behavior", ...). The goal `flags` are one string of the names move, look, jump and target
 * separated by commas, matched without regard to case; any other word is ignored. Inside the callback,
 * event_field_str(ctx, "key") and event_field_str(ctx, "phase") ("can_use", "can_continue", "start", "stop", "tick")
 * say what is asked, and the two imports below read the mob and answer. At most 24 calls per mob per tick reach a
 * plugin, and a callback that does not answer means false. */

/* entity_event_entity: the handle of the entity an entity.* event is about (its "entity_id" field). Result: the
 * handle, INVALID for a ctx that is not an event with an entity, ABSENT when that entity is gone, STALE once the
 * callback returned. */
BASALT_IMPORT(entity_event_entity) int32_t basalt_entity_event_entity(int32_t ctx);

/* entity_ai_answer: answers the goal or behaviour question of this ctx (can_use, can_continue; the other phases
 * ignore it). OK, INVALID for any other ctx, STALE once the callback returned. */
BASALT_IMPORT(entity_ai_answer) int32_t basalt_entity_ai_answer(int32_t ctx, int32_t result);

#ifdef __cplusplus
}
#endif
#endif
