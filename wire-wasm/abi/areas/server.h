/* Area server: permissions, dynamic commands, broadcast, plugin storage and files, configuration values, services shared
 * between plugins, scheduler completions and server administration. Server profile only.
 *
 * Conventions of the whole ABI apply (basalt_abi.h, basalt_values.h, core_values.h): text is pointer plus byte length,
 * handles are positive i32, a value is one encoded document, a buffer result is the length the full answer needs.
 * Every import in this area runs on the plugin thread, inside a plugin callback or in basalt_init, and never blocks.
 * Callbacks of this area never nest: a callback that would start inside another callback of the same plugin is queued
 * to run after it, or, for a provider asked from inside its own plugin's call, skipped (said per import below).
 *
 * Capabilities (plugin.toml [capabilities]): `files` (a list of folders of the plugin folder, "." for all of it),
 * `storage` (store_* and data_*), `commands` (command_run), `datapacks` (the datapacks.* operations of server_call).
 * An import without its capability returns BASALT_ERR_DENIED.
 *
 * ---- Permissions ---------------------------------------------------------------------------------------------------
 * A subject is the text "console", a player uuid or a player name. Answers come from the server's chain: plugin
 * providers (highest priority first), then the grants plugins gave, then groups.toml.
 */
#ifndef BASALT_AREA_SERVER_H
#define BASALT_AREA_SERVER_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* permission_check: asks the whole chain. 1 granted, 0 denied, ABSENT when no provider decides because the subject is
 * not online (the console always decides), INVALID for an empty node or malformed text. The caller's own provider is
 * not asked: a plugin never answers its own question through the chain. */
BASALT_IMPORT(permission_check)
int32_t basalt_permission_check(const char *subject, int32_t slen, const char *node, int32_t nlen);

/* permission_meta: a text option for a subject from the providers, such as "prefix" or "suffix". Writes it to `out`
 * and returns the length the full answer needs; ABSENT when no provider gives one or the subject is not online. */
BASALT_IMPORT(permission_meta)
int32_t basalt_permission_meta(const char *subject, int32_t slen, const char *key, int32_t klen, char *out,
                               int32_t cap);

/* permission_has: whether the chain grants the online player the node (the player's own may() answer). 1 or 0, STALE
 * for a player that left, INVALID for an empty node. */
BASALT_IMPORT(permission_has) int32_t basalt_permission_has(int32_t player, const char *node, int32_t nlen);

/* permission_set: grants (mode 1), denies (mode 0) or drops this plugin's grant or denial (mode -1) of a node for the
 * player, on top of the player's group, until the player leaves. The source of the entry is the calling plugin. The
 * player's command tree is sent again. OK, STALE, INVALID for another mode or an empty node. */
BASALT_IMPORT(permission_set) int32_t basalt_permission_set(int32_t player, const char *node, int32_t nlen, int32_t mode);

/* permission_provide: answers permission checks for the whole server until the plugin unloads or the provider is
 * removed. `priority` is above 0, higher asks first (INVALID otherwise); `kinds` is a bit set, 1 for checks and 2 for
 * options (INVALID when 0). Returns a provider handle. The callback runs with a completion-kind ctx whose value_result
 * is the query map {kind: "check" or "meta", name: text, uuid: text (empty for the console), console: bool, node:
 * text (the option key for kind "meta"), world: text (empty when not in a world)}. A check answers with
 * permission_answer, an option with permission_answer_meta; not answering lets lower providers decide. The callback is
 * a decision: it must answer inside the call, and a query raised on another thread, or while this plugin is inside any
 * other callback, is skipped as undecided. */
BASALT_IMPORT(permission_provide) int32_t basalt_permission_provide(int32_t priority, int32_t kinds, int32_t token);

/* permission_provider_remove: stops a provider. OK, or ABSENT for an unknown or already removed handle. */
BASALT_IMPORT(permission_provider_remove) int32_t basalt_permission_provider_remove(int32_t provider);

/* permission_answer: inside a "check" provider callback, answer 1 (granted), 0 (denied) or -1 (undecided). OK, STALE
 * once the callback returned, INVALID for another ctx, another kind of query or another answer. */
BASALT_IMPORT(permission_answer) int32_t basalt_permission_answer(int32_t ctx, int32_t answer);

/* permission_answer_meta: inside a "meta" provider callback, answers the option with text. OK, STALE, INVALID. */
BASALT_IMPORT(permission_answer_meta) int32_t basalt_permission_answer_meta(int32_t ctx, const char *text, int32_t len);

/* permission_changed: says the answers of this plugin's providers changed, so command trees are sent again. OK. */
BASALT_IMPORT(permission_changed) int32_t basalt_permission_changed(void);

/* ---- Commands beyond command_register -----------------------------------------------------------------------------
 * command_register (core) takes the grammar of the plugin docs. The imports below add what it lacks. */

/* command_register_ex: like command_register and also lets the plugin change the command later: only a command
 * registered through this import can be aliased or unregistered. A non-negative `complete_token` also asks the
 * plugin for tab completions of the command's arguments: that callback runs with a command-kind ctx (command_source
 * and command_arg_* work) and command_complete_input gives the text typed so far for the current argument; the
 * callback adds candidates with command_complete_add. It runs on the thread that handles tab completion, only when
 * that is the plugin thread (otherwise nothing is offered) and never while the plugin is in another callback. A
 * negative `complete_token` means no completions. OK, INVALID for a malformed or already registered spec. */
BASALT_IMPORT(command_register_ex)
int32_t basalt_command_register_ex(const char *spec, int32_t slen, const char *permission, int32_t plen, int32_t token,
                                   int32_t complete_token);

/* command_complete_input: inside a completion callback, writes the text typed so far for the current argument and
 * returns the length the full answer needs. INVALID outside a completion callback, STALE after it. */
BASALT_IMPORT(command_complete_input) int32_t basalt_command_complete_input(int32_t ctx, char *out, int32_t cap);

/* command_complete_add: inside a completion callback, offers one candidate replacement for the current argument.
 * OK, or INVALID for an empty text or a ctx that is not a completion callback. */
BASALT_IMPORT(command_complete_add) int32_t basalt_command_complete_add(int32_t ctx, const char *text, int32_t len);

/* command_alias: registers `alias` as another first word for every command this plugin registered through
 * command_register_ex under `name`, with the same arguments, handlers and permission (the permission of the original
 * name when none was given). OK, ABSENT when the plugin has no command called `name`, INVALID when
 * `alias` is empty, has a space, or is already a command of this plugin. */
BASALT_IMPORT(command_alias)
int32_t basalt_command_alias(const char *name, int32_t nlen, const char *alias, int32_t alen);

/* command_unregister: removes every command_register_ex command (and alias) of this plugin under the first word
 * `name`; the result is how many were removed, ABSENT for none. It works at any time, so a plugin can change its
 * command set while the server runs. */
BASALT_IMPORT(command_unregister) int32_t basalt_command_unregister(const char *name, int32_t nlen);

/* command_run: queues a command line (a leading slash is optional) to run as the console (player 0) or as the player
 * on the next tick, after the current callback returned, so the command may call back into the plugin without nesting.
 * Both need capability `commands`. 1 when it was queued, DENIED without the capability, STALE for a player that left,
 * INVALID for an empty line. */
BASALT_IMPORT(command_run) int32_t basalt_command_run(const char *command, int32_t clen, int32_t player);

/* command_arg_f64: the float argument `name` of the command ctx, converting an integer. ABSENT when it was not typed,
 * INVALID when it is not numeric. Writes the value at `out`. */
BASALT_IMPORT(command_arg_f64) int32_t basalt_command_arg_f64(int32_t ctx, const char *name, int32_t nlen, double *out);

/* command_arg_bool: the bool argument `name`, 1 or 0 at `out`. ABSENT when it was not typed, INVALID when it is not
 * a bool argument. */
BASALT_IMPORT(command_arg_bool) int32_t basalt_command_arg_bool(int32_t ctx, const char *name, int32_t nlen, int32_t *out);

/* command_source: describes who ran the command, as a value map {name: text, console: bool, kind: "console",
 * "player", "command_block", "proxied" or "other", world: text, x: f64, y: f64, z: f64, caller: text (for a proxied
 * call, the sender that asked: its kind, a colon and its name)}. Length the full answer needs, STALE, INVALID. */
BASALT_IMPORT(command_source) int32_t basalt_command_source(int32_t ctx, char *out, int32_t cap);

/* command_source_player: the player handle of the sender of a command ctx, 0 when it is the console or not a player
 * who is online. STALE, INVALID. */
BASALT_IMPORT(command_source_player) int32_t basalt_command_source_player(int32_t ctx);

/* ---- Text, broadcast and server information ------------------------------------------------------------------------ */

/* server_broadcast: sends `text` (a '@' prefix is a lang key, anything else MiniMessage; arguments as in
 * player_send) to the audience of `flags`: bit 1 every online player, bit 2 the console log of the plugin. Returns the
 * number of players it reached (0 when only the console was asked). ABSENT for an unknown lang key, INVALID for no
 * bit or malformed arguments. */
BASALT_IMPORT(server_broadcast)
int32_t basalt_server_broadcast(const char *text, int32_t tlen, const char *args, int32_t alen, int32_t flags);

/* placeholder_fill: replaces every "%name%" of `text` with the registered placeholders' answers for the player (0 for
 * none), into `out`; length the full answer needs. STALE for a player that left. */
BASALT_IMPORT(placeholder_fill)
int32_t basalt_placeholder_fill(const char *text, int32_t tlen, int32_t player, char *out, int32_t cap);

/* server_info: a value map {software: "Basalt", version: text (empty when unknown), minecraft: text, protocol: i64,
 * tps_1m: f64, tps_5m: f64, tps_15m: f64, mean_tick_ms: f64, tick_rate: f64, frozen: bool, paused: bool,
 * online: i64, max_players: i64 (0 when unknown), plugins: [{id, name, version, disabled}], plugin: {id, name,
 * version, authors: [text], lane: 0, lanes: 1, folder: text}}. Length the full answer needs. */
BASALT_IMPORT(server_info) int32_t basalt_server_info(char *out, int32_t cap);

/* server_call: the administrative operations, by name, with one value in and one value out (`args` may be empty with
 * alen 0). Unknown operation or malformed arguments is INVALID; the result is the length the full answer needs. The
 * operations and their arguments (a map unless said) and results:
 *   settings.keys                          list of text
 *   settings.get {key}                     text, null when unknown
 *   settings.set {key, value}              bool (false when read only or the value does not fit)
 *   tick.rate | tick.set_rate {rate}       f64 | null
 *   tick.frozen | tick.set_frozen {frozen} bool | null
 *   tick.step {count} | tick.sprint {count}    bool
 *   tick.stop_sprint                       bool
 *   tick.pace                              {tps_1m, tps_5m, tps_15m, mean_tick_ms}
 *   tick.times                             list of i64 nanoseconds (the last hundred ticks)
 *   tick.paused                            bool
 *   links.list                             list of {kind, label, url}
 *   links.set {links: list of {kind, label, url}}    null
 *   access.is_banned | is_whitelisted | is_operator {who}    bool (who: uuid or name)
 *   access.known {who}                     {uuid, name, first_seen, last_seen} (millis) or null
 *   access.statistic {uuid, key}           i64
 *   access.set_statistic {uuid, key, value}   bool
 *   access.last_death {uuid}               {dimension, x, y, z} or null
 *   datapacks.list                         list of {id, enabled, required, compatibility, source}   (datapacks)
 *   datapacks.refresh                      null                                                       (datapacks)
 *   datapacks.resource {name}              bytes or null                                              (datapacks)
 *   datapacks.resources {prefix}           list of text                                               (datapacks)
 *   datapacks.resource_pack {name}         text or null                                               (datapacks)
 *   datapacks.functions | function_tags    list of text                                               (datapacks)
 *   datapacks.scheduled                    list of {name, trigger_time}                               (datapacks)
 *   datapacks.schedule {function, delay, replace}    bool                                             (datapacks)
 *   datapacks.unschedule {name}            i64                                                        (datapacks)
 *   stopwatches.list                       list of text
 *   stopwatches.create | restart | remove {id}    bool
 *   stopwatches.elapsed {id}               f64 seconds or null
 *   ops                                    list of every operation name above
 * Errors besides INVALID: DENIED for a datapacks.* operation without the capability. */
BASALT_IMPORT(server_call)
int32_t basalt_server_call(const char *op, int32_t olen, const char *args, int32_t alen, char *out, int32_t cap);

/* profile_lookup: looks a player name up on Mojang's services without stopping the server. Returns a request handle;
 * later basalt_callback(token, ctx) runs with a completion ctx whose value_result is {uuid, name, texture, signature}
 * or null when the name is unknown or the services are unreachable (texture and signature are empty when the account
 * has none; with them the value is about 2 KB, so read it into a 4 KB buffer). request_cancel drops it. INVALID for an empty name or too many pending requests. */
BASALT_IMPORT(profile_lookup) int32_t basalt_profile_lookup(const char *name, int32_t nlen, int32_t token);

/* ---- Storage, configuration and files ------------------------------------------------------------------------------
 * store_* is a plugin-wide (not per player) key-value store kept in the player database under the plugin's id and
 * written through at once. Values are null (delete), bool, i64, f64 or string; a list, map or bytes is INVALID. */

/* store_get: the value of `key` as a value document into `out` (length the full answer needs), ABSENT when the key is
 * not set, DENIED without capability `storage`. */
BASALT_IMPORT(store_get) int32_t basalt_store_get(const char *key, int32_t klen, char *out, int32_t cap);

/* store_set: stores the value, or deletes the key when the value is null. OK, DENIED, INVALID. */
BASALT_IMPORT(store_set) int32_t basalt_store_set(const char *key, int32_t klen, const char *value, int32_t vlen);

/* data_read: the JSON document data/<name>.json of the plugin folder (a name with a '/' or ending in ".json" is a path
 * in the data folder; ".." and absolute paths are INVALID) decoded as a value: objects are maps, arrays lists, numbers
 * i64 when whole and f64 otherwise. Length the full answer needs, ABSENT when the file does not exist, INVALID when it
 * is not valid JSON, DENIED without capability `storage`. */
BASALT_IMPORT(data_read) int32_t basalt_data_read(const char *name, int32_t nlen, char *out, int32_t cap);

/* data_write: writes the value (bytes are INVALID, map keys are the object keys) as JSON to the same file,
 * through a temporary file and a rename, creating folders. OK, INVALID, DENIED. */
BASALT_IMPORT(data_write) int32_t basalt_data_write(const char *name, int32_t nlen, const char *value, int32_t vlen);

/* config_value: the configuration (resources/config.toml, and every other .toml of the plugin folder as a table named
 * after its file) at a dotted key as a value: tables are maps, arrays lists, scalars as in config_*. The empty key is
 * the whole configuration. A key with '-' or '_' also finds the other spelling. Length the full answer needs, ABSENT
 * when there is no such key. */
BASALT_IMPORT(config_value) int32_t basalt_config_value(const char *key, int32_t klen, char *out, int32_t cap);

/* files_*: the plugin folder (plugins/<id>/), as seen through capability `files`, whose entries are the folders the
 * plugin may use ("." is the whole folder, "data" is data/ and below). A path is relative with '/' separators and
 * never has "..", a drive, a root or a backslash (INVALID); a path outside the granted folders is DENIED. Reading
 * falls back to the packaged resources/ of the plugin when the file is not in the folder yet. */

/* files_read: writes the file's bytes and returns the length the full answer needs; ABSENT when there is no file. */
BASALT_IMPORT(files_read) int32_t basalt_files_read(const char *path, int32_t plen, char *out, int32_t cap);

/* files_write: replaces the file through a temporary sibling and a rename, creating folders. OK, DENIED, INVALID, or
 * ABSENT when the write failed. */
BASALT_IMPORT(files_write) int32_t basalt_files_write(const char *path, int32_t plen, const char *data, int32_t dlen);

/* files_exists: 1 when the path exists in the folder or the packaged resources, else 0. */
BASALT_IMPORT(files_exists) int32_t basalt_files_exists(const char *path, int32_t plen);

/* files_list: a value list of the names of the packaged and live files whose path starts with the prefix (the empty
 * path lists everything), sorted. Length the full answer needs. */
BASALT_IMPORT(files_list) int32_t basalt_files_list(const char *prefix, int32_t plen, char *out, int32_t cap);

/* files_stat: {kind: "file", "directory" or "other", size: i64, modified: i64 millis} of a path in the plugin folder,
 * ABSENT when it does not exist. Length the full answer needs. */
BASALT_IMPORT(files_stat) int32_t basalt_files_stat(const char *path, int32_t plen, char *out, int32_t cap);

/* files_walk: every path under a folder, parents first, directories ending in a slash, as a value list. ABSENT when
 * the folder does not exist. */
BASALT_IMPORT(files_walk) int32_t basalt_files_walk(const char *path, int32_t plen, char *out, int32_t cap);

/* files_delete: removes a file or an empty folder (recursive 0), or a folder and everything in it (recursive 1).
 * OK, ABSENT when there is nothing there, INVALID, DENIED. */
BASALT_IMPORT(files_delete) int32_t basalt_files_delete(const char *path, int32_t plen, int32_t recursive);

/* files_capabilities: {atomic_move: bool, hard_links: bool} of the file system under a folder of the plugin folder. */
BASALT_IMPORT(files_capabilities) int32_t basalt_files_capabilities(const char *path, int32_t plen, char *out, int32_t cap);

/* ---- Services shared between plugins -------------------------------------------------------------------------------
 * A provider plugin lists the service names in plugin.toml [exports] services and registers a handler; consumers call
 * it by name with one value and get one value back, later. Calls cross plugin lanes: the handler never runs inside the
 * caller's callback. ORDER: calls to one service run in the order service_call was made, each on the provider's own
 * thread on the next tick after the caller's current callback returned, and the caller's completion callback runs right
 * after the handler returned. TIMEOUT: a call the provider has not answered within `timeout` ticks (a disabled provider, a
 * handler that failed) completes with {ok: false, error: "timeout"}; an answer after that is dropped. */

/* service_register: offers `name` (it must be in this plugin's [exports] services, else DENIED; a taken name or an
 * empty one is INVALID). Returns a service handle. The handler runs with a completion-kind ctx whose value_result is
 * the call {name: text, caller: plugin id, args: value}; it answers inside the call with service_reply and cannot wait.
 * The service disappears when the plugin unloads or the handle is removed. */
BASALT_IMPORT(service_register) int32_t basalt_service_register(const char *name, int32_t nlen, int32_t token);

/* service_reply: sets the value the caller receives (null is allowed). A second reply replaces the first. OK, STALE,
 * INVALID for a ctx that is not a service call or a bad value. */
BASALT_IMPORT(service_reply) int32_t basalt_service_reply(int32_t ctx, const char *value, int32_t vlen);

/* service_remove: unregisters a service. OK, or ABSENT for an unknown handle. */
BASALT_IMPORT(service_remove) int32_t basalt_service_remove(int32_t service);

/* service_has: 1 when some loaded plugin offers the service, else 0. */
BASALT_IMPORT(service_has) int32_t basalt_service_has(const char *name, int32_t nlen);

/* service_list: a value list of every service offered by loaded plugins, sorted. */
BASALT_IMPORT(service_list) int32_t basalt_service_list(char *out, int32_t cap);

/* service_call: calls `name` with the value `args` (alen 0 for null). Returns a request handle; basalt_callback(token,
 * ctx) later runs with a completion ctx whose value_result is {ok: true, value: value} or {ok: false, error: text}
 * (error is "unavailable" for a service nobody offers, "timeout", or "failed" when the handler trapped). `timeout` is
 * in ticks, 0 for 100. request_cancel drops the call if the handler has not run yet. INVALID for an empty name or a bad
 * value, ABSENT for a service nobody offers right now. */
BASALT_IMPORT(service_call)
int32_t basalt_service_call(const char *name, int32_t nlen, const char *args, int32_t alen, int32_t timeout, int32_t token);

/* ---- Scheduler completions ----------------------------------------------------------------------------------------- */

/* task_wait: the completion form of a wait: calls basalt_callback(token, ctx) after `ticks` ticks (at least 1)
 * with a completion ctx whose value_result is null. Returns a request handle; request_cancel drops it. */
BASALT_IMPORT(task_wait) int32_t basalt_task_wait(int32_t ticks, int32_t token);

/* task_at: calls basalt_callback(token, ctx) once the server runs the code that owns the place (the region of a world
 * position), with a completion ctx whose value_result is null. The world is a name. Returns a request handle;
 * ABSENT for an unknown world, INVALID for an empty one. */
BASALT_IMPORT(task_at) int32_t basalt_task_at(const char *world, int32_t wlen, double x, double z, int32_t token);

#ifdef __cplusplus
}
#endif
#endif
