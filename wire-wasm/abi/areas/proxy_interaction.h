/* Proxy area P3: dialogs, resource packs, cookies, client transfer and the extra player operations the Lua API had.
 * Module "basalt_proxy". Everything follows basalt_abi.h and basalt_proxy_abi.h; values are the encoding of
 * basalt_values.h. Every import runs on the single proxy plugin thread, inside a callback.
 *
 * Player handles are the ones of basalt_player_by_name, basalt_players_online, basalt_event_player and
 * basalt_proxy_players_on. A call that targets a player answers STALE once the player left. "Handed over" calls answer
 * OK when the proxy accepted the request for the player's connection and INVALID when it could not (the player is in a
 * phase that cannot show it, or the proxy has no such facility), like the other player calls of the proxy profile.
 * Text arguments are MiniMessage, a leading '@' is a lang key of the plugin, and a text is cut at 4096 bytes.
 *
 * ---- Observed events -------------------------------------------------------------------------------------------
 *
 * basalt_proxy_interaction_watch registers an observer of two events the proxy reports from the client. The callback
 * context answers basalt_value_result with a map:
 *   player.resource_pack   player (int, a player handle), player_name (string), id (string, the pack UUID text),
 *                          status (string: successfully_loaded, declined, failed_download, accepted, downloaded,
 *                          invalid_url, failed_reload or discarded)
 *   player.settings        player, player_name, settings (the map of basalt_proxy_player_settings), sent when the
 *                          client information arrives or changes
 * The handle in "player" is valid inside the callback. Observers run in registration order after the proxy handled
 * the event and cannot change anything.
 *
 * ---- Dialogs ---------------------------------------------------------------------------------------------------- */
#ifndef BASALT_AREA_PROXY_INTERACTION_H
#define BASALT_AREA_PROXY_INTERACTION_H
#include "../basalt_abi.h"
#include "../basalt_proxy_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* interaction_watch: registers `token` to run basalt_callback(token, ctx) once per event named `name`
 * ("player.resource_pack" or "player.settings"). OK, INVALID for another name. ctx answers basalt_value_result with the
 * map above and is STALE once the callback returned. A full proxy queue drops the event like the other observed
 * events; the proxy keeps no history for plugins that register late. */
BASALT_PROXY_IMPORT(interaction_watch)
int32_t basalt_proxy_interaction_watch(const char *name, int32_t nlen, int32_t token);

/* dialog_show: opens a dialog on the player's screen from one value, a map with:
 *   kind                 string, "notice" (default), "confirmation" or "multi_action"
 *   title                string, MiniMessage
 *   body                 string or list of strings (at most 32 lines, MiniMessage), optional
 *   buttons              list of maps {label (string, MiniMessage), command (string, at most 256 bytes)}, at most 16
 *                        entries; a button runs its command as the player when clicked, there is no callback
 *   can_close_with_escape  bool, default true
 * Unknown keys are ignored. OK once handed over; STALE for a player who left; INVALID for a value that is not a map, a
 * kind that is none of the three, a title that is not a string, a body or button of another shape, or a refused
 * request. */
BASALT_PROXY_IMPORT(dialog_show)
int32_t basalt_proxy_dialog_show(int32_t player, const char *value, int32_t vlen);

/* dialog_clear: closes any dialog the proxy showed. OK, STALE, INVALID when it could not be handed over. */
BASALT_PROXY_IMPORT(dialog_clear) int32_t basalt_proxy_dialog_clear(int32_t player);

/* ---- Resource packs --------------------------------------------------------------------------------------------- */

/* resource_pack_push: offers a pack to the player. `value` is a map with url (string, required), hash (string, the
 * SHA-1 as 40 hex digits or empty), required (bool, default false), prompt (string MiniMessage, optional) and id
 * (string: a UUID, or any other text that becomes the name-based UUID of "basalt:resource_pack:<text>"; the default is
 * the url). The pack id as UUID text is written to `out` with the needed-length convention (the status of the call
 * when it fails: STALE for a player who left, INVALID for a missing url, a hash that is not 40 hex digits, a bad value
 * or a refused request). The client answers with player.resource_pack events carrying that id. */
BASALT_PROXY_IMPORT(resource_pack_push)
int32_t basalt_proxy_resource_pack_push(int32_t player, const char *value, int32_t vlen, char *out, int32_t cap);

/* resource_pack_remove: pops one pack by id (the same id rule: UUID text, or text hashed to a UUID). OK, STALE,
 * INVALID when the request was refused. */
BASALT_PROXY_IMPORT(resource_pack_remove)
int32_t basalt_proxy_resource_pack_remove(int32_t player, const char *id, int32_t idlen);

/* resource_pack_clear: removes every pack the proxy pushed. OK, STALE, INVALID when refused. */
BASALT_PROXY_IMPORT(resource_pack_clear) int32_t basalt_proxy_resource_pack_clear(int32_t player);

/* ---- Cookies and transfer --------------------------------------------------------------------------------------- */

/* cookie_set: stores `data` (arbitrary bytes, at most 5120) under the cookie key (an identifier such as
 * "plugin:session", at most 256 bytes) in the client for its next connection. OK, STALE, INVALID for an empty or over
 * long key or data over the limit, or a refused request. */
BASALT_PROXY_IMPORT(cookie_set)
int32_t basalt_proxy_cookie_set(int32_t player, const char *key, int32_t klen, const char *data, int32_t dlen);

/* cookie_request: asks the client for the cookie `key`. Returns a request handle (positive) and later calls
 * basalt_callback(token, ctx), whose basalt_value_result is the cookie as bytes, or null when the client has none or
 * never answered within the proxy's wait. The handle is STALE after the callback and can be dropped with
 * basalt_request_cancel. Errors: STALE for a player who left, INVALID for an empty key or a refused request, and
 * ABSENT when too many requests of the plugin are pending. */
BASALT_PROXY_IMPORT(cookie_request)
int32_t basalt_proxy_cookie_request(int32_t player, const char *key, int32_t klen, int32_t token);

/* player_transfer: tells the client to reconnect to host:port (the transfer packet; port 1..65535, 25565 is the
 * usual one). OK, STALE, INVALID for an empty host, a port out of range or a refused request. */
BASALT_PROXY_IMPORT(player_transfer)
int32_t basalt_proxy_player_transfer(int32_t player, const char *host, int32_t hlen, int32_t port);

/* ---- Players ---------------------------------------------------------------------------------------------------- */

/* players_by_uuid: the handle of the connected player with that UUID text (dashes as the proxy prints it), 0 when none. */
BASALT_PROXY_IMPORT(players_by_uuid) int32_t basalt_proxy_players_by_uuid(const char *uuid, int32_t ulen);

/* players_count: how many players are connected to the proxy, or to the backend `server` when it is not empty (0 for a
 * name that is not a backend). Never fails. */
BASALT_PROXY_IMPORT(players_count) int32_t basalt_proxy_players_count(const char *server, int32_t slen);

/* players_broadcast: sends one chat line to every connected player. `text` is MiniMessage or '@' lang key and `args` the
 * packed arguments list of basalt_abi.h. OK, INVALID for a text that cannot be resolved. */
BASALT_PROXY_IMPORT(players_broadcast)
int32_t basalt_proxy_players_broadcast(const char *text, int32_t tlen, const char *args, int32_t alen);

/* player_address: writes the client's IP text and returns the length the full answer needs, written only when it
 * fits. STALE for a player who left, ABSENT when the proxy has no address for them. */
BASALT_PROXY_IMPORT(player_address) int32_t basalt_proxy_player_address(int32_t player, char *out, int32_t cap);

/* player_port: the client's source port, STALE for a player who left, ABSENT when unknown. */
BASALT_PROXY_IMPORT(player_port) int32_t basalt_proxy_player_port(int32_t player);

/* player_settings: writes the client information as a map (locale string, view_distance int, chat_mode "full",
 * "system" or "hidden", chat_colors bool, skin_parts int, main_hand "left" or "right", text_filtering bool,
 * server_listing bool, particles "all", "decreased" or "minimal"), the needed-length convention. ABSENT until the
 * client has sent it, STALE for a player who left. */
BASALT_PROXY_IMPORT(player_settings)
int32_t basalt_proxy_player_settings(int32_t player, char *out, int32_t cap);

/* player_data_get_f64: the float stored under `key` in the plugin data of the player (capability players_data), or
 * `def` when the key is missing, holds another type, the capability is missing or the player left. An integer stored
 * by basalt_data_set_i64 reads back as that number. */
BASALT_PROXY_IMPORT(player_data_get_f64)
double basalt_proxy_player_data_get_f64(int32_t player, const char *key, int32_t klen, double def);

/* player_data_set_f64: stores a float under `key`. OK, STALE, INVALID for a non finite number or a bad key, DENIED
 * without the capability players_data. The same key space and limits as basalt_data_set_i64. */
BASALT_PROXY_IMPORT(player_data_set_f64)
int32_t basalt_proxy_player_data_set_f64(int32_t player, const char *key, int32_t klen, double value);

#ifdef __cplusplus
}
#endif
#endif
