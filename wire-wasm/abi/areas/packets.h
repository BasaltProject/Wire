#ifndef BASALT_AREA_PACKETS_H
#define BASALT_AREA_PACKETS_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Plugin channels and raw packets (server profile only).
 *
 * Capabilities. `channel_*` needs `network = ["<channel>", ...]` in the [capabilities] table of plugin.toml and only
 * works for a channel named there; `packet_*` needs `packets = true`. Both also need the operator grant under
 * [plugins.allow] in config/server.toml. Without any of them the import returns BASALT_ERR_DENIED.
 *
 * Bodies are bytes (pointer and length). A packet body is the packet without its id, in the server's own protocol
 * shape (an older client's packets are translated first; the info value says so). The largest body is 1 MiB, a longer
 * one is BASALT_ERR_INVALID.
 *
 * Names. A packet is named as the server names it in `/plugins packets`: `clientbound.set_title_text`,
 * `serverbound.chat_message`, `serverbound.play.custom_payload` (the phase is added when the name alone is ambiguous),
 * a hex id such as `clientbound.configuration.0x07`. A listener may also use a wildcard: `serverbound.*`, `clientbound.play.*` or `*`.
 * Phases are handshake, status, login, configuration and play. packet_names lists the real names and packet_lookup
 * says what one name means, so nothing here is a hand-written table.
 *
 * Threads. A packet or channel listener runs on the thread that carries the packet, usually the network thread of that
 * connection, holding the plugin's turn: it must not block, wait or do slow work. A listener has a time budget of 5
 * ms per call. When a call goes over, its answers (cancel, set_body) are discarded and one strike counts against the
 * listener; the third consecutive strike removes the listener and logs it. The server watchdog still cuts a plugin
 * that never returns. Everything else in this area is called from the plugin's own callbacks, never from outside one.
 *
 * Callback context. A listener is called as basalt_callback(token, ctx). `ctx` is leased: it works only inside that
 * callback and is STALE once it returns. The packet_ctx_* imports below answer for both packet and channel contexts
 * unless said otherwise. */

/* channel_listen: calls `token` for every message a client sends on plugin channel `name` (for example
 * "example:chan"); ctx answers packet_ctx_info (kind "channel"), packet_ctx_body and packet_ctx_player. The packet_ctx
 * setters and relays on a channel ctx return BASALT_ERR_INVALID. OK; DENIED without the network capability or for a
 * channel the manifest does not list; INVALID for an empty name. The listener stays until the plugin is disabled. */
BASALT_IMPORT(channel_listen) int32_t basalt_channel_listen(const char *name, int32_t name_len, int32_t token);

/* channel_send: sends `body` to `player` on plugin channel `name`. 1 when it was sent, 0 when the player cannot
 * receive it (not a Java client, or the client did not register the channel). STALE for a player that left, INVALID
 * for a bad handle or name, DENIED as above. Owner thread: the plugin's. */
BASALT_IMPORT(channel_send) int32_t basalt_channel_send(int32_t player, const char *name, int32_t name_len,
                                                        const char *body, int32_t body_len);

/* packet_listen: calls `token` for every packet that `pattern` matches. `options` is one value (basalt_values.h):
 * null for the defaults, or a map with the optional keys "priority" (string "first", "early", "normal", "late" or
 * "last", default "normal"; first runs first) and "observe" (bool, default false). An observer only watches: its
 * packet_ctx_cancel and packet_ctx_set_body return DENIED and what it did is dropped. A plain listener may cancel the
 * packet or replace its body; once a listener cancelled a packet, later plain listeners are skipped and observers
 * still see it; with several replacements the last one wins. The packet ctx answers every packet_ctx_* import.
 * Returns the listener handle (a positive i32, STALE after packet_unlisten or when the plugin is disabled); INVALID
 * for an unknown name or bad options; DENIED without the packets capability. With no listener on a packet the server
 * pays one atomic read for it. */
BASALT_IMPORT(packet_listen) int32_t basalt_packet_listen(const char *pattern, int32_t pattern_len,
                                                          const char *options, int32_t options_len, int32_t token);

/* packet_unlisten: removes a listener made by packet_listen. OK; STALE for a handle that was already removed;
 * INVALID for a malformed handle. Allowed from inside the listener itself. */
BASALT_IMPORT(packet_unlisten) int32_t basalt_packet_unlisten(int32_t listener);

/* packet_send: sends the clientbound packet `name` with `body` to `player`. `intercept` non-zero runs the listeners on
 * it (including this plugin's), zero sends it past them. 1 when written, 0 when the player has no Java connection
 * (Bedrock) or it is closing; INVALID for an unknown or serverbound name or a bad body; STALE for a player that left;
 * DENIED without the packets capability. The body is in the wire format of the player's protocol. */
BASALT_IMPORT(packet_send) int32_t basalt_packet_send(int32_t player, const char *name, int32_t name_len,
                                                      const char *body, int32_t body_len, int32_t intercept);

/* packet_receive: handles the serverbound packet `name` as if the player's client had sent it. Same arguments and
 * results as packet_send, with a serverbound name. */
BASALT_IMPORT(packet_receive) int32_t basalt_packet_receive(int32_t player, const char *name, int32_t name_len,
                                                            const char *body, int32_t body_len, int32_t intercept);

/* packet_lookup: what `name` means, as one map value written to `out` with the usual needed-length result: "name"
 * (string, as the server spells it), "phase" (string, null for a wildcard over all phases), "serverbound" (bool, null
 * when a wildcard covers both), "id" (int, null for a wildcard) and "wildcard" (bool). INVALID for a name this server
 * does not have, DENIED without the packets capability. */
BASALT_IMPORT(packet_lookup) int32_t basalt_packet_lookup(const char *name, int32_t name_len, char *out, int32_t cap);

/* packet_names: the packet names that start with `prefix` (empty for all), as a value list of strings in the order
 * of `/plugins packets`, written with the needed-length convention. DENIED without the packets capability. */
BASALT_IMPORT(packet_names) int32_t basalt_packet_names(const char *prefix, int32_t prefix_len, char *out,
                                                        int32_t cap);

/* packet_player_actions: the names accepted by packet_player_receive, as a value list of strings, generated from the
 * server's own player operation table. DENIED without the packets capability. */
BASALT_IMPORT(packet_player_actions) int32_t basalt_packet_player_actions(char *out, int32_t cap);

/* packet_player_receive: does what the client packet named `action` would do for `player`: use_item, use_item_on,
 * container_click, interact, attack, custom_payload, chat_command, move, pick_block, pick_entity, place_recipe,
 * creative_slot, sign_update, rename_item, beacon, select_trade, container_button, container_close, held_slot, chat,
 * edit_book (packet_player_actions lists them). `args` is one value: a list of the arguments in the order the server
 * documents for that action (numbers, booleans, strings, null, a list of strings for book pages). 1 when the server
 * accepted it, 0 when it did not (no Java connection, or the packet was refused); INVALID for an unknown action or an
 * argument that is not a scalar or a list of strings; STALE for a player that left; DENIED without the packets
 * capability. */
BASALT_IMPORT(packet_player_receive) int32_t basalt_packet_player_receive(int32_t player, const char *action,
                                                                         int32_t action_len, const char *args,
                                                                         int32_t args_len);

/* packet_ctx_info: writes one map value describing what the listener is looking at. Packet: "kind" ("packet"),
 * "name", "phase", "id", "serverbound", "connection" (int, stable for the connection), "protocol" (the version the
 * client announced, 0 before the handshake), "translated" (bool, the bytes are in the server's shape for an older
 * client), "size", "player_name", "player_uuid" (empty before login), "cancelled" (what earlier listeners decided) and
 * "observer". Channel: "kind" ("channel"), "channel", "size", "player_name", "player_uuid". Needed-length result;
 * STALE once the callback returned, INVALID for another kind of ctx. */
BASALT_IMPORT(packet_ctx_info) int32_t basalt_packet_ctx_info(int32_t ctx, char *out, int32_t cap);

/* packet_ctx_body: writes the body as it arrived (not what earlier listeners replaced it with), needed-length result,
 * STALE or INVALID as packet_ctx_info. */
BASALT_IMPORT(packet_ctx_body) int32_t basalt_packet_ctx_body(int32_t ctx, char *out, int32_t cap);

/* packet_ctx_player: the handle of the player the packet or message belongs to, 0 when there is none yet (before
 * login) or the player is gone. STALE or INVALID as packet_ctx_info. */
BASALT_IMPORT(packet_ctx_player) int32_t basalt_packet_ctx_player(int32_t ctx);

/* packet_ctx_cancel: cancels the packet, so it is neither sent nor handled. OK; DENIED for an observer; INVALID for a
 * channel ctx; STALE once the callback returned. */
BASALT_IMPORT(packet_ctx_cancel) int32_t basalt_packet_ctx_cancel(int32_t ctx);

/* packet_ctx_set_body: replaces the body that continues on its way. OK; INVALID for a body over 1 MiB, bad pointers or
 * a channel ctx; DENIED for an observer; STALE once the callback returned. */
BASALT_IMPORT(packet_ctx_set_body) int32_t basalt_packet_ctx_set_body(int32_t ctx, const char *body,
                                                                      int32_t body_len);

/* packet_ctx_send: sends another clientbound packet on the connection this packet travels on (pkt:send). Arguments and
 * results as packet_send without the player; INVALID for a channel ctx, 0 when the connection is gone. */
BASALT_IMPORT(packet_ctx_send) int32_t basalt_packet_ctx_send(int32_t ctx, const char *name, int32_t name_len,
                                                              const char *body, int32_t body_len, int32_t intercept);

/* packet_ctx_receive: hands a serverbound packet to the server as if the client had sent it on this connection
 * (pkt:receive). Arguments and results as packet_ctx_send with a serverbound name. */
BASALT_IMPORT(packet_ctx_receive) int32_t basalt_packet_ctx_receive(int32_t ctx, const char *name, int32_t name_len,
                                                                   const char *body, int32_t body_len,
                                                                   int32_t intercept);

#ifdef __cplusplus
}
#endif
#endif
