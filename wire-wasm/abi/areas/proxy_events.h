/* Proxy area P1: events as values, chat, command and kick decisions, plugin identity and configuration, typed command
 * arguments. Module "basalt_proxy". Everything here follows basalt_abi.h and basalt_proxy_abi.h; values are the
 * encoding of basalt_values.h. Every import runs on the single proxy plugin thread, inside a callback.
 *
 * ---- Events -------------------------------------------------------------------------------------------------------
 *
 * Names accepted by basalt_event_on, now including three more decided events:
 *   player.chat      a chat message of a player in play, before it is relayed to the backend
 *   player.command   a slash command typed by a player (without the slash), before it is dispatched
 *   player.kick      a backend kicked a player in play, before the proxy disconnects them
 *
 * A handler registered with basalt_event_on on a decided event (login, server_pre_connect, chat, command, kick, ping) is
 * a decider: handlers run by priority, each one sees what the previous ones set, the proxy waits for them within its
 * decision deadline, and a late answer is ignored. Cancelling does not stop later handlers; they see cancelled = true
 * and may undo it. basalt_proxy_event_watch registers an observer of the finished event instead: it runs after the
 * proxy acted, its ctx is read only, and its answer changes nothing.
 *
 * The fields of an event (name, value type, which events, writable) are:
 *
 *   player_name      string  every player event
 *   player_uuid      string  every player event
 *   address          string  login, join, chat, command, kick, server_pre_connect (the client address)
 *   port             int     same events as address
 *   host             string  decided events (the name the client typed to connect)
 *   server           string  join, quit, server_change, chat, command: the backend the player is on now; observed
 *                            kick: the backend; server_pre_connect and kick (decider): the backend to connect to,
 *                            writable, the name of a backend that exists
 *   from_server      string  server_change, server_pre_connect, kick: the backend the player is leaving
 *   message          string  chat: the line; writable on a decider, the proxy relays the new text
 *   command          string  command: the line without the slash; writable on a decider
 *   signed           bool    chat, command: the client signed the content, which the proxy may refuse to rewrite
 *   cancelled        bool    login, server_pre_connect, chat, command: writable on a decider; kick has no such field
 *                            because a kick cannot be undone (answer a redirect with server instead)
 *   reason           string  login, server_pre_connect, chat, command: the text shown to the player when cancelled;
 *                            kick: the kick text, writable to replace it; MiniMessage, a leading '@' is a lang key
 *   reason_json      string  write only, same events as reason: the same text as component JSON
 *   protocol         int     ping: the client's protocol version
 *   motd, version, favicon  string  ping, writable on the decider (MiniMessage, version name, data:image/png URI)
 *   motd_json        string  ping, write only: the description as component JSON
 *   sample           list    ping, writable: up to 32 strings of at most 256 bytes, the hover lines
 *   max_players, online, version_protocol  int  ping, writable, -1 when no handler set it
 *
 * Observed events (basalt_proxy_event_watch) offer: join, quit, server_change (player_name, player_uuid, server,
 * address, port, from_server), chat (adds message), kick (adds reason, the plain text of the kick).
 */
#ifndef BASALT_AREA_PROXY_EVENTS_H
#define BASALT_AREA_PROXY_EVENTS_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* event_get: writes into `out` the value of `field` in ctx (the encoding of basalt_values.h) and returns the length the
 * full answer needs, written only when it fits in `cap`. An empty field name answers a map of every field the event
 * has right now (write only fields are left out). ABSENT when the event does not have the field, STALE once the
 * callback returned, INVALID for a live ctx that is not an event (a command) or a field that is not valid text. */
BASALT_PROXY_IMPORT(event_get)
int32_t basalt_proxy_event_get(int32_t ctx, const char *field, int32_t flen, char *out, int32_t cap);

/* event_set: sets `field` of a decided event to the encoded value (value, vlen). OK, STALE for a dead ctx, ABSENT for
 * a field the event does not have (INVALID for a ctx that is not an event), INVALID when the event is observed, the field is read only, the value has another
 * type than the field (a bool accepts true or false only, an int an integer, a string a string, sample a list of strings),
 * the text is over 4096 bytes, a server is not a backend, or a ping number is negative. Setting reason or reason_json
 * replaces the other, so the last one set wins. Text fields set to null are INVALID. */
BASALT_PROXY_IMPORT(event_set)
int32_t basalt_proxy_event_set(int32_t ctx, const char *field, int32_t flen, const char *value, int32_t vlen);

/* event_fields: writes a list of maps, one per field of ctx, with the keys name (string), type ("string", "int", "bool",
 * "list"), readable (bool) and writable (bool; false on an observed event). Same needed-length convention and errors as
 * event_get. The table behind it is the one event_get and event_set check against, so it is always complete. */
BASALT_PROXY_IMPORT(event_fields)
int32_t basalt_proxy_event_fields(int32_t ctx, char *out, int32_t cap);

/* event_watch: registers `token` to run basalt_callback(token, ctx) once per finished event named `name` (join, quit,
 * server_change, chat, kick). Observers run in registration order after the proxy acted and the ctx is read only.
 * OK, INVALID for an unknown name or one that cannot be observed (login, server_pre_connect, command, ping, plugin.*,
 * which are decided or lifecycle events). Queued like the other observed events: a full queue drops the event. */
BASALT_PROXY_IMPORT(event_watch) int32_t basalt_proxy_event_watch(const char *name, int32_t nlen, int32_t token);

/* ---- Identity, manifest and configuration --------------------------------------------------------------------- */

/* plugin_info: writes the UTF-8 text of `field`, one of "id", "name" (the id when plugin.toml has none) or "version",
 * and returns the length the full answer needs, written only when it fits in `cap`. INVALID for another field. */
BASALT_PROXY_IMPORT(plugin_info) int32_t basalt_proxy_plugin_info(const char *field, int32_t flen, char *out, int32_t cap);

/* plugin_manifest: writes a map with id, name, version (strings), authors (list of strings), capabilities (list of the
 * names declared in plugin.toml) and depends (list of maps with id, requirement and hard) as read from plugin.toml.
 * Needed-length convention; never fails except INVALID when the document cannot be built. */
BASALT_PROXY_IMPORT(plugin_manifest) int32_t basalt_proxy_plugin_manifest(char *out, int32_t cap);

/* config_value: writes the value at `key` of config.toml (dots walk nested tables, an empty key is the whole file) as a
 * value: strings, integers, floats, booleans, lists and maps, with TOML arrays as lists and tables as maps. The file
 * limits.toml next to config.toml, when present, is readable under the key "limits". ABSENT when the key does not exist;
 * dashed keys are also found with underscores. Needed-length convention. */
BASALT_PROXY_IMPORT(config_value)
int32_t basalt_proxy_config_value(const char *key, int32_t klen, char *out, int32_t cap);

/* ---- Commands ------------------------------------------------------------------------------------------------- */

/* command_arg_f64: reads the float argument `name` of the command being run (an int argument converts). OK, ABSENT for
 * an omitted optional or unknown name, INVALID when the argument is not a number, STALE outside a command callback. */
BASALT_PROXY_IMPORT(command_arg_f64)
int32_t basalt_proxy_command_arg_f64(int32_t ctx, const char *name, int32_t nlen, double *out);

/* command_arg_bool: reads the bool argument `name` (the words true and false) into *out as 0 or 1. Results as above. */
BASALT_PROXY_IMPORT(command_arg_bool)
int32_t basalt_proxy_command_arg_bool(int32_t ctx, const char *name, int32_t nlen, int32_t *out);

/* command_args: writes a map with every declared argument of the running command, null for an omitted optional one,
 * with typed values (string, int, float, bool). Needed-length convention; STALE outside a command callback. */
BASALT_PROXY_IMPORT(command_args) int32_t basalt_proxy_command_args(int32_t ctx, char *out, int32_t cap);

#ifdef __cplusplus
}
#endif
#endif
