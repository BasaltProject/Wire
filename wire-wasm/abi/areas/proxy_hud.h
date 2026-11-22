/* Proxy area P2: what a plugin shows a player outside the chat, from the proxy itself: boss bars, a sidebar, the tab list
 * header and footer, fake tab list rows, the pause menu links, the chat completion list and clearing the title.
 * Module "basalt_proxy". Everything follows basalt_abi.h and basalt_proxy_abi.h; structured arguments are the values of
 * basalt_values.h. Titles and the action bar stay in basalt_proxy_player_title and basalt_player_action_bar.
 *
 * Shared rules of every import:
 *  - `player` is a proxy player handle. A handle of a player who left answers BASALT_ERR_STALE.
 *  - Text is MiniMessage, or a lang key of the plugin when it starts with '@'; it is cut at 4096 bytes. Where a call
 *    has an `args` map, its entries are the {name} fields of every text of the call (the packed list of basalt_abi.h
 *    for the calls that take one).
 *  - OK means the proxy handed the change to the player's connection. BASALT_ERR_ABSENT means it could not: the
 *    player is not in the play state yet, is leaving, or is a client the proxy does not send HUD packets to (a
 *    legacy or Bedrock client). The effect reaches the client in connection order, like the title.
 *  - A boss bar or a tab row is named by an id chosen by the plugin, 1 to 64 bytes, per player; the id space is shared
 *    by all plugins (the same id from two plugins is one bar). The player has one sidebar, one tab list and one list of
 *    links; the last plugin to set them wins. Nothing outlives the player's connection.
 *  - Every import runs on the single proxy plugin thread, inside a callback.
 *
 * Boss bar colors: pink blue red green yellow purple white. Overlays: progress notched_6 notched_10 notched_12
 * notched_20. A color or overlay that is not in the list falls back to purple and progress (parity with the old Lua
 * API), never an error. Progress is clamped to 0..1.
 */
#ifndef BASALT_AREA_PROXY_HUD_H
#define BASALT_AREA_PROXY_HUD_H
#include "../basalt_abi.h"
#include "../basalt_values.h"
#ifdef __cplusplus
extern "C" {
#endif

/* hud_clear_title: takes the title, subtitle and action bar off the player's screen; `reset` non zero also puts the
 * fade times back to their defaults. OK, STALE, ABSENT as above. */
BASALT_PROXY_IMPORT(hud_clear_title) int32_t basalt_proxy_hud_clear_title(int32_t player, int32_t reset);

/* hud_tab_list: sets the header and footer of the player's tab list. `spec` is a map with the optional string keys
 * header and footer and the optional map args; a missing or null part keeps what the player has, an empty string
 * clears it. OK, STALE, ABSENT, INVALID for a spec that is not a map or a part that is not a string. */
BASALT_PROXY_IMPORT(hud_tab_list) int32_t basalt_proxy_hud_tab_list(int32_t player, const char *spec, int32_t slen);

/* hud_bossbar_show: shows (or replaces) the boss bar `id`. `spec` is a map with the string text (required), the number
 * progress (default 1.0), the strings color (default "purple") and overlay (default "progress"), and the optional map
 * args. OK, STALE, ABSENT, INVALID for an id that is empty or over 64 bytes, a spec without text or with a key of the
 * wrong type, or a progress that is not a number. */
BASALT_PROXY_IMPORT(hud_bossbar_show)
int32_t basalt_proxy_hud_bossbar_show(int32_t player, const char *id, int32_t ilen, const char *spec, int32_t slen);

/* hud_bossbar_hide: removes the boss bar `id`. OK, STALE, ABSENT, INVALID for a bad id. */
BASALT_PROXY_IMPORT(hud_bossbar_hide)
int32_t basalt_proxy_hud_bossbar_hide(int32_t player, const char *id, int32_t ilen);

/* hud_bossbar_progress: sets the fill of the boss bar `id` (0 empty, 1 full, clamped). Same results as hud_bossbar_show;
 * INVALID also for NaN. */
BASALT_PROXY_IMPORT(hud_bossbar_progress)
int32_t basalt_proxy_hud_bossbar_progress(int32_t player, const char *id, int32_t ilen, double progress);

/* hud_bossbar_title: sets the text of the boss bar `id`; `args` is the packed list of basalt_abi.h. Same results. */
BASALT_PROXY_IMPORT(hud_bossbar_title)
int32_t basalt_proxy_hud_bossbar_title(int32_t player, const char *id, int32_t ilen, const char *text, int32_t tlen,
                                       const char *args, int32_t alen);

/* hud_bossbar_style: sets the color and the overlay of the boss bar `id` by name (see the lists above). Same
 * results. */
BASALT_PROXY_IMPORT(hud_bossbar_style)
int32_t basalt_proxy_hud_bossbar_style(int32_t player, const char *id, int32_t ilen, const char *color, int32_t clen,
                                       const char *overlay, int32_t olen);

/* hud_sidebar: shows the plugin's sidebar. `spec` is a map with the string title (required), the list of strings
 * lines, top to bottom (at most 15, the rest is ignored), and the optional map args. OK, STALE, ABSENT, INVALID for a
 * spec without title or with a line that is not a string. */
BASALT_PROXY_IMPORT(hud_sidebar) int32_t basalt_proxy_hud_sidebar(int32_t player, const char *spec, int32_t slen);

/* hud_sidebar_clear: takes the sidebar down. OK, STALE, ABSENT. */
BASALT_PROXY_IMPORT(hud_sidebar_clear) int32_t basalt_proxy_hud_sidebar_clear(int32_t player);

/* hud_tab_entry: adds or updates the fake row `id` in the player's own tab list. `spec` is a map with the strings name
 * (the profile name of the row) and text (the display text), both required, the integer latency in milliseconds (0 to
 * 10000, clamped, default 0), and the optional map args. OK, STALE, ABSENT, INVALID for a bad id or spec. */
BASALT_PROXY_IMPORT(hud_tab_entry)
int32_t basalt_proxy_hud_tab_entry(int32_t player, const char *id, int32_t ilen, const char *spec, int32_t slen);

/* hud_tab_entry_remove: removes the fake row `id`. OK, STALE, ABSENT, INVALID for a bad id. */
BASALT_PROXY_IMPORT(hud_tab_entry_remove)
int32_t basalt_proxy_hud_tab_entry_remove(int32_t player, const char *id, int32_t ilen);

/* hud_server_links: replaces the links of the client's pause menu. `links` is a list whose items are pairs, either a
 * map with the strings label and url or a list of two strings [label, url]. A label equal to one of report_bug,
 * community_guidelines, support, status, feedback, community, website, forums, news or announcements uses that
 * known link type; any other label is MiniMessage. An empty list clears the links. OK, STALE, ABSENT, INVALID for a
 * value that is not a list of pairs. */
BASALT_PROXY_IMPORT(hud_server_links)
int32_t basalt_proxy_hud_server_links(int32_t player, const char *links, int32_t llen);

/* hud_chat_completions: changes the words the client offers when completing chat. `action` is "add", "remove" or
 * "set" (replace the whole list); `words` is a list of strings. OK, STALE, ABSENT, INVALID for another action or a
 * value that is not a list of strings. */
BASALT_PROXY_IMPORT(hud_chat_completions)
int32_t basalt_proxy_hud_chat_completions(int32_t player, const char *action, int32_t alen, const char *words,
                                          int32_t wlen);

#ifdef __cplusplus
}
#endif
#endif
