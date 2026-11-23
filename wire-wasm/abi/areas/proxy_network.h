/* Proxy area P4: plugin messaging channels (including the BungeeCord channel the proxy answers itself), raw packet
 * listeners and senders, and antibot control. Module "basalt_proxy". Everything here follows basalt_abi.h and
 * basalt_proxy_abi.h; values are the encoding of basalt_values.h and bodies are bytes.
 *
 * Capabilities declared in plugin.toml: "network" gates the channel imports and the antibot imports, "packets" gates
 * every packet import that listens, sends, receives or injects (packet_resolve and packet_names read facts of the
 * protocol and need nothing). Each one also needs the operator to grant the same name to the plugin under
 * [plugins.allow] in proxy.toml, as "commands" does. An import without its capability or its grant answers DENIED.
 *
 * Threads. Channel messages and antibot events are queued and delivered on the single proxy plugin thread like every
 * other event, so their callbacks never nest and use the usual call budget. Packet listeners are different: they run
 * synchronously on the thread that carries the packet (a client session owner), because the proxy waits for their
 * answer before relaying. A listener first waits at most the packet timeout (proxy.toml plugins.packet-timeout-ms, 25
 * ms) for the plugin thread to be free and then runs with that same time budget. A listener that did not get to run
 * leaves the packet untouched, or cancels it when it was registered with fail "closed". A listener that traps or runs
 * out of its budget counts as a failure of the plugin like any callback and the packet goes on untouched. A packet
 * that is sent while a plugin callback runs, on the same thread, is not shown to listeners. A packet over the size
 * limit (proxy.toml plugins.packet-max-kib, 256 KiB) goes past listeners untouched.
 *
 * ---- Messaging channels ----------------------------------------------------------------------------------------
 *
 * A channel is "namespace:name", 1 to 128 printable ASCII characters. A plugin registers at most 64. "BungeeCord" and
 * "bungeecord:main" cannot be registered: the proxy answers them from backends itself (Connect, ConnectOther, IP,
 * IPOther, PlayerCount, PlayerList, GetServers, GetServer, Message, MessageRaw, Forward, ForwardToPlayer, UUID,
 * UUIDOther, ServerIP, KickPlayer, KickPlayerRaw) and a plugin never sees those messages. A registered channel is
 * announced to backends (minecraft:register) and messages on it are offered to the plugins that registered it.
 */
#ifndef BASALT_AREA_PROXY_NETWORK_H
#define BASALT_AREA_PROXY_NETWORK_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* network_channel_register: wants messages on `name` and announces it to backends. OK (registering twice is OK),
 * INVALID for a malformed name, one of the BungeeCord channels, or a plugin that already has 64, DENIED without the
 * network capability. */
BASALT_PROXY_IMPORT(network_channel_register)
int32_t basalt_proxy_network_channel_register(const char *name, int32_t nlen);

/* network_channel_unregister: the plugin stops wanting `name`; the channel stays announced while another plugin
 * still wants it. OK, ABSENT when this plugin had not registered it, DENIED without the network capability. */
BASALT_PROXY_IMPORT(network_channel_unregister)
int32_t basalt_proxy_network_channel_unregister(const char *name, int32_t nlen);

/* network_message_on: registers `token` to run basalt_callback(token, ctx) for every message the proxy relays on a
 * channel this plugin registered. `filter` selects one channel (compared exactly) and an empty filter (flen 0) takes
 * all of the plugin's channels. The callback is observed only: the message was already relayed. ctx answers
 * network_message_get and basalt_event_player (the player the message belongs to). OK, INVALID for a malformed filter
 * or a token registered 64 times, DENIED without the network capability. */
BASALT_PROXY_IMPORT(network_message_on)
int32_t basalt_proxy_network_message_on(const char *filter, int32_t flen, int32_t token);

/* network_message_get: inside a network_message_on callback writes a map: player_name (string), player_uuid (string),
 * channel (string), data (bytes, the whole payload), source ("backend" when a backend sent it toward the client,
 * "client" when the client sent it toward the backend), server (string, the backend of the player). Needed-length
 * convention. STALE once the callback returned, INVALID for a ctx that is not a message. */
BASALT_PROXY_IMPORT(network_message_get)
int32_t basalt_proxy_network_message_get(int32_t ctx, char *out, int32_t cap);

/* network_message_send: sends `data` (up to 32768 bytes, either way) on `channel` for the player handle. toward_client
 * 0 sends it to the player's current backend as if the proxy had written it, 1 to the player's client. OK once handed
 * over, ABSENT when the proxy could not hand it over (it is stopping or a connection queue is full; a player who is
 * not in play drops it quietly and the call still answers OK), STALE for a handle of a player who left, INVALID for a
 * bad channel, a payload over the limit, a toward_client other than 0 or 1, or a BungeeCord channel, DENIED without
 * the network capability. The data is not interpreted. */
BASALT_PROXY_IMPORT(network_message_send)
int32_t basalt_proxy_network_message_send(int32_t player, const char *channel, int32_t clen, const char *data,
                                          int32_t dlen, int32_t toward_client);

/* ---- Raw packets ----------------------------------------------------------------------------------------------- */

/* Packet names are the ones plugins always used: "clientbound.set_title_text", "serverbound.play.chat",
 * "clientbound.configuration.0x07" (a bare hexadecimal id is accepted for any packet). A listener name may also be a
 * pattern: "*", "serverbound.*", "clientbound.play.*". The phase names are "handshake", "status", "login",
 * "configuration" and "play". Bodies never include the packet id. */

/* packet_resolve: writes a map for one packet name (never a pattern): name (the canonical spelling), serverbound
 * (bool), phase (string), id (int). Needed-length convention. ABSENT when this proxy has no such packet. */
BASALT_PROXY_IMPORT(packet_resolve)
int32_t basalt_proxy_packet_resolve(const char *name, int32_t nlen, char *out, int32_t cap);

/* packet_names: writes a list of every packet name the proxy knows, strings in canonical spelling. Needed-length
 * convention. */
BASALT_PROXY_IMPORT(packet_names) int32_t basalt_proxy_packet_names(char *out, int32_t cap);

/* packet_on: registers a decider listener, `token` runs basalt_callback(token, ctx) for every packet matching `name`
 * (a name or pattern) before it is relayed, in priority order across plugins. `opts` is an optional value (olen 0 for
 * none) with the keys priority (string "first", "early", "normal", "late", "last", or int 0..4 with 0 running
 * first, default normal) and fail ("open" or "closed", default "open"); unknown keys are ignored. Returns a positive
 * listener handle. INVALID for an unknown name, a malformed opts or a wrong value, DENIED without the packets
 * capability, ABSENT when this proxy does not relay packets to plugins. */
BASALT_PROXY_IMPORT(packet_on)
int32_t basalt_proxy_packet_on(const char *name, int32_t nlen, const char *opts, int32_t olen, int32_t token);

/* packet_watch: the same as packet_on for an observer: it sees the final packet, after deciders, and its
 * packet_cancel and packet_set_body answer INVALID. The handle space is shared with packet_on. */
BASALT_PROXY_IMPORT(packet_watch)
int32_t basalt_proxy_packet_watch(const char *name, int32_t nlen, const char *opts, int32_t olen, int32_t token);

/* packet_remove: stops a listener. OK, ABSENT for a handle that was already removed. After it returns the token is
 * never called again for packets that arrive later; a delivery in flight on another thread may still finish. */
BASALT_PROXY_IMPORT(packet_remove) int32_t basalt_proxy_packet_remove(int32_t listener);

/* packet_get: inside a packet callback writes a map describing the packet: name, serverbound (bool), phase, id (int),
 * connection (int, the proxy's id of the connection), protocol (int, the client's), backend_protocol (int, the
 * backend's, 0 when unknown), translated (bool, the client is older than the proxy speaks natively so the bytes have
 * the proxy's shape), size (int, body bytes), backend (string, the backend carrying the packet, empty before one),
 * player_name, player_uuid (strings, empty before login), editable (bool, false for packets the proxy answers itself
 * and for login packets), observer (bool, the listener was registered with packet_watch), cancelled (bool, a decider
 * cancelled it; only observers see true). Needed-length convention. STALE once the callback returned, INVALID for a
 * ctx that is not a packet. basalt_event_player(ctx) answers the player handle (0 before login). */
BASALT_PROXY_IMPORT(packet_get) int32_t basalt_proxy_packet_get(int32_t ctx, char *out, int32_t cap);

/* packet_body: writes the body of the packet (bytes, the replaced body once packet_set_body ran) and returns the
 * length the full answer needs, written only when it fits in `cap`. Raw bytes, not a value. STALE, INVALID as
 * packet_get. */
BASALT_PROXY_IMPORT(packet_body) int32_t basalt_proxy_packet_body(int32_t ctx, char *out, int32_t cap);

/* packet_set_body: replaces the body that is relayed with (body, len), up to the size limit. OK, STALE, INVALID for a
 * ctx that is not a packet, an observer, a packet that is not editable, or a body over the limit. */
BASALT_PROXY_IMPORT(packet_set_body) int32_t basalt_proxy_packet_set_body(int32_t ctx, const char *body, int32_t len);

/* packet_cancel: drops the packet instead of relaying it. Later deciders are not called for a cancelled packet, only
 * observers, which see cancelled true. OK, STALE, INVALID as packet_set_body. */
BASALT_PROXY_IMPORT(packet_cancel) int32_t basalt_proxy_packet_cancel(int32_t ctx);

/* packet_inject: sends another packet on the connection of ctx, in the same phase: a clientbound name goes to the
 * client, a serverbound name is handled as if the client had sent it. `intercept` 1 lets listeners see it. At most 64
 * packets per callback. OK once handed over, ABSENT when the connection cannot take it, STALE, INVALID for a name
 * that is not a packet, a phase other than the connection's, a body over the limit or more than 64 injected. */
BASALT_PROXY_IMPORT(packet_inject)
int32_t basalt_proxy_packet_inject(int32_t ctx, const char *name, int32_t nlen, const char *body, int32_t len,
                                   int32_t intercept);

/* packet_send: sends a clientbound packet to the player's client (handle from basalt_player_by_name and the like). OK
 * once handed over, ABSENT when the proxy could not hand it over (it is stopping or a connection queue is full; a
 * connection that cannot take it drops it quietly and the call still answers OK), STALE for a handle of a player who
 * left, INVALID for a name that is not a clientbound packet or a body over the limit, DENIED without the packets
 * capability. */
BASALT_PROXY_IMPORT(packet_send)
int32_t basalt_proxy_packet_send(int32_t player, const char *name, int32_t nlen, const char *body, int32_t len);

/* packet_receive: makes the proxy handle a serverbound packet as if the player's client had sent it. Results as
 * packet_send, INVALID for a name that is not a serverbound packet. */
BASALT_PROXY_IMPORT(packet_receive)
int32_t basalt_proxy_packet_receive(int32_t player, const char *name, int32_t nlen, const char *body, int32_t len);

/* ---- Antibot --------------------------------------------------------------------------------------------------- */

/* The antibot calls take player names and addresses, not handles: a bot is rarely a connected player. They need the
 * network capability. */

/* antibot_verify / antibot_unverify: marks or unmarks a player name as verified, so it passes the antibot checks (the
 * same list the limbo fills). OK, INVALID for a name that is not a valid login name, DENIED without the capability. */
BASALT_PROXY_IMPORT(antibot_verify) int32_t basalt_proxy_antibot_verify(const char *name, int32_t nlen);
BASALT_PROXY_IMPORT(antibot_unverify) int32_t basalt_proxy_antibot_unverify(const char *name, int32_t nlen);

/* antibot_is_verified: 1 when the name is on the verified list, 0 when not. INVALID, DENIED as above. */
BASALT_PROXY_IMPORT(antibot_is_verified) int32_t basalt_proxy_antibot_is_verified(const char *name, int32_t nlen);

/* antibot_ban / antibot_unban: bans or lifts the ban of an IP address text (v4 or v6, no port). OK when the origin of
 * the address was banned or cleared, ABSENT when an unban found nothing tracked, INVALID for text that is not an
 * address, DENIED without the capability. */
BASALT_PROXY_IMPORT(antibot_ban) int32_t basalt_proxy_antibot_ban(const char *address, int32_t alen);
BASALT_PROXY_IMPORT(antibot_unban) int32_t basalt_proxy_antibot_unban(const char *address, int32_t alen);

/* antibot_under_attack: 1 while the antibot is in attack mode, else 0. DENIED without the capability. */
BASALT_PROXY_IMPORT(antibot_under_attack) int32_t basalt_proxy_antibot_under_attack(void);

/* antibot_on: registers `token` for every antibot event (a verification result, a ban, a mitigation step). Observed
 * only, on the plugin thread; ctx answers antibot_event_get. OK, INVALID for a token registered 64 times, DENIED
 * without the capability. */
BASALT_PROXY_IMPORT(antibot_on) int32_t basalt_proxy_antibot_on(int32_t token);

/* antibot_event_get: inside an antibot_on callback writes a map: kind (string), player_name (string), action (string),
 * reason (string), skipped (int, connections the event stands for, 0 when it is about one), origin (string, the
 * address or network the event came from). Needed-length convention. STALE, INVALID as network_message_get. */
BASALT_PROXY_IMPORT(antibot_event_get) int32_t basalt_proxy_antibot_event_get(int32_t ctx, char *out, int32_t cap);

#ifdef __cplusplus
}
#endif
#endif
