/* Proxy area P5: backends (register, unregister, ping), the console and chat spoofing, permission providers, checks and
 * groups, component JSON text, the plugin store, per-plugin data by holder (online or not) and a waiting completion.
 * Module "basalt_proxy". Everything follows basalt_abi.h and basalt_proxy_abi.h; values use basalt_values.h and the
 * completions of core_values.h. Every import runs on the single proxy plugin thread, inside a callback, except where an
 * import says it is also asked from other threads.
 *
 * Capabilities (plugin.toml [capabilities]): storage gates store_get, store_set and store_top; players_data gates
 * player_data_get and player_data_set; commands gates console_run and also needs the operator to grant "commands" to
 * the plugin under [plugins.allow] in proxy.toml. An import without its capability returns BASALT_ERR_DENIED.
 *
 * ---- Permission providers -------------------------------------------------------------------------------------
 *
 * permission_provide registers one or two callbacks that join the proxy's permission chain. The chain asks them from
 * ANY thread (the I/O threads of players, the console, commands), while a plugin runs on its single thread, so a
 * question is queued to the plugin thread, ahead of the other queued work, and the asker waits at most 25 ms. The
 * callback gets a ctx; permission_request describes the question and permission_answer (check) or
 * permission_answer_text (meta) sets the reply on that ctx. An answer that arrives after the asker stopped waiting, or
 * after the ctx's callback ended, is ignored (the setters return STALE) and the chain goes on to the next provider, as it
 * does when the callback sets nothing, which is the "undefined" answer. A check asked from inside any plugin callback
 * (permission_check, player_has_permission, the permission gate of a plugin command) is answered by the proxy's own
 * providers only, never by a plugin's: plugin callbacks do not nest.
 */
#ifndef BASALT_AREA_PROXY_ADMIN_H
#define BASALT_AREA_PROXY_ADMIN_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- Backends ------------------------------------------------------------------------------------------------- */

/* servers_register: adds a backend `name` at host:port (1..65535) until the proxy stops, copying the settings (forwarding,
 * timeouts, protocol range) of the backend `like` ("" copies none). The names listed in basalt_proxy_servers_list are the
 * proxy's own, so register only a name that is not there. Returns OK, or INVALID with the reason (UTF-8, truncated to
 * `ecap`, not NUL terminated, the byte count is not returned) written to `err` when `ecap` is positive. */
BASALT_PROXY_IMPORT(servers_register)
int32_t basalt_proxy_servers_register(const char *name, int32_t nlen, const char *host, int32_t hlen, int32_t port,
                                      const char *like, int32_t llen, char *err, int32_t ecap);

/* servers_unregister: removes a backend added by servers_register. OK; ABSENT when no backend has that name; DENIED when
 * it is one of servers.toml, which a plugin may not remove. */
BASALT_PROXY_IMPORT(servers_unregister) int32_t basalt_proxy_servers_unregister(const char *name, int32_t nlen);

/* servers_ping: asks backend `name` for its server list status. Returns a request handle (core_values.h); later
 * basalt_callback(token, ctx) runs on the plugin thread and basalt_value_result gives a map with motd (plain text),
 * motd_json (string), online, max_players, protocol, latency (ints, latency in ms) and version (string), or null when
 * the backend did not answer. ABSENT when the backend does not exist or the ping cannot start, INVALID for a bad name,
 * INVALID when too many requests are pending. Cancel with basalt_request_cancel. */
BASALT_PROXY_IMPORT(servers_ping) int32_t basalt_proxy_servers_ping(const char *name, int32_t nlen, int32_t token);

/* ---- Console and chat ----------------------------------------------------------------------------------------- */

/* console_run: runs `line` (no slash, at most 4096 bytes) as the console would and logs "ran /line" under the plugin. 1
 * when the line named a command, 0 when it did not; DENIED without the commands capability and the operator's grant.
 * It reaches the commands that plugins registered and the proxy's own; the answer of the command goes to the log. */
BASALT_PROXY_IMPORT(console_run) int32_t basalt_proxy_console_run(const char *line, int32_t len);

/* player_chat: makes the player say `text` (cut at 256 bytes), or run it as a command when it starts with '/'; unsigned, so
 * the proxy and the backend see a message the client did not sign. OK once handed over, INVALID for an empty text or when
 * the player's connection cannot take it, STALE for a player that left. */
BASALT_PROXY_IMPORT(player_chat) int32_t basalt_proxy_player_chat(int32_t player, const char *text, int32_t len);

/* ---- Permissions ---------------------------------------------------------------------------------------------- */

/* permission_check: asks the chain whether `who` has `node`. `who` is a player name, exact spelling first and then
 * ignoring case, or "" for the console; a player that is not connected is asked by name alone. `context` is an optional
 * map (length 0 for none): server and world (strings, the server defaults to the player's backend) and any number of other
 * string entries that providers read. Returns 1 granted, 0 denied, ABSENT when nothing in the chain decided (the proxy
 * counts that as denied). INVALID for a context that is not a map of strings. */
BASALT_PROXY_IMPORT(permission_check)
int32_t basalt_proxy_permission_check(const char *who, int32_t wlen, const char *node, int32_t nlen,
                                      const char *context, int32_t clen);

/* permission_meta: the text option `key` (prefix, suffix or any other) of `who`, same naming and context as
 * permission_check. Writes the text to `out`; returns the length the full answer needs, written only when it fits in
 * `cap`; ABSENT when nothing provides it. */
BASALT_PROXY_IMPORT(permission_meta)
int32_t basalt_proxy_permission_meta(const char *who, int32_t wlen, const char *key, int32_t klen, const char *context,
                                     int32_t clen, char *out, int32_t cap);

/* permission_provide: puts a provider in the chain. `check_token` (0 for none) is called as basalt_callback(token, ctx)
 * to answer a check, `meta_token` (0 for none) to answer a meta question; at least one is needed. `priority` is above the
 * proxy's default provider and decides the order among plugins, highest first; 0 means the plugin default (100).
 * Returns the provider handle (positive i32), INVALID when both tokens are 0 or the priority is negative, ABSENT when
 * the proxy has no permission chain. The provider goes away with permission_unprovide or when the plugin is disabled. */
BASALT_PROXY_IMPORT(permission_provide)
int32_t basalt_proxy_permission_provide(int32_t check_token, int32_t meta_token, int32_t priority);

/* permission_unprovide: takes the provider out of the chain. OK, or STALE for a handle already removed. */
BASALT_PROXY_IMPORT(permission_unprovide) int32_t basalt_proxy_permission_unprovide(int32_t provider);

/* permission_request: inside a provider callback, writes a map describing the question: kind ("check" or "meta"), name,
 * uuid (strings, uuid is "" when unknown), console (bool), text (the node of a check, the key of a meta) and context (a
 * map with server, world and any extra string entries). Needed-length convention. STALE after the callback or when the
 * asker stopped waiting, INVALID when ctx is not a provider question. */
BASALT_PROXY_IMPORT(permission_request) int32_t basalt_proxy_permission_request(int32_t ctx, char *out, int32_t cap);

/* permission_answer: answers a check question: 1 grants, 0 denies, any negative value leaves it undefined (the next
 * provider decides). The last call before the callback returns counts. OK, INVALID when ctx is a meta question or not a
 * provider question, STALE when the answer is late. */
BASALT_PROXY_IMPORT(permission_answer) int32_t basalt_proxy_permission_answer(int32_t ctx, int32_t answer);

/* permission_answer_text: answers a meta question with `text` (up to 4096 bytes). Same results as permission_answer;
 * a call with a negative length leaves the answer undefined. */
BASALT_PROXY_IMPORT(permission_answer_text)
int32_t basalt_proxy_permission_answer_text(int32_t ctx, const char *text, int32_t len);

/* permission_changed: tells the proxy that what the providers answer changed, so it rebuilds what it derived from them.
 * OK; ABSENT when the proxy has no permission chain. */
BASALT_PROXY_IMPORT(permission_changed) int32_t basalt_proxy_permission_changed(void);

/* permission_override: grants (1), denies (0) or removes (-1) `node` for the player named `name` (connected or not)
 * until the proxy stops; the override is asked before the groups. OK, INVALID for another value or an empty node. */
BASALT_PROXY_IMPORT(permission_override)
int32_t basalt_proxy_permission_override(const char *name, int32_t nlen, const char *node, int32_t olen, int32_t value);

/* permission_list: writes the nodes the player named `name` has right now (groups and overrides) as a value list of
 * strings, sorted. Needed-length convention. */
BASALT_PROXY_IMPORT(permission_list)
int32_t basalt_proxy_permission_list(const char *name, int32_t nlen, char *out, int32_t cap);

/* group_get: writes the name of the player's group, "" in length when none. Needed-length convention. */
BASALT_PROXY_IMPORT(group_get) int32_t basalt_proxy_group_get(const char *name, int32_t nlen, char *out, int32_t cap);

/* group_set: puts the player named `name` in `group`; an empty group clears it. OK, INVALID when the player never
 * joined the proxy or the group is not defined in groups.toml. */
BASALT_PROXY_IMPORT(group_set)
int32_t basalt_proxy_group_set(const char *name, int32_t nlen, const char *group, int32_t glen);

/* ---- Component JSON text -------------------------------------------------------------------------------------- */

/* player_send_json: sends a chat line given as a text component in JSON (at most 65536 bytes). OK, STALE for a player
 * that left, INVALID when the text is not a valid component. */
BASALT_PROXY_IMPORT(player_send_json)
int32_t basalt_proxy_player_send_json(int32_t player, const char *json, int32_t len);

/* player_kick_json: disconnects the player with a reason given as component JSON. Results as player_send_json. */
BASALT_PROXY_IMPORT(player_kick_json)
int32_t basalt_proxy_player_kick_json(int32_t player, const char *json, int32_t len);

/* ---- Plugin store and per-plugin data ------------------------------------------------------------------------- */

/* store_get: writes the value of `key` in the plugin's own store (shared by every player and kept in the proxy's plugin
 * database), a bool, int, float or string value (the encoding of basalt_values.h). A float without a fraction reads back
 * as an int. Needed-length convention. ABSENT when the key is not set, DENIED without the storage capability, INVALID
 * for an empty key or one over 128 bytes or when the proxy has no plugin database. */
BASALT_PROXY_IMPORT(store_get)
int32_t basalt_proxy_store_get(const char *key, int32_t klen, char *out, int32_t cap);

/* store_set: sets `key` to the encoded value (value, vlen), a bool, int, float or string of at most 65536 bytes; a null
 * value deletes the key. A plugin may hold 1000 keys here. OK, DENIED, INVALID for a key as above, another type, an
 * oversized value or the 1001st key. */
BASALT_PROXY_IMPORT(store_set)
int32_t basalt_proxy_store_set(const char *key, int32_t klen, const char *value, int32_t vlen);

/* store_top: the best `limit` (1..1000, default 10 for a value under 1) numeric values of `key` across the plugin's store
 * and the data of every player, highest first, as a list of maps with uuid ("server" for the store), name (the player's
 * name when connected, else the uuid) and value (int or float). Rows whose value is not a number are skipped. Needed-length
 * convention; DENIED without the storage capability, INVALID as store_get. */
BASALT_PROXY_IMPORT(store_top)
int32_t basalt_proxy_store_top(const char *key, int32_t klen, int32_t limit, char *out, int32_t cap);

/* player_data_get: reads `key` of the plugin's data for the holder, the player's UUID text as the proxy knows it (a
 * player that never reported one is held by name), whether the player is connected or not. Same value encoding, results
 * and key limits as store_get, with the capability players_data. */
BASALT_PROXY_IMPORT(player_data_get)
int32_t basalt_proxy_player_data_get(const char *holder, int32_t hlen, const char *key, int32_t klen, char *out,
                                     int32_t cap);

/* player_data_set: writes or, with a null value, deletes `key` for the holder. Same results as store_set, with a
 * limit of 1000 keys per holder and plugin. */
BASALT_PROXY_IMPORT(player_data_set)
int32_t basalt_proxy_player_data_set(const char *holder, int32_t hlen, const char *key, int32_t klen, const char *value,
                                     int32_t vlen);

/* ---- Waiting -------------------------------------------------------------------------------------------------- */

/* wait: returns a request handle whose completion callback basalt_callback(token, ctx) runs after `ticks` ticks of 50 ms
 * (0 is one tick) with a null value_result. INVALID for negative ticks or when too many requests are pending; cancel with
 * basalt_request_cancel. */
BASALT_PROXY_IMPORT(wait) int32_t basalt_proxy_wait(int32_t ticks, int32_t token);

#ifdef __cplusplus
}
#endif
#endif
