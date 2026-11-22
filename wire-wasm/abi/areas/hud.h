/* Area hud: scoreboards, teams, boss bars, holograms, NPCs, the tab list and the claimable screen surfaces.
 *
 * Handles. scoreboard, bossbar, hologram, npc and surface claim handles are positive i32 values that belong to the
 * plugin that made them; 0 means none, a removed or released handle is STALE and a malformed one is INVALID. Every
 * handle the plugin made and did not release is removed or released when the plugin is disabled (a boss bar with a
 * `key` is a kept bar of the server and survives). Players are the `player` handles of basalt_abi.h and worlds are
 * named by string.
 *
 * Text. Every text argument, and every `text`, `title`, `prefix`, `suffix` or `display_name` map value, is MiniMessage
 * with the plugin language applied: a string that starts with '@' is the lang key after it, anything else is
 * MiniMessage. A lang key that does not exist is BASALT_ERR_ABSENT.
 *
 * Values. `spec` arguments are one value (basalt_values.h): a map whose unknown keys are ignored and whose absent keys
 * leave the current setting alone. A zero-length spec is an empty map. An `out` list or map is written with the
 * needed-length convention. A key of the wrong type is BASALT_ERR_INVALID.
 *
 * Threads. Every import runs on the plugin's own turn; the targets are shared server state and the host serialises the
 * change with the server. Callbacks (hud_npc_create click, hud_surface_claim render) run on the plugin's turn too and
 * are skipped when that turn is not available.
 *
 * Errors. ABSENT: the named objective, team, entry, key or board does not exist. INVALID: bad name, slot or spec, or
 * the server refused (a taken name). STALE: the handle is gone, including a boss bar removed by a command.
 */
#ifndef BASALT_AREA_HUD_H
#define BASALT_AREA_HUD_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- Scoreboards ----------------------------------------------------------------------------------------------
 *
 * A board holds objectives, scores and teams. The main board is the one /scoreboard and /team edit and every player
 * sees by default; a board the plugin creates is its own and is drawn for the players it was shown to. Slots are
 * "list", "sidebar", "below_name" and "sidebar.team.<color>" (a team color name such as red).
 */

/* scoreboard_main: the handle of the main board. The same handle on every call, never STALE, and it cannot be
 * released (INVALID). */
BASALT_IMPORT(hud_scoreboard_main) int32_t basalt_hud_scoreboard_main(void);

/* scoreboard_create: a new empty board of the plugin's own; its handle, or INVALID when scoreboards are not
 * available. */
BASALT_IMPORT(hud_scoreboard_create) int32_t basalt_hud_scoreboard_create(void);

/* scoreboard_release: forgets a board made by scoreboard_create once nothing shows it. OK; STALE; INVALID for the main
 * board. */
BASALT_IMPORT(hud_scoreboard_release) int32_t basalt_hud_scoreboard_release(int32_t board);

/* scoreboard_list: the names a board holds as a list of strings. `what` is "objectives", "entries" or "teams"
 * (anything else is INVALID). Result: the length of the encoded list; STALE for a bad board. */
BASALT_IMPORT(hud_scoreboard_list)
int32_t basalt_hud_scoreboard_list(int32_t board, const char *what, int32_t wlen, char *out, int32_t cap);

/* objective_add: spec keys criterion (string, default "dummy"), title (text, default the name), hearts (bool, shows
 * the score as hearts). OK; INVALID when the name is taken or the criterion is unknown. */
BASALT_IMPORT(hud_objective_add)
int32_t basalt_hud_objective_add(int32_t board, const char *name, int32_t nlen, const char *spec, int32_t slen);

/* objective_remove: OK; ABSENT when there is no such objective. */
BASALT_IMPORT(hud_objective_remove) int32_t basalt_hud_objective_remove(int32_t board, const char *name, int32_t nlen);

/* display_set: shows the objective in the slot; an empty objective name clears the slot. OK; INVALID for a bad slot;
 * ABSENT when the objective does not exist. */
BASALT_IMPORT(hud_display_set)
int32_t basalt_hud_display_set(int32_t board, const char *slot, int32_t slotlen, const char *objective, int32_t olen);

/* display_get: the name of the objective shown in the slot, written to `out`; its length. ABSENT when the slot is
 * empty; INVALID for a bad slot. */
BASALT_IMPORT(hud_display_get)
int32_t basalt_hud_display_get(int32_t board, const char *slot, int32_t slotlen, char *out, int32_t cap);

/* score_get: the entry's score in the objective, stored in `*out`. OK; ABSENT when the entry has none. */
BASALT_IMPORT(hud_score_get)
int32_t basalt_hud_score_get(int32_t board, const char *entry, int32_t elen, const char *objective, int32_t olen,
                             int32_t *out);

/* score_set: OK; INVALID when the objective does not exist or is read only. */
BASALT_IMPORT(hud_score_set)
int32_t basalt_hud_score_set(int32_t board, const char *entry, int32_t elen, const char *objective, int32_t olen,
                             int32_t value);

/* score_reset: forgets the entry's score in the objective, or in every objective when `objective` is empty. OK. */
BASALT_IMPORT(hud_score_reset)
int32_t basalt_hud_score_reset(int32_t board, const char *entry, int32_t elen, const char *objective, int32_t olen);

/* team_add: OK; INVALID when the name is taken. */
BASALT_IMPORT(hud_team_add) int32_t basalt_hud_team_add(int32_t board, const char *name, int32_t nlen);

/* team_set: spec keys display_name, prefix, suffix (text), color (a color name or "none"), friendly_fire and
 * see_invisible (bool), name_tags, death_messages (always, never, hideForOtherTeams, hideForOwnTeam) and collision
 * (always, never, pushOtherTeams, pushOwnTeam). OK; ABSENT when the team does not exist. */
BASALT_IMPORT(hud_team_set)
int32_t basalt_hud_team_set(int32_t board, const char *name, int32_t nlen, const char *spec, int32_t slen);

/* team_remove: OK; ABSENT when there is no such team. */
BASALT_IMPORT(hud_team_remove) int32_t basalt_hud_team_remove(int32_t board, const char *name, int32_t nlen);

/* team_join: an entry (a player name or an entity uuid) joins the team and leaves the one it was on. OK; ABSENT when
 * the team does not exist. */
BASALT_IMPORT(hud_team_join)
int32_t basalt_hud_team_join(int32_t board, const char *team, int32_t tlen, const char *entry, int32_t elen);

/* team_leave: OK; ABSENT when the entry is not on that team. */
BASALT_IMPORT(hud_team_leave)
int32_t basalt_hud_team_leave(int32_t board, const char *team, int32_t tlen, const char *entry, int32_t elen);

/* team_of: the name of the team the entry is on, written to `out`; its length. ABSENT when on none. */
BASALT_IMPORT(hud_team_of)
int32_t basalt_hud_team_of(int32_t board, const char *entry, int32_t elen, char *out, int32_t cap);

/* scoreboard_show: draws the board for the player instead of the one they had. OK; STALE for a gone player or board.
 * Showing the main board puts the player back on the default. */
BASALT_IMPORT(hud_scoreboard_show) int32_t basalt_hud_scoreboard_show(int32_t player, int32_t board);

/* scoreboard_shown: the handle of the board the player sees, when it is the main board or one the plugin holds.
 * ABSENT when it is a board of another plugin; STALE for a gone player. */
BASALT_IMPORT(hud_scoreboard_shown) int32_t basalt_hud_scoreboard_shown(int32_t player);

/* ---- Boss bars ------------------------------------------------------------------------------------------------
 *
 * A bar is drawn at the top of the screen of the players added to it. Spec keys: title (text), color (pink, blue, red,
 * green, yellow, purple, white), style (progress, notched_6, notched_10, notched_12, notched_20), progress (a number
 * from 0 to 1), darken_sky, boss_music, fog and visible (bool), and on create only key (a namespaced key that keeps
 * the bar in /bossbar across restarts).
 */

/* bossbar_create: the new bar's handle. INVALID for a bad spec or when a kept bar of that key exists. */
BASALT_IMPORT(hud_bossbar_create) int32_t basalt_hud_bossbar_create(const char *spec, int32_t slen);

/* bossbar_attach: a handle for an existing bar by its id or key (from bossbar_kept or bossbar_get), so a plugin can
 * keep driving a kept bar after a restart. ABSENT when there is none. The handle is the plugin's to drop with
 * bossbar_remove only when it wants the bar deleted. */
BASALT_IMPORT(hud_bossbar_attach) int32_t basalt_hud_bossbar_attach(const char *id, int32_t ilen);

/* bossbar_update: applies the spec keys present. OK; STALE when the bar is gone; INVALID for a bad spec. */
BASALT_IMPORT(hud_bossbar_update) int32_t basalt_hud_bossbar_update(int32_t bar, const char *spec, int32_t slen);

/* bossbar_remove: deletes the bar for everyone. OK; STALE. */
BASALT_IMPORT(hud_bossbar_remove) int32_t basalt_hud_bossbar_remove(int32_t bar);

/* bossbar_player_add and bossbar_player_remove: OK, also when the player was already in or out; STALE for a gone bar
 * or player. */
BASALT_IMPORT(hud_bossbar_player_add) int32_t basalt_hud_bossbar_player_add(int32_t bar, int32_t player);
BASALT_IMPORT(hud_bossbar_player_remove) int32_t basalt_hud_bossbar_player_remove(int32_t bar, int32_t player);

/* bossbar_players: the uuids of the players shown the bar, as a list of strings. STALE for a gone bar. */
BASALT_IMPORT(hud_bossbar_players) int32_t basalt_hud_bossbar_players(int32_t bar, char *out, int32_t cap);

/* bossbar_get: a map with id, key (string or null), title, color, style, progress, darken_sky, boss_music, fog and
 * visible. The title is the server's text form. STALE for a gone bar. */
BASALT_IMPORT(hud_bossbar_get) int32_t basalt_hud_bossbar_get(int32_t bar, char *out, int32_t cap);

/* bossbar_kept: the ids of the kept bars as a list of strings. */
BASALT_IMPORT(hud_bossbar_kept) int32_t basalt_hud_bossbar_kept(char *out, int32_t cap);

/* ---- Holograms ------------------------------------------------------------------------------------------------
 *
 * A hologram is a floating text the server draws; everybody sees it unless an audience limits it.
 */

/* hologram_create: the new hologram's handle. ABSENT when the world does not exist or the lang key is missing;
 * INVALID when the server could not make it. */
BASALT_IMPORT(hud_hologram_create)
int32_t basalt_hud_hologram_create(const char *world, int32_t wlen, const char *text, int32_t tlen, double x, double y,
                                   double z);

/* hologram_set: spec keys text, x, y and z (a move, the three together), audience (a list of player names or uuids,
 * or null for everyone). OK; STALE; INVALID for a partial position or a key of the wrong type. */
BASALT_IMPORT(hud_hologram_set) int32_t basalt_hud_hologram_set(int32_t hologram, const char *spec, int32_t slen);

/* hologram_get: a map with id (the entity id), world, x, y, z, alive, text and audience (a list of uuids or null).
 * STALE for a gone hologram. */
BASALT_IMPORT(hud_hologram_get) int32_t basalt_hud_hologram_get(int32_t hologram, char *out, int32_t cap);

/* hologram_remove: OK; STALE. */
BASALT_IMPORT(hud_hologram_remove) int32_t basalt_hud_hologram_remove(int32_t hologram);

/* ---- NPCs -----------------------------------------------------------------------------------------------------
 *
 * An NPC is a fake player drawn to its viewers. Its click callback runs basalt_callback(token, ctx) with a completion
 * ctx whose value_result is a map with npc (the handle), player (the clicker's name, for player_by_name) and uuid.
 */

/* npc_create: spec keys world (default the default world), name (text), x, y, z, uuid (a fixed uuid), skin (a map
 * with texture and signature, or with name of an online player). `token` is the click callback, 0 for none. The new
 * NPC's handle (it has the attributes max_health and movement_speed); ABSENT for an unknown world; INVALID for a bad spec or when the server could not make it. */
BASALT_IMPORT(hud_npc_create) int32_t basalt_hud_npc_create(const char *spec, int32_t slen, int32_t token);

/* npc_set: spec keys name, x, y, z (a move), yaw and pitch (a look), looks_at_players (bool), skin (as in create),
 * audience (a list of names or uuids, or null), health, held_slot (0 to 8), flags (the shared flags byte), pose (the
 * Pose ordinal), attributes (a map from attribute name to base value) and equipment (a map from a Bukkit slot number
 * as a string, 0 to 8 hotbar, 36 to 39 feet to head, 40 off hand, to a map with item and count, or null to clear).
 * Keys are applied in that order and OK is returned when all applied. INVALID for the first one that fails (an
 * attribute the NPC lacks, an unknown item); STALE when the NPC is gone. */
BASALT_IMPORT(hud_npc_set) int32_t basalt_hud_npc_set(int32_t npc, const char *spec, int32_t slen);

/* npc_get: a map with id (the entity id), uuid, name, world, x, y, z, alive, audience, health, max_health, flags, pose
 * and held_slot. STALE for a gone NPC. */
BASALT_IMPORT(hud_npc_get) int32_t basalt_hud_npc_get(int32_t npc, char *out, int32_t cap);

/* npc_item: what the NPC holds in a Bukkit slot, as a map with item and count. ABSENT when the slot is empty. */
BASALT_IMPORT(hud_npc_item) int32_t basalt_hud_npc_item(int32_t npc, int32_t slot, char *out, int32_t cap);

/* npc_attribute: the current value of an attribute (a name without namespace means minecraft:), stored in `*out`.
 * OK; ABSENT when the NPC lacks it. */
BASALT_IMPORT(hud_npc_attribute)
int32_t basalt_hud_npc_attribute(int32_t npc, const char *name, int32_t nlen, double *out);

/* npc_animate: plays an animation of the ClientboundAnimatePacket action id (0 swing main hand). OK; STALE. */
BASALT_IMPORT(hud_npc_animate) int32_t basalt_hud_npc_animate(int32_t npc, int32_t action);

/* npc_remove: OK; STALE. */
BASALT_IMPORT(hud_npc_remove) int32_t basalt_hud_npc_remove(int32_t npc);

/* ---- Tab list and per-player HUD -------------------------------------------------------------------------------
 *
 * player_set and player_get address the player's HUD properties by name: tab_header and tab_footer (text, null puts
 * the configured tab list back), list_name (text or null), listed (bool), list_order (int), score (int, the death
 * screen score), waypoint_style (string) and waypoint_color (an RGB int, -1 for none). The names come from the
 * server's player property table.
 */

/* player_set: applies the present keys. OK; STALE; INVALID for an unknown key or a value of the wrong type. */
BASALT_IMPORT(hud_player_set) int32_t basalt_hud_player_set(int32_t player, const char *spec, int32_t slen);

/* player_get: a map holding every property above. STALE for a gone player. */
BASALT_IMPORT(hud_player_get) int32_t basalt_hud_player_get(int32_t player, char *out, int32_t cap);

/* tab_entry: whether this viewer's tab list shows the player with the entity id. OK; INVALID when no online player has
 * that entity id (an NPC is not a tab list entry). */
BASALT_IMPORT(hud_tab_entry) int32_t basalt_hud_tab_entry(int32_t viewer, int32_t entity_id, int32_t listed);

/* ---- Surfaces -------------------------------------------------------------------------------------------------
 *
 * A surface is a part of the screen several plugins may want: "sidebar", "boss_bar", "tab_list", "name_tag" and
 * "chat_format". A claim has a priority; the highest drawn content wins per player, a tie keeps the first claimer and
 * is logged, and an empty answer lets the next claim draw. Content is a map with title (text) and lines (a list of
 * text, at most 15 shown on the sidebar; for boss_bar the first line is the progress from 0 to 1). Text may use
 * %placeholders%, filled per player. The server redraws every 20 ticks.
 */

/* surface_claim: claims the surface. `spec` is the static content, or empty; `token` is a render callback, 0 for none.
 * The callback runs per player and redraw with a completion ctx whose value_result is a map with surface and player
 * (the name, for player_by_name); it answers with surface_answer, and not answering draws nothing for that player.
 * The claim's handle; INVALID for an unknown surface name. */
BASALT_IMPORT(hud_surface_claim)
int32_t basalt_hud_surface_claim(const char *surface, int32_t slen, int32_t priority, const char *spec, int32_t cslen,
                                 int32_t token);

/* surface_update: replaces the static content. OK; STALE. */
BASALT_IMPORT(hud_surface_update) int32_t basalt_hud_surface_update(int32_t claim, const char *spec, int32_t slen);

/* surface_release: lets go of the surface. OK; STALE. */
BASALT_IMPORT(hud_surface_release) int32_t basalt_hud_surface_release(int32_t claim);

/* surface_answer: inside a render callback, the content for that player. OK; STALE once the callback returned;
 * INVALID for a ctx that is not a render callback or a bad spec. */
BASALT_IMPORT(hud_surface_answer) int32_t basalt_hud_surface_answer(int32_t ctx, const char *spec, int32_t slen);

#ifdef __cplusplus
}
#endif
#endif
