/* Basalt proxy plugin ABI, version 1. The normative definition of what a WebAssembly plugin of Basalt Proxy may import
 * and export.
 *
 * A proxy plugin follows every convention of basalt_abi.h (pointers are offsets into the plugin's own memory, strings
 * are UTF-8 pointer plus length, buffer-returning calls give back the length the full answer needs, negative results
 * are error codes, tokens map callbacks back to the plugin, text is MiniMessage with the '@' lang key rule). It
 * imports from two modules:
 *
 *   "basalt"        the generic calls whose meaning is identical on the proxy, listed under "Shared imports"
 *   "basalt_proxy"  what only a proxy has: backends, the player's current backend, moving a player, decisions
 *
 * A module that imports anything of the server profile that is not listed below (menus, placeholders, particles,
 * sounds, store_top) does not validate as a proxy plugin, and the other way round.
 *
 * Exports are the ones of basalt_abi.h: memory, basalt_abi_version, basalt_init and basalt_callback.
 */
#ifndef BASALT_PROXY_ABI_H
#define BASALT_PROXY_ABI_H

#include "basalt_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BASALT_PROXY_ABI_VERSION 1

#define BASALT_PROXY_IMPORT(name) __attribute__((import_module("basalt_proxy"), import_name(#name)))

/* ---- Shared imports (module "basalt"), declared in basalt_abi.h ----------------------------------------------------
 *
 *   logging         basalt_log
 *   configuration   basalt_config_i64, basalt_config_f64, basalt_config_bool, basalt_config_str
 *   text            basalt_translate
 *   players         basalt_player_by_name, basalt_players_online, basalt_player_name, basalt_player_send,
 *                   basalt_player_action_bar
 *   player data     basalt_data_get_i64/set_i64/get_bool/set_bool/get_str/set_str, basalt_data_remove
 *                   (capability players_data; the key space is the player's UUID in the plugin database)
 *   events          basalt_event_on, basalt_event_player, basalt_event_field_str, basalt_event_field_i64,
 *                   basalt_event_cancel
 *   commands        basalt_command_register, basalt_command_source_name, basalt_command_is_console,
 *                   basalt_command_reply, basalt_command_arg_str, basalt_command_arg_i64
 *   tasks           basalt_task_after, basalt_task_every, basalt_task_cancel
 *
 * Differences from the server profile are only in what the names and handles refer to:
 *   - A player handle is a player connected to the proxy. It goes stale when the player leaves the proxy, not when
 *     they change backend.
 *   - Event names are the proxy ones listed under "Events". basalt_event_player gives 0 for events about someone who
 *     is not connected yet (player.login) and for a console command.
 *   - A command source that is a player also answers basalt_event_player(ctx) inside the command callback.
 *   - Delays and periods of tasks are in ticks of 50 ms of wall time, since a proxy has no world tick.
 *   - Callbacks run on the single plugin thread, one at a time across all plugins, never inside another callback;
 *     the exceptions are packet listeners and permission providers (see proxy_network.h and proxy_admin.h).
 */

/* ---- Events (names for basalt_event_on) ----------------------------------------------------------------------------
 *
 * Observed events, handlers cannot change the outcome:
 *   plugin.enable, plugin.disable     once, after the proxy started or reloaded / before it stops or reloads
 *   player.join                       a player entered the network (first backend connected)
 *   player.server_change              a player moved to another backend
 *   player.quit                       a player left; the handle still works inside the callback
 *
 * Decided events, handlers run by priority (lowest first), each one sees what the previous ones set, and the proxy
 * waits for them within its decision deadline (a late answer is ignored):
 *   player.login                      before the player is let in
 *   player.server_pre_connect         before a connection to a backend
 *   server.ping                       a server list ping, before the answer is sent
 *   player.chat, player.command, player.kick   decided too, see proxy_events.h
 *
 * Fields read with basalt_event_field_str / basalt_event_field_i64 (ABSENT where the event does not have it):
 *   player_name, player_uuid           str   every player event
 *   address                            str   login, join
 *   port                               i64   login, join
 *   host                               str   login, server_pre_connect, server.ping (the name the client typed)
 *   server                             str   join, quit, server_change: the player's backend now;
 *                                            server_pre_connect: the backend about to be connected
 *   from_server                        str   server_change, server_pre_connect: the backend left, empty on the first
 *   protocol                           str/i64  server.ping: the client's protocol version
 *   cancelled                          i64   decided events, 1 once an earlier handler cancelled
 *   reason                             str   login, server_pre_connect: the denial message set so far
 *   motd, version, sample, favicon     str   server.ping: what earlier handlers set, empty when untouched
 *   max_players, online, version_protocol  i64  server.ping: -1 when untouched
 *
 * Fields written with the setters below (INVALID on an observed event, DENIED never):
 *   cancel (basalt_event_cancel)       login, server_pre_connect: deny; the player is disconnected with `reason`
 *   reason       str  MiniMessage shown to the player when cancelled (a '@' lang key is accepted)
 *   server       str  server_pre_connect: connect to this backend instead; INVALID when it does not exist
 *   motd         str  server.ping: MiniMessage description
 *   version      str  server.ping: the version name in the server list
 *   sample       str  server.ping: the hover lines, one per '\n', at most 32 lines of 256 bytes
 *   favicon      str  server.ping: a data:image/png;base64 URI, at most 64 KiB, empty removes the icon
 *   max_players, online, version_protocol  i64  server.ping
 */
BASALT_PROXY_IMPORT(event_set_str)
int32_t basalt_proxy_event_set_str(int32_t ctx, const char *field, int32_t flen, const char *value, int32_t vlen);
BASALT_PROXY_IMPORT(event_set_i64)
int32_t basalt_proxy_event_set_i64(int32_t ctx, const char *field, int32_t flen, int64_t value);

/* ---- Imports: players -----------------------------------------------------------------------------------------
 *
 * Every call takes a player handle from basalt_player_by_name, basalt_players_online, basalt_event_player or
 * basalt_proxy_players_on, and answers STALE once the player left.
 */
BASALT_PROXY_IMPORT(player_uuid) int32_t basalt_proxy_player_uuid(int32_t player, char *out, int32_t cap);
BASALT_PROXY_IMPORT(player_server) int32_t basalt_proxy_player_server(int32_t player, char *out, int32_t cap);
BASALT_PROXY_IMPORT(player_ping) int32_t basalt_proxy_player_ping(int32_t player); /* milliseconds, ABSENT unknown */

/* The client information packet: `field` is "locale", "chat_mode" ("full", "system", "hidden"), "main_hand",
 * "particles" for the string call and "view_distance", "chat_colors", "skin_parts", "text_filtering",
 * "server_listing" for the number call (booleans are 0 or 1). ABSENT until the client has sent it. */
BASALT_PROXY_IMPORT(player_setting_str)
int32_t basalt_proxy_player_setting_str(int32_t player, const char *field, int32_t flen, char *out, int32_t cap);
BASALT_PROXY_IMPORT(player_setting_i64)
int32_t basalt_proxy_player_setting_i64(int32_t player, const char *field, int32_t flen, int64_t *out);

/* 1 when the proxy's permission chain grants the node to the player, 0 when it does not. */
BASALT_PROXY_IMPORT(player_has_permission)
int32_t basalt_proxy_player_has_permission(int32_t player, const char *node, int32_t nlen);

/* Title and subtitle (MiniMessage, '@' lang key accepted, length 0 leaves that part alone). The three times are in
 * ticks; a negative value of any of them means the client default and none are sent when all are negative. */
BASALT_PROXY_IMPORT(player_title)
int32_t basalt_proxy_player_title(int32_t player, const char *title, int32_t tlen, const char *subtitle, int32_t slen,
                                  int32_t fade_in, int32_t stay, int32_t fade_out);

/* Moves the player to a backend. OK once handed over, INVALID when the backend does not exist or refuses. */
BASALT_PROXY_IMPORT(player_connect)
int32_t basalt_proxy_player_connect(int32_t player, const char *server, int32_t slen);

/* Disconnects the player with a reason: MiniMessage or '@' lang key, arguments as in basalt_abi.h. */
BASALT_PROXY_IMPORT(player_kick)
int32_t basalt_proxy_player_kick(int32_t player, const char *reason, int32_t rlen, const char *args, int32_t alen);

/* ---- Imports: backends ----------------------------------------------------------------------------------------- */
BASALT_PROXY_IMPORT(servers_list) int32_t basalt_proxy_servers_list(char *out, int32_t cap); /* names, one per '\n' */
BASALT_PROXY_IMPORT(server_address)
int32_t basalt_proxy_server_address(const char *name, int32_t nlen, char *out, int32_t cap); /* "host:port" */

/* The players connected to a backend, as handles; the count is returned whatever `cap` is. */
BASALT_PROXY_IMPORT(players_on)
int32_t basalt_proxy_players_on(const char *server, int32_t slen, int32_t *out, int32_t cap);

#ifdef __cplusplus
}
#endif

#endif /* BASALT_PROXY_ABI_H */
