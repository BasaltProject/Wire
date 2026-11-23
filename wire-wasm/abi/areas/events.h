/* Area events: generic event access by value, post-decision listeners, plugin-to-plugin events, and the player actions
 * the first version lacked (health, game mode, teleport, titles, kick, effects, forms, dialogs, windows, any named
 * player property or operation). Server profile only; the proxy declares its own copy in proxy_events.h.
 *
 * An event ctx is the handle basalt_callback receives for a decision listener (basalt_event_on) or a listener of this
 * area (basalt_event_on_ex). It is valid until the callback returns: afterwards every event_* call answers STALE.
 * Callbacks run on the plugin thread and must not block. A listener that would start inside another callback of the
 * same plugin is skipped, so an event caused by one of the plugin's own calls never reaches its own listeners (other
 * plugins' listeners still run). An event the server raises on a thread other than the plugin thread is skipped for
 * plugins too, unless the server hands it over first. Fields are the ones the server declares for that event, by
 * name (basalt_event_describe lists them); a field is null, bool, i64, f64 or string as the server fires it.
 *
 * Player handles are the ones of basalt_player_by_name; they are STALE once the player left. Every player import
 * runs on the plugin thread and takes effect on the player at once.
 */
#ifndef BASALT_AREA_EVENTS_H
#define BASALT_AREA_EVENTS_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Flags of basalt_event_on_ex. */
#define BASALT_EVENT_POST 1      /* listen after every decision listener ran: the event can no longer change */
#define BASALT_EVENT_CANCELLED 2 /* also receive the event after another listener cancelled it */

/* event_on_ex: listens to `name` ("player.chat", "block.break", or a plugin event "plugin:topic" which has a colon)
 * and calls basalt_callback(token, ctx). `priority` is BASALT_PRIORITY_*, `flags` the BASALT_EVENT_* bits.
 * `filters` is one value, a map from field name to the value the field must equal (same kind, an i64 never equals an
 * f64) for the listener to run; ptr 0 and len 0 mean no filter. A post listener (BASALT_EVENT_POST) runs after the
 * decision, on a copy: event_set answers INVALID and event_cancel changes nothing. BASALT_EVENT_CANCELLED on an event
 * that cannot be cancelled is INVALID. Plugin events accept no filters and no flags but priority.
 * OK; INVALID for an unknown event, a bad priority, flags, a filter on a field the event lacks or a malformed map. */
BASALT_IMPORT(event_on_ex)
int32_t basalt_event_on_ex(const char *name, int32_t nlen, int32_t priority, int32_t flags, const char *filters,
                           int32_t flen, int32_t token);

/* event_name: the event's name into `out`; the length it needs. STALE outside the callback, INVALID for a ctx that is
 * not an event. */
BASALT_IMPORT(event_name) int32_t basalt_event_name(int32_t ctx, char *out, int32_t cap);

/* event_get: one field as a value (the encoding of basalt_values.h) into `out`; the length it needs. An unset field
 * is the null value. The names "name" and "uuid" stand for player_name and player_uuid, and "online" on a player event
 * is the number of players online. ABSENT when the event has no such field; STALE, INVALID as event_name. */
BASALT_IMPORT(event_get) int32_t basalt_event_get(int32_t ctx, const char *field, int32_t flen, char *out, int32_t cap);

/* event_set: replaces one field with the value at (value, vlen). Only fields the event lists as mutable can be set.
 * A number keeps the kind the field has (an i64 sent to an f64 field is converted, an f64 sent to an i64 field must be
 * whole); a field the server left unset takes any scalar kind; null unsets. OK; INVALID for a field that is not
 * mutable or does not exist, a value that does not fit the field, a post listener or a plugin event; STALE. */
BASALT_IMPORT(event_set)
int32_t basalt_event_set(int32_t ctx, const char *field, int32_t flen, const char *value, int32_t vlen);

/* event_fields: every field of the event with its current value, as one map, into `out`. The needed length. */
BASALT_IMPORT(event_fields) int32_t basalt_event_fields(int32_t ctx, char *out, int32_t cap);

/* event_describe: what the server declares for event `name`, as the map {name: string, cancellable: bool,
 * custom: bool, fields: list of {name: string, mutable: bool}}, into `out`. The tables come from the server's own event
 * definitions. ABSENT for an unknown name. */
BASALT_IMPORT(event_describe)
int32_t basalt_event_describe(const char *name, int32_t nlen, char *out, int32_t cap);

/* event_names: the list of every event name the server declares, as a value, sorted. The needed length. */
BASALT_IMPORT(event_names) int32_t basalt_event_names(char *out, int32_t cap);

/* event_cancelled: 1 when the event is cancelled so far, 0 when not. STALE, INVALID as event_name. */
BASALT_IMPORT(event_cancelled) int32_t basalt_event_cancelled(int32_t ctx);

/* event_cancel_with: cancels the event and keeps a message for whoever caused it: `text` is a lang key when it starts
 * with '@', else MiniMessage, with arguments as in basalt_player_send. The message is always kept (event_cancel_message
 * reads it back) but the server shows it only for player.login (the disconnect reason), server.command (printed on
 * the console) and server.remote_command (the reply to the sender); every other event is cancelled silently. OK;
 * INVALID when the event cannot be cancelled or this is a post listener; ABSENT when the lang key does not exist. */
BASALT_IMPORT(event_cancel_with)
int32_t basalt_event_cancel_with(int32_t ctx, const char *text, int32_t tlen, const char *args, int32_t alen);

/* event_cancel_message: the message given to event_cancel_with, into `out`; the needed length, ABSENT when none. */
BASALT_IMPORT(event_cancel_message) int32_t basalt_event_cancel_message(int32_t ctx, char *out, int32_t cap);

/* event_is_post: 1 for a post listener, 0 for a decision listener. STALE, INVALID as event_name. */
BASALT_IMPORT(event_is_post) int32_t basalt_event_is_post(int32_t ctx);

/* event_emit: fires the plugin event `name` (it must contain a colon, "myplugin:level_up") with `value` as its
 * payload, one value of any shape. Every plugin that listens with event_on_ex runs, in priority order, on its own
 * thread turn; a listener of the emitting plugin does not run while the emitter is inside a callback (callbacks never
 * nest in one plugin). Returns how many listeners ran, 0 when nobody listens. INVALID for a name without a colon, a
 * malformed value or a chain of emits nested more than 8 deep. */
BASALT_IMPORT(event_emit) int32_t basalt_event_emit(const char *name, int32_t nlen, const char *value, int32_t vlen);

/* event_payload: the payload of a plugin event, into `out`; the needed length. INVALID for a server event. */
BASALT_IMPORT(event_payload) int32_t basalt_event_payload(int32_t ctx, char *out, int32_t cap);

/* player_by_uuid: the handle of the online player with that uuid text, 0 when none. */
BASALT_IMPORT(player_by_uuid) int32_t basalt_player_by_uuid(const char *uuid, int32_t len);

/* player_uuid: the uuid text into `out`; the needed length. STALE for a player who left. */
BASALT_IMPORT(player_uuid) int32_t basalt_player_uuid(int32_t player, char *out, int32_t cap);

/* player_get: one property by name as a value into `out`; the needed length. The names are the ones
 * basalt_player_describe lists plus health, food, gamemode, level, world, x, y, z, yaw, pitch, ping, sneaking,
 * flying, on_ground, held_item, address, permission_level and online. ABSENT when there is no such property;
 * STALE. */
BASALT_IMPORT(player_get)
int32_t basalt_player_get(int32_t player, const char *name, int32_t nlen, char *out, int32_t cap);

/* player_set: writes one property by name from the value at (value, vlen). Writable names are health, food, gamemode,
 * level and every property basalt_player_describe marks writable. OK; INVALID for a read only or unknown name or a
 * value of the wrong kind; STALE. */
BASALT_IMPORT(player_set)
int32_t basalt_player_set(int32_t player, const char *name, int32_t nlen, const char *value, int32_t vlen);

/* player_call: runs the named operation (basalt_player_describe lists them) with `args`, a list value (ptr 0 and len 0
 * are an empty list), and writes its result as a value into `out`; the needed length. The result is null for an
 * operation with nothing to answer. Operations that report success answer a bool. ABSENT for an unknown operation;
 * INVALID for a malformed argument list; STALE. */
BASALT_IMPORT(player_call)
int32_t basalt_player_call(int32_t player, const char *op, int32_t olen, const char *args, int32_t alen, char *out,
                           int32_t cap);

/* player_describe: the player property and operation tables the server defines, as {properties: list of
 * {name: string, kind: "bool"|"int"|"real"|"text"|"list", writable: bool}, operations: list of string}, into `out`.
 * The needed length. */
BASALT_IMPORT(player_describe) int32_t basalt_player_describe(char *out, int32_t cap);

/* player_health: writes the health in half hearts to *out (an f64). OK; STALE. */
BASALT_IMPORT(player_health) int32_t basalt_player_health(int32_t player, double *out);

/* player_set_health: sets the health. OK; INVALID for a negative or NaN value; STALE. */
BASALT_IMPORT(player_set_health) int32_t basalt_player_set_health(int32_t player, double health);

/* player_gamemode: "survival", "creative", "adventure" or "spectator" into `out`; the needed length. STALE. */
BASALT_IMPORT(player_gamemode) int32_t basalt_player_gamemode(int32_t player, char *out, int32_t cap);

/* player_set_gamemode: changes the game mode by name. OK; INVALID for an unknown name; STALE. */
BASALT_IMPORT(player_set_gamemode) int32_t basalt_player_set_gamemode(int32_t player, const char *mode, int32_t mlen);

/* player_teleport: moves the player to (x, y, z) facing yaw and pitch in degrees, in `world` (an empty name is the
 * world the player is in). OK; ABSENT when the move could not be done (unknown world, nothing to teleport); INVALID for
 * NaN or an infinite coordinate; STALE. */
BASALT_IMPORT(player_teleport)
int32_t basalt_player_teleport(int32_t player, const char *world, int32_t wlen, double x, double y, double z, float yaw,
                               float pitch);

/* player_title: shows a title and a subtitle (text rules and packed `args` of basalt_player_send, one block for both;
 * empty subtitle for none) with the timings in ticks. OK; INVALID for a negative timing; ABSENT for a missing lang key;
 * STALE. */
BASALT_IMPORT(player_title)
int32_t basalt_player_title(int32_t player, const char *title, int32_t tlen, const char *subtitle, int32_t slen,
                            int32_t fade_in, int32_t stay, int32_t fade_out, const char *args, int32_t alen);

/* player_kick: disconnects the player with the message (text rules of basalt_player_send). OK; ABSENT for a missing
 * lang key; STALE. The handle is STALE afterwards. */
BASALT_IMPORT(player_kick)
int32_t basalt_player_kick(int32_t player, const char *text, int32_t tlen, const char *args, int32_t alen);

/* player_effect: gives a potion effect by id ("minecraft:speed") for `ticks` (-1 is endless) at `amplifier` (0 is
 * level one); 0 ticks removes it. OK; ABSENT for an unknown effect; INVALID for ticks below -1 or a negative
 * amplifier; STALE. */
BASALT_IMPORT(player_effect)
int32_t basalt_player_effect(int32_t player, const char *effect, int32_t elen, int32_t ticks, int32_t amplifier);

/* player_open_inventory: opens a window of `kind` for the player. `spec` is one value (ptr 0 and len 0 for none).
 * Kinds: "merchant" ({title: string JSON text component, offers: string SNBT}), "trades" ({entity: i64 entity id of a
 * villager}), "sign" ({x, y, z: i64, front: bool}), "book" ({item: string holding the binary NBT bytes, not SNBT, of
 * the written book: a compound with id, count and components}). Opening the editor of a real sign first fires
 * player.sign_open (cause "plugin"), which a listener can cancel. The player's own chest windows are menus
 * (basalt_menu_open). OK; ABSENT when the server could not open it; INVALID for an unknown kind or spec; STALE. */
BASALT_IMPORT(player_open_inventory)
int32_t basalt_player_open_inventory(int32_t player, const char *kind, int32_t klen, const char *spec, int32_t slen);

/* player_close_window: closes the window the player has open. OK; STALE. */
BASALT_IMPORT(player_close_window) int32_t basalt_player_close_window(int32_t player);

/* form_open: shows a form to the player. The form is one map {title: string, body: string or list of string,
 * buttons: list of string (default ["OK"]), fields: list of maps {key, type: "text"|"toggle"|"slider"|"choice",
 * label, initial, min, max, step, options}}; every text is a lang key with '@' or MiniMessage. Returns a request
 * handle and later calls basalt_callback(token, ctx) once, where basalt_value_result answers the map {player: i64
 * handle, button: i64 index from 0, values: map from field key to string, bool (toggle) or f64 (slider)}; the callback
 * does not run when the player closes the form or leaves, or after basalt_request_cancel. INVALID for a malformed
 * form; STALE. */
BASALT_IMPORT(form_open)
int32_t basalt_form_open(int32_t player, const char *form, int32_t flen, int32_t token);

/* form_close: closes the form the player has open from this plugin and drops its completion. OK, ABSENT when none. */
BASALT_IMPORT(form_close) int32_t basalt_form_close(int32_t player);

/* dialog_show: shows a dialog to the player: `dialog` is a full dialog as SNBT (starts with '{') or the id of a
 * registry dialog. Returns a request handle and calls basalt_callback(token, ctx) once, on the first custom click
 * the player answers, where basalt_value_result answers {player: i64 handle, id: string, payload: string SNBT or
 * empty}; no answer means no callback (basalt_request_cancel drops it). ABSENT when the server could not show the
 * dialog; INVALID for malformed text; STALE. */
BASALT_IMPORT(dialog_show)
int32_t basalt_dialog_show(int32_t player, const char *dialog, int32_t dlen, int32_t token);

/* dialog_clear: closes the dialog the player has open. OK; STALE. */
BASALT_IMPORT(dialog_clear) int32_t basalt_dialog_clear(int32_t player);

#ifdef __cplusplus
}
#endif
#endif
