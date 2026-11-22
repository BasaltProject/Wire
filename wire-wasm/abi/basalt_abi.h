/* Basalt plugin ABI, version 1. The normative definition of what a WebAssembly plugin may import and export.
 *
 * A plugin is a wasm32 module that imports from the module "basalt" and exports the functions listed under
 * "Exports". Every pointer is an offset into the plugin's own linear memory, every string is UTF-8 given as a
 * pointer and a byte length, and every integer is little-endian. The host never allocates inside the plugin: it
 * writes only into buffers the plugin hands it.
 *
 * Buffer-returning calls give back the length the full answer needs and write it only when it fits in `cap`, so
 * the plugin retries with a bigger buffer when the result exceeds `cap`. A negative result is an error code.
 */
#ifndef BASALT_ABI_H
#define BASALT_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASALT_ABI_VERSION 1

#define BASALT_IMPORT(name) __attribute__((import_module("basalt"), import_name(#name)))
/* An import that the proxy profile offers too, with the same meaning (see basalt_proxy_abi.h). */
#define BASALT_IMPORT_SHARED(name) __attribute__((import_module("basalt"), import_name(#name)))
#define BASALT_EXPORT(name) __attribute__((export_name(#name)))

/* Error codes (negative results). */
#define BASALT_OK 0
#define BASALT_ERR_ABSENT (-1)  /* the key, argument or field does not exist */
#define BASALT_ERR_STALE (-2)   /* the handle no longer refers to anything (player left, context ended) */
#define BASALT_ERR_INVALID (-3) /* a bad argument: unknown name, malformed text, out of range */
#define BASALT_ERR_DENIED (-4)  /* the plugin lacks the capability in plugin.toml */

/* Log levels. */
#define BASALT_LOG_DEBUG 0
#define BASALT_LOG_INFO 1
#define BASALT_LOG_WARN 2
#define BASALT_LOG_ERROR 3

/* Event priorities, lowest runs first. */
#define BASALT_PRIORITY_LOWEST 0
#define BASALT_PRIORITY_LOW 1
#define BASALT_PRIORITY_NORMAL 2
#define BASALT_PRIORITY_HIGH 3
#define BASALT_PRIORITY_HIGHEST 4

/* ---- Exports: what the plugin provides ----------------------------------------------------------------------
 *
 *   memory                                    the linear memory, exported under this name
 *   int32_t basalt_abi_version(void)          returns BASALT_ABI_VERSION
 *   void    basalt_init(void)                 called once after instantiation; register everything here
 *   void    basalt_callback(int32_t token, int32_t ctx)
 *                                             runs what was registered under `token`; `ctx` is the handle of the
 *                                             call (event, command, task, menu click, placeholder) and is valid
 *                                             only until the callback returns
 *
 * A token is any integer the plugin picked when it registered; the plugin maps it back to its own function.
 * Callbacks run on the owner of what they touch and must not block.
 */

/* ---- Arguments lists -----------------------------------------------------------------------------------------
 *
 * Text templates take named arguments as a packed list: a u32 count, then per entry u32 key length, the key bytes,
 * u32 value length, the value bytes. A zero-length list (ptr 0, len 0) means no arguments. Each "{key}" in the text
 * is replaced by its value.
 */

/* ---- Imports: logging --------------------------------------------------------------------------------------- */
BASALT_IMPORT_SHARED(log) void basalt_log(int32_t level, const char *msg, int32_t len);

/* ---- Imports: configuration (resources/config.toml; keys are dotted, e.g. "shop.sparkles-price") ------------- */
BASALT_IMPORT_SHARED(config_i64) int64_t basalt_config_i64(const char *key, int32_t klen, int64_t def);
BASALT_IMPORT_SHARED(config_f64) double basalt_config_f64(const char *key, int32_t klen, double def);
BASALT_IMPORT_SHARED(config_bool) int32_t basalt_config_bool(const char *key, int32_t klen, int32_t def);
BASALT_IMPORT_SHARED(config_str) int32_t basalt_config_str(const char *key, int32_t klen, char *out, int32_t cap);

/* ---- Imports: text (resources/lang/<code>.toml, MiniMessage) -------------------------------------------------------
 *
 * translate fills `out` with the template for `key` with its arguments applied, in the plugin's language.
 */
BASALT_IMPORT_SHARED(translate)
int32_t basalt_translate(const char *key, int32_t klen, const char *args, int32_t alen, char *out, int32_t cap);

/* ---- Imports: players ---------------------------------------------------------------------------------------- */
BASALT_IMPORT_SHARED(player_by_name) int32_t basalt_player_by_name(const char *name, int32_t len); /* handle, 0 if none */
BASALT_IMPORT_SHARED(players_online) int32_t basalt_players_online(int32_t *out, int32_t cap);     /* count of handles */
BASALT_IMPORT_SHARED(player_name) int32_t basalt_player_name(int32_t player, char *out, int32_t cap);

/* send: `text` that starts with '@' is the lang key after it, anything else is MiniMessage; arguments as above. */
BASALT_IMPORT_SHARED(player_send)
int32_t basalt_player_send(int32_t player, const char *text, int32_t tlen, const char *args, int32_t alen);
BASALT_IMPORT_SHARED(player_action_bar)
int32_t basalt_player_action_bar(int32_t player, const char *text, int32_t tlen, const char *args, int32_t alen);
BASALT_IMPORT(player_particle)
int32_t basalt_player_particle(int32_t player, const char *id, int32_t idlen, int32_t count, double spread);
BASALT_IMPORT(player_sound)
int32_t basalt_player_sound(int32_t player, const char *id, int32_t idlen, float volume, float pitch);

/* ---- Imports: per-player data (needs capability players_data; persisted in players.db under the plugin) ------ */
BASALT_IMPORT_SHARED(data_get_i64) int64_t basalt_data_get_i64(int32_t player, const char *key, int32_t klen, int64_t def);
BASALT_IMPORT_SHARED(data_set_i64) int32_t basalt_data_set_i64(int32_t player, const char *key, int32_t klen, int64_t value);
BASALT_IMPORT_SHARED(data_get_bool) int32_t basalt_data_get_bool(int32_t player, const char *key, int32_t klen, int32_t def);
BASALT_IMPORT_SHARED(data_set_bool) int32_t basalt_data_set_bool(int32_t player, const char *key, int32_t klen, int32_t value);
BASALT_IMPORT_SHARED(data_get_str)
int32_t basalt_data_get_str(int32_t player, const char *key, int32_t klen, char *out, int32_t cap);
BASALT_IMPORT_SHARED(data_set_str)
int32_t basalt_data_set_str(int32_t player, const char *key, int32_t klen, const char *value, int32_t vlen);
BASALT_IMPORT_SHARED(data_remove) int32_t basalt_data_remove(int32_t player, const char *key, int32_t klen);

/* store_top: the `limit` highest values of a numeric data key across all players, best first. The result is a packed
 * list of rows: u32 name length, the name, i64 value. */
BASALT_IMPORT(store_top)
int32_t basalt_store_top(const char *key, int32_t klen, int32_t limit, char *out, int32_t cap);

/* ---- Imports: events ------------------------------------------------------------------------------------------
 *
 * Listens to an event by its name ("player.join", "plugin.enable", ...) and calls basalt_callback(token, ctx).
 * Inside the callback the ctx handle answers the event_* calls.
 */
BASALT_IMPORT_SHARED(event_on) int32_t basalt_event_on(const char *name, int32_t nlen, int32_t priority, int32_t token);
BASALT_IMPORT_SHARED(event_player) int32_t basalt_event_player(int32_t ctx); /* player handle, 0 if the event has none */
BASALT_IMPORT_SHARED(event_field_str)
int32_t basalt_event_field_str(int32_t ctx, const char *field, int32_t flen, char *out, int32_t cap);
BASALT_IMPORT_SHARED(event_field_i64)
int32_t basalt_event_field_i64(int32_t ctx, const char *field, int32_t flen, int64_t *out); /* 0 ok, else an error */
BASALT_IMPORT_SHARED(event_cancel) int32_t basalt_event_cancel(int32_t ctx, int32_t cancelled);

/* ---- Imports: commands ----------------------------------------------------------------------------------------
 *
 * `spec` is the command grammar as in the plugin docs: "coins top [limit: int 1..25]". `permission` may be empty.
 */
BASALT_IMPORT_SHARED(command_register)
int32_t basalt_command_register(const char *spec, int32_t slen, const char *permission, int32_t plen, int32_t token);
BASALT_IMPORT_SHARED(command_source_name) int32_t basalt_command_source_name(int32_t ctx, char *out, int32_t cap);
BASALT_IMPORT_SHARED(command_is_console) int32_t basalt_command_is_console(int32_t ctx);
BASALT_IMPORT_SHARED(command_reply)
int32_t basalt_command_reply(int32_t ctx, const char *text, int32_t tlen, const char *args, int32_t alen);
BASALT_IMPORT_SHARED(command_arg_str)
int32_t basalt_command_arg_str(int32_t ctx, const char *name, int32_t nlen, char *out, int32_t cap); /* ABSENT if unset */
BASALT_IMPORT_SHARED(command_arg_i64)
int32_t basalt_command_arg_i64(int32_t ctx, const char *name, int32_t nlen, int64_t *out); /* ABSENT if unset */

/* ---- Imports: tasks (delays and periods in ticks, 20 per second) --------------------------------------------- */
BASALT_IMPORT_SHARED(task_after) int32_t basalt_task_after(int32_t delay, int32_t token);      /* task handle */
BASALT_IMPORT_SHARED(task_every) int32_t basalt_task_every(int32_t delay, int32_t period, int32_t token);
BASALT_IMPORT_SHARED(task_cancel) int32_t basalt_task_cancel(int32_t task);

/* ---- Imports: placeholders ------------------------------------------------------------------------------------
 *
 * The name is what appears in "%name%". In the callback, placeholder_player is the player it is resolved for (0 for
 * none) and the plugin answers with placeholder_result.
 */
BASALT_IMPORT(placeholder_register) int32_t basalt_placeholder_register(const char *name, int32_t nlen, int32_t token);
BASALT_IMPORT(placeholder_player) int32_t basalt_placeholder_player(int32_t ctx);
BASALT_IMPORT(placeholder_result) int32_t basalt_placeholder_result(int32_t ctx, const char *text, int32_t len);

/* ---- Imports: menus -------------------------------------------------------------------------------------------
 *
 * A menu is a chest window the plugin owns and every player who opens it shares. `title`, item names and lore are
 * MiniMessage (already translated); lore lines are separated by '\n'. Clicking a slot calls
 * basalt_callback(token, ctx) where ctx answers menu_click_player.
 */
BASALT_IMPORT(menu_create) int32_t basalt_menu_create(const char *title, int32_t tlen, int32_t rows); /* handle */
BASALT_IMPORT(menu_set)
int32_t basalt_menu_set(int32_t menu, int32_t slot, const char *item, int32_t ilen, const char *name, int32_t nlen,
                        const char *lore, int32_t llen, int32_t token);
BASALT_IMPORT(menu_open) int32_t basalt_menu_open(int32_t menu, int32_t player);
BASALT_IMPORT(menu_click_player) int32_t basalt_menu_click_player(int32_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* BASALT_ABI_H */
