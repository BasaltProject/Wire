#ifndef BASALT_AREA_ITEMS_H
#define BASALT_AREA_ITEMS_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Area items: item stacks, inventories, recipes, registries of items and container menus.
 *
 * Item stack value (a map, see basalt_values.h). Every import that reads a stack accepts it; unknown keys are ignored.
 *   "id"           string   item id, `minecraft:` prefix optional on input. Required, empty or missing is INVALID
 *   "count"        int      default 1, clamped to 1..99
 *   "name"         string   custom name as MiniMessage text
 *   "lore"         list     of strings, one per line
 *   "damage"       int      durability already used, default 0
 *   "enchantments" map      enchantment id to level
 *   "components"   map      item component id (`minecraft:food`) to its SNBT text; the host parses and validates it,
 *                           an unparsable text is INVALID. This is where every other data component lives
 * A slot that holds nothing is the null value. Stacks the host writes carry "id" and "count" always and the other keys
 * only when they are set ("damage" above 0), plus every component the stack has under "components".
 *
 * Inventory handles. An inventory handle is a positive i32 made by one of the inventory_of_* imports; every one of
 * those returns a new handle, or STALE/ABSENT/INVALID as documented. The handle stays valid until inventory_release
 * or until the plugin is disabled. It becomes STALE (the use returns STALE) when what it points at is gone: the
 * player left, the window closed, the block entity was removed. Slot numbers by kind:
 *   player   0..26 storage, 27..35 hotbar (selected slot = 27 + held slot), 36..39 armour feet to head, 40 off hand
 *   block    the slot of the container block entity (chest 0..26, hopper 0..4, furnace 0..2, ...), one block entity
 *            only (half of a double chest)
 *   ender    0..26, the player's ender chest
 *   window   the protocol numbers of the window the player has open; slot -1 is the item on the cursor
 * No other entity has an inventory handle: inventory_of_entity only accepts a player entity and gives the player kind.
 * All inventory imports run on the plugin thread.
 *
 * Other areas' handles: `player` is the v1 player handle and `entity` the handle defined by entities.h. */

/* inventory_of_player: the inventory of an online player. Returns a handle, STALE for a player that left, INVALID
 * for a bad player handle. Never needs a capability. */
BASALT_IMPORT(inventory_of_player) int32_t basalt_inventory_of_player(int32_t player);

/* inventory_of_block: the container block entity at (x, y, z) of the world named `world` (empty means the default
 * world). Returns a handle; ABSENT when the world is unknown or the block holds no container, INVALID for bad text. */
BASALT_IMPORT(inventory_of_block)
int32_t basalt_inventory_of_block(const char *world, int32_t wlen, int32_t x, int32_t y, int32_t z);

/* inventory_of_window: the container window the player has open right now (including their own inventory slots, as
 * in the protocol). Returns a handle; ABSENT when no window is open or the connection has none (Bedrock), STALE or
 * INVALID for the player. The handle goes STALE when that window closes. */
BASALT_IMPORT(inventory_of_window) int32_t basalt_inventory_of_window(int32_t player);

/* inventory_of_ender: the ender chest of an online player (27 slots). Returns a handle, STALE or INVALID for the player. */
BASALT_IMPORT(inventory_of_ender) int32_t basalt_inventory_of_ender(int32_t player);

/* inventory_of_entity: the inventory of a player entity (`entity` handle of entities.h, from entity_of_player); it
 * is the same handle kind as inventory_of_player, with the player slot numbers above. Only a player entity works: for
 * any other entity (a minecart, a horse, a villager) it answers ABSENT. STALE for a removed entity, INVALID for a bad
 * handle. The player operations send_equipment and open_trades and the properties slot_limit and enchantment_seed are
 * reached with entity_call and entity_get/entity_set on the same entity handle. */
BASALT_IMPORT(inventory_of_entity) int32_t basalt_inventory_of_entity(int32_t entity);

/* inventory_release: forgets the handle. OK, STALE when it was already released, INVALID when malformed. */
BASALT_IMPORT(inventory_release) int32_t basalt_inventory_release(int32_t inventory);

/* inventory_size: the number of slots (window: the slots of the whole window). STALE or INVALID for the handle. */
BASALT_IMPORT(inventory_size) int32_t basalt_inventory_size(int32_t inventory);

/* inventory_get: the stack in `slot` as an item value written to `out` (needed-length convention); an empty slot is
 * the null value (1 byte). INVALID for a slot out of range, STALE or INVALID for the handle. */
BASALT_IMPORT(inventory_get) int32_t basalt_inventory_get(int32_t inventory, int32_t slot, char *out, int32_t cap);

/* inventory_set: puts the item value (or null to empty the slot) in `slot` and resends the inventory to whoever has
 * it open. OK; INVALID for a bad slot, bad item value or unknown item id, STALE or INVALID for the handle. */
BASALT_IMPORT(inventory_set)
int32_t basalt_inventory_set(int32_t inventory, int32_t slot, const char *item, int32_t ilen);

/* inventory_add: puts the item value where it fits, merging stacks first. Returns the count that did NOT fit (0 means
 * all of it went in); negative for INVALID (bad item), STALE or INVALID (handle). */
BASALT_IMPORT(inventory_add) int32_t basalt_inventory_add(int32_t inventory, const char *item, int32_t ilen);

/* inventory_remove: takes up to `count` of item `id` (any components). Returns how many were taken (0 when none);
 * negative for INVALID (count below 1 or empty id), STALE or INVALID (handle). */
BASALT_IMPORT(inventory_remove)
int32_t basalt_inventory_remove(int32_t inventory, const char *id, int32_t idlen, int32_t count);

/* inventory_count: how many of item `id` the inventory holds in total. Negative for STALE or INVALID. */
BASALT_IMPORT(inventory_count) int32_t basalt_inventory_count(int32_t inventory, const char *id, int32_t idlen);

/* inventory_clear: empties every slot (for a window only the container part). OK, STALE or INVALID. */
BASALT_IMPORT(inventory_clear) int32_t basalt_inventory_clear(int32_t inventory);

/* inventory_contents: every non-empty slot as a list of maps {"slot": int, "item": item value}, in slot order, written
 * to `out` (needed-length convention). STALE or INVALID for the handle. */
BASALT_IMPORT(inventory_contents) int32_t basalt_inventory_contents(int32_t inventory, char *out, int32_t cap);

/* inventory_held_slot: the selected hotbar index 0..8 of a player inventory. ABSENT for any other kind, STALE or
 * INVALID for the handle. */
BASALT_IMPORT(inventory_held_slot) int32_t basalt_inventory_held_slot(int32_t inventory);

/* inventory_set_held_slot: selects hotbar index 0..8 on a player inventory. OK; INVALID for an index out of range,
 * ABSENT for another kind, STALE or INVALID for the handle. */
BASALT_IMPORT(inventory_set_held_slot) int32_t basalt_inventory_set_held_slot(int32_t inventory, int32_t slot);

/* item_cooldown: ticks left on the use cooldown of item `id` for the player, 0 when none. INVALID for empty id,
 * STALE or INVALID for the player. */
BASALT_IMPORT(item_cooldown) int32_t basalt_item_cooldown(int32_t player, const char *id, int32_t idlen);

/* item_set_cooldown: starts (ticks above 0) or clears (0 or less) the use cooldown of item `id` for the player.
 * OK; INVALID for empty id, STALE or INVALID for the player. */
BASALT_IMPORT(item_set_cooldown)
int32_t basalt_item_set_cooldown(int32_t player, const char *id, int32_t idlen, int32_t ticks);

/* registry_get: reads item related registry data by kind into `out` as a value (needed-length convention).
 * `kind` is one of (the argument `key` is the text after it):
 *   "item"           key = item id: map of what the registries know (max_stack_size, use_cooldown_ticks, max_damage,
 *                    burn_ticks, and when they apply block, crafting_remainder, food, armor, weapon, tool, rarity,
 *                    enchantable, repairable, composter_chance)
 *   "items"          key ignored: list of every item id in registry order
 *   "creative_tab"   key = tab (`building_blocks`): list of item ids in display order, empty for tabs without a list
 *   "enchantment"    key = enchantment id: the datapack definition as a map
 *   "enchantments"   key ignored: list of every enchantment id
 *   "tags"           key = registry: map of tag id to list of member ids. Only `enchantment` has data; any other
 *                    registry answers an empty map
 *   "mob_effects"    key ignored: list of maps {id, protocol_id, color}
 *   "potions"        key ignored: list of maps {id, effects}, each effect {id, amplifier, duration}
 * ABSENT for an "item" id that is malformed (a well formed id the server does not know answers the defaults) and for
 * an unknown "enchantment"; INVALID for an unknown kind. */
BASALT_IMPORT(registry_get)
int32_t basalt_registry_get(const char *kind, int32_t klen, const char *key, int32_t keylen, char *out, int32_t cap);

/* enchantment_level_value: evaluates an enchantment level based number at `level`. `spec` is a value: a bare number,
 * or a map with "type" (constant, linear, levels_squared, clamped, fraction, exponent, lookup) and that type's
 * fields as in the datapack format ("value", "base", "per_level_above_first", "added", "min", "max", "numerator",
 * "denominator", "power", "values", "fallback"). Returns the number; 0.0 for a malformed spec. */
BASALT_IMPORT(enchantment_level_value)
double basalt_enchantment_level_value(const char *spec, int32_t slen, int32_t level);

/* recipe_add: registers a recipe from the datapack recipe JSON text. OK; INVALID when the id is taken, the id is
 * malformed or the JSON is not a recipe. Types: crafting_shaped, crafting_shapeless, crafting_transmute, smelting,
 * blasting, smoking, campfire_cooking, stonecutting, smithing_transform, smithing_trim, brewing (input, reagent,
 * output) and the special crafting recipes. */
BASALT_IMPORT(recipe_add) int32_t basalt_recipe_add(const char *id, int32_t idlen, const char *json, int32_t jlen);

/* recipe_remove: takes a recipe, the server's own included, out of every lookup. OK, ABSENT when there is none. */
BASALT_IMPORT(recipe_remove) int32_t basalt_recipe_remove(const char *id, int32_t idlen);

/* recipe_has: 1 when a recipe by that id exists, else 0. INVALID for empty id. */
BASALT_IMPORT(recipe_has) int32_t basalt_recipe_has(const char *id, int32_t idlen);

/* recipe_ids: every recipe id (the datapack's first, then the added ones) as a list of strings (needed-length). */
BASALT_IMPORT(recipe_ids) int32_t basalt_recipe_ids(char *out, int32_t cap);

/* recipe_clear: empties the recipe list. recipe_reset: puts back the datapack recipes and drops the added ones.
 * recipe_reset_brewing: drops the brewing recipes plugins added and restores the shipped ones. All return OK. */
BASALT_IMPORT(recipe_clear) int32_t basalt_recipe_clear(void);
BASALT_IMPORT(recipe_reset) int32_t basalt_recipe_reset(void);
BASALT_IMPORT(recipe_reset_brewing) int32_t basalt_recipe_reset_brewing(void);

/* recipe_get: a recipe as a map mirroring its fields (needed-length). ABSENT when there is none. */
BASALT_IMPORT(recipe_get) int32_t basalt_recipe_get(const char *id, int32_t idlen, char *out, int32_t cap);

/* recipe_station: what a crafting station makes, as a value (needed-length). `kind` and `input`:
 *   "crafting"   input {"width": int, "height": int, "items": list of item values or null, row by row}
 *                answer {"recipe": string, "result": item, "remaining": list of item or null}
 *   "anvil"      input {"left": item or null, "right": item or null, "name": string optional}
 *                answer {"result": item or null, "cost": int, "materials": int}
 *   "smithing"   input {"template": item or null, "base": item or null, "addition": item or null}
 *                answer {"recipe": string, "result": item}
 * ABSENT when no recipe fits, INVALID for an unknown kind or a bad input. */
BASALT_IMPORT(recipe_station)
int32_t basalt_recipe_station(const char *kind, int32_t klen, const char *input, int32_t ilen, char *out,
                              int32_t cap);

/* recipe_book_get: the recipe ids the player has discovered, a list of strings (needed-length). */
BASALT_IMPORT(recipe_book_get) int32_t basalt_recipe_book_get(int32_t player, char *out, int32_t cap);

/* recipe_book_set: `ids` is a list value of recipe ids; with `learn` 1 they are discovered, with 0 forgotten.
 * Returns how many actually changed; negative for INVALID (bad list) or STALE/INVALID (player). */
BASALT_IMPORT(recipe_book_set)
int32_t basalt_recipe_book_set(int32_t player, const char *ids, int32_t ilen, int32_t learn);

/* recipe_book_has: 1 when the player knows the recipe, else 0. */
BASALT_IMPORT(recipe_book_has) int32_t basalt_recipe_book_has(int32_t player, const char *id, int32_t idlen);

/* recipe_book_resend: sends the player the recipe packet and recipe book again. OK; ABSENT when the connection has none. */
BASALT_IMPORT(recipe_book_resend) int32_t basalt_recipe_book_resend(int32_t player);

/* merchant_open: opens a merchant window with trades the plugin made. `spec` is {"title": MiniMessage string,
 * "offers": list of {"buy": item, "buy2": item or null, "sell": item, "uses": int, "max_uses": int, "xp": int,
 * "price_multiplier": float, "demand": int, "special_price": int}}. OK; ABSENT when the connection has no merchant
 * window, INVALID for a bad spec, STALE or INVALID for the player. */
BASALT_IMPORT(merchant_open) int32_t basalt_merchant_open(int32_t player, const char *spec, int32_t slen);

/* window_open: opens a plugin-held container window for the player. `spec` is {"kind": string (chest, barrel,
 * shulker_box, hopper, dispenser, dropper, furnace, blast_furnace, smoker, brewing_stand, crafter, anvil, grindstone,
 * loom, cartography_table, stonecutter, smithing, enchanting, beacon, lectern, merchant, crafting),
 * "size": int (chest only, rounded up to rows of 9, default 27), "title": MiniMessage string, "items": list of item
 * or null per slot, "editable": bool (default true)}. Returns the container id (positive); INVALID for an unknown
 * kind or bad spec, ABSENT when the connection has no windows, STALE or INVALID for the player. */
BASALT_IMPORT(window_open) int32_t basalt_window_open(int32_t player, const char *spec, int32_t slen);

/* window_info: the player's open window as {"container_id": int, "kind": string, "slots": int, "plugin": bool,
 * "world": string, "block": {"x","y","z"} when it belongs to a block} (needed-length). ABSENT when none is open. */
BASALT_IMPORT(window_info) int32_t basalt_window_info(int32_t player, char *out, int32_t cap);

/* window_close: closes the window the player has open. OK, ABSENT when none. */
BASALT_IMPORT(window_close) int32_t basalt_window_close(int32_t player);

/* window_update: changes the open window with a map: "title" (MiniMessage string, reopens the screen under the new
 * title) and "properties" (map of data id as decimal string to int, furnace progress, enchantment costs, anvil
 * cost). Unknown keys ignored. OK; ABSENT when no window is open or a property is not accepted, INVALID for a bad
 * map. */
BASALT_IMPORT(window_update) int32_t basalt_window_update(int32_t player, const char *spec, int32_t slen);

/* Container menus. A menu is a window the plugin owns and shares between players, whose slots can run a click
 * handler. container_menu_create returns a handle (positive i32, valid until the plugin is disabled).
 * Click handlers register with a token: basalt_callback(token, ctx) runs when a player clicks that slot; during the
 * callback ctx answers container_menu_click_info, _result and _cancel, and is STALE after the callback returns.
 * On this server the clicks of a menu are always cancelled (a menu is read only), so _cancel(1) is OK and _cancel(0)
 * is DENIED; menus of a kind other than chest run no handlers (they only show items). Handlers run on the plugin
 * thread. */

/* container_menu_create: `spec` is {"title": MiniMessage string, "rows": int 1..6 (chest, default 3), "kind": string
 * as window_open (default "chest")}. Returns the menu handle; INVALID for a bad spec or unknown kind. */
BASALT_IMPORT(container_menu_create) int32_t basalt_container_menu_create(const char *spec, int32_t slen);

/* container_menu_set: puts the item value in `slot` of the menu and registers `token` for clicks on it (0 for no
 * handler); a null item clears the slot and its handler. Redraws for everyone who has the menu open. OK; INVALID for
 * a slot out of range, a bad item or a bad menu handle. */
BASALT_IMPORT(container_menu_set)
int32_t basalt_container_menu_set(int32_t menu, int32_t slot, const char *item, int32_t ilen, int32_t token);

/* container_menu_clear: removes every item and handler of the menu. OK, INVALID for a bad handle. */
BASALT_IMPORT(container_menu_clear) int32_t basalt_container_menu_clear(int32_t menu);

/* container_menu_open: shows the menu to the player, replacing a window they had open. OK; ABSENT when the connection
 * has no windows, STALE or INVALID for the player, INVALID for a bad menu. */
BASALT_IMPORT(container_menu_open) int32_t basalt_container_menu_open(int32_t menu, int32_t player);

/* container_menu_close: closes the menu for that player if they have it open. OK, ABSENT when they do not. */
BASALT_IMPORT(container_menu_close) int32_t basalt_container_menu_close(int32_t menu, int32_t player);

/* container_menu_click_info: during a click callback, the click as {"player": int player handle, "slot": int,
 * "button": int, "mode": string} (needed-length); the server reports button 0 and mode "pickup". STALE or INVALID for ctx. */
BASALT_IMPORT(container_menu_click_info) int32_t basalt_container_menu_click_info(int32_t ctx, char *out, int32_t cap);

/* container_menu_click_result: replaces the stack shown in the clicked slot with the item value (null clears it);
 * applied after the callback returns. OK; INVALID for a bad item, STALE or INVALID for ctx. */
BASALT_IMPORT(container_menu_click_result)
int32_t basalt_container_menu_click_result(int32_t ctx, const char *item, int32_t ilen);

/* container_menu_click_cancel: sets whether the click is cancelled. See the note above for what the server accepts.
 * OK, DENIED when the server cannot honour the value, STALE or INVALID for ctx. */
BASALT_IMPORT(container_menu_click_cancel) int32_t basalt_container_menu_click_cancel(int32_t ctx, int32_t cancelled);

/* Loot and trades. A `roll` spec is a map with the optional keys "world" (name, default world when absent), "x", "y",
 * "z" (numbers), "luck" (number), "seed" (int, 0 or absent rolls without a fixed seed) and "looter" (player uuid);
 * null or an empty map takes every default. Item values are the stacks described at the top. */

/* loot_tables: every loot table id as a list of strings (needed-length). */
BASALT_IMPORT(loot_tables) int32_t basalt_loot_tables(char *out, int32_t cap);

/* loot_roll: what the loot table `table` gives for the roll spec, a list of item values (needed-length). ABSENT when
 * the table or the world is unknown, INVALID for a bad spec. */
BASALT_IMPORT(loot_roll)
int32_t basalt_loot_roll(const char *table, int32_t tlen, const char *spec, int32_t slen, char *out, int32_t cap);

/* loot_roll_json: the same for a loot table given as JSON text (the format of a loot table file). ABSENT when the JSON
 * does not parse as a table or the world is unknown, INVALID for a bad spec. */
BASALT_IMPORT(loot_roll_json)
int32_t basalt_loot_roll_json(const char *json, int32_t jlen, const char *spec, int32_t slen, char *out, int32_t cap);

/* loot_number: evaluates a number provider given as JSON text with an empty context. `as_float` 1 picks a float
 * provider, 0 an int one. The answer is a value, an int or a float (needed-length). ABSENT when the JSON does not parse,
 * INVALID for bad arguments. */
BASALT_IMPORT(loot_number)
int32_t basalt_loot_number(const char *json, int32_t jlen, int32_t as_float, int64_t seed, char *out, int32_t cap);

/* loot_apply: runs loot functions on a stack. `spec` is {"functions": JSON text (one function object or an array),
 * "item": item value, "world", "x", "y", "z", "seed" optional}. The answer is the resulting item value (needed-length).
 * ABSENT when the JSON or the world is unknown, INVALID for a bad spec. */
BASALT_IMPORT(loot_apply) int32_t basalt_loot_apply(const char *spec, int32_t slen, char *out, int32_t cap);

/* loot_enchant: rolls enchantments onto a stack as the enchant_with_levels function does. `spec` is {"item": item
 * value, "levels": int, "options": enchantment id or a tag with a leading `#` (default `#minecraft:in_enchanting_table`),
 * "seed", "world" optional}. The answer is the resulting item value (needed-length). ABSENT when the world is unknown,
 * INVALID for a bad spec. */
BASALT_IMPORT(loot_enchant) int32_t basalt_loot_enchant(const char *spec, int32_t slen, char *out, int32_t cap);

/* trades_roll: the trades a villager of `profession` offers at `level` (1 to 5), as a list of maps {"cost_a": item,
 * "cost_b": item or null, "result": item, "max_uses": int, "xp": int, "price_multiplier": float} (needed-length).
 * `spec` takes the roll keys plus "variant" (villager type), "amount" (int) and "listed" (list of trade ids to draw
 * from without repeats). ABSENT when the profession, level or world is unknown, INVALID for a bad spec. */
BASALT_IMPORT(trades_roll)
int32_t basalt_trades_roll(const char *profession, int32_t plen, int32_t level, const char *spec, int32_t slen,
                           char *out, int32_t cap);

#ifdef __cplusplus
}
#endif
#endif
