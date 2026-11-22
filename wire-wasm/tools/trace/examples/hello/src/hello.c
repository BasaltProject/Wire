#include "basalt_abi.h"

void *memcpy(void *dest, const void *src, unsigned long n) {
    char *d = dest;
    const char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

#define S(literal) literal, (int32_t)(sizeof(literal) - 1)

enum {
    kEnable = 1,
    kJoin,
    kLeave,
    kCoins,
    kTop,
    kPeriodic,
    kLater,
    kPlaceholder,
    kClick,
};

static char gBuf[256];
static char gArgs[256];
static int32_t gMenu;
static int32_t gPeriodic;
static int32_t gFirings;

static int32_t length(const char *text) {
    int32_t n = 0;
    while (text[n]) {
        ++n;
    }
    return n;
}

static int32_t decimal(int64_t value, char *out) {
    char tmp[24];
    int32_t n = 0;
    int32_t at = 0;
    uint64_t magnitude = value < 0 ? 0 - (uint64_t)value : (uint64_t)value;
    do {
        tmp[n++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (value < 0) {
        out[at++] = '-';
    }
    while (n) {
        out[at++] = tmp[--n];
    }
    return at;
}

static int32_t put32(char *out, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out[i] = (char)(value >> (8 * i));
    }
    return 4;
}

static int32_t packOne(const char *key, const char *value, int32_t vlen) {
    int32_t at = put32(gArgs, 1);
    int32_t klen = length(key);
    at += put32(gArgs + at, (uint32_t)klen);
    memcpy(gArgs + at, key, (unsigned long)klen);
    at += klen;
    at += put32(gArgs + at, (uint32_t)vlen);
    memcpy(gArgs + at, value, (unsigned long)vlen);
    return at + vlen;
}

static int32_t packTwo(const char *k1, const char *v1, int32_t l1, const char *k2, const char *v2, int32_t l2) {
    int32_t at = put32(gArgs, 2);
    int32_t kl = length(k1);
    at += put32(gArgs + at, (uint32_t)kl);
    memcpy(gArgs + at, k1, (unsigned long)kl);
    at += kl;
    at += put32(gArgs + at, (uint32_t)l1);
    memcpy(gArgs + at, v1, (unsigned long)l1);
    at += l1;
    kl = length(k2);
    at += put32(gArgs + at, (uint32_t)kl);
    memcpy(gArgs + at, k2, (unsigned long)kl);
    at += kl;
    at += put32(gArgs + at, (uint32_t)l2);
    memcpy(gArgs + at, v2, (unsigned long)l2);
    return at + l2;
}

static void say(int32_t ctx, const char *text, int32_t tlen, int32_t alen) {
    basalt_command_reply(ctx, text, tlen, gArgs, alen);
}

static int64_t readI64(const char *at) {
    int64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | (uint8_t)at[i];
    }
    return value;
}

static uint32_t readU32(const char *at) {
    return (uint32_t)(uint8_t)at[0] | ((uint32_t)(uint8_t)at[1] << 8) | ((uint32_t)(uint8_t)at[2] << 16) |
           ((uint32_t)(uint8_t)at[3] << 24);
}

static void onEnable(int32_t ctx) {
    (void)ctx;
    basalt_log(BASALT_LOG_INFO, S("hello enabled"));
    int32_t online = basalt_players_online((int32_t *)gBuf, 16);
    int32_t names = basalt_player_name(99, gBuf, 64);
    int32_t sent = basalt_player_send(99, S("x"), 0, 0);
    int64_t out = 0;
    int32_t missing = basalt_event_field_i64(ctx, S("nope"), &out);
    int32_t absent = basalt_translate(S("missing-key"), 0, 0, gBuf, 64);
    int32_t tiny = basalt_translate(S("welcome"), gArgs, packOne("player", "Steve", 5), gBuf, 4);
    (void)online;
    (void)names;
    (void)sent;
    (void)missing;
    (void)absent;
    (void)tiny;
}

static void onJoin(int32_t ctx) {
    int32_t player = basalt_event_player(ctx);
    int32_t n = basalt_event_field_str(ctx, S("name"), gBuf, 64);
    int64_t online = 0;
    basalt_event_field_i64(ctx, S("online"), &online);
    int64_t coins = basalt_data_get_i64(player, S("coins"), -1);
    if (coins < 0) {
        coins = 100 + online;
        basalt_data_set_i64(player, S("coins"), coins);
        basalt_data_set_bool(player, S("seen"), 1);
        basalt_data_set_str(player, S("motto"), S("be brave"));
    }
    basalt_player_send(player, S("@welcome"), gArgs, packOne("player", gBuf, n));
    basalt_player_action_bar(player, S("<green>online"), 0, 0);
    basalt_player_particle(player, S("minecraft:heart"), 5, 0.25);
    basalt_player_sound(player, S("minecraft:entity.player.levelup"), 0.5f, 1.25f);
    basalt_event_cancel(ctx, 0);
}

static void onLeave(int32_t ctx) {
    int32_t player = basalt_event_player(ctx);
    int32_t seen = basalt_data_get_bool(player, S("seen"), 0);
    int32_t len = basalt_data_get_str(player, S("motto"), gBuf, 64);
    basalt_log(BASALT_LOG_DEBUG, gBuf, len > 0 ? len : 0);
    basalt_data_remove(player, S("motto"));
    basalt_data_remove(player, S("motto"));
    (void)seen;
}

static void onCoins(int32_t ctx) {
    int32_t console = basalt_command_is_console(ctx);
    int32_t n = basalt_command_source_name(ctx, gBuf, 64);
    int32_t t = basalt_command_arg_str(ctx, S("target"), gBuf + 100, 64);
    if (t == BASALT_ERR_ABSENT) {
        if (console) {
            say(ctx, S("@console-only"), 0);
            return;
        }
        int32_t self = basalt_player_by_name(gBuf, n);
        char digits[24];
        int32_t dl = decimal(basalt_data_get_i64(self, S("coins"), 0), digits);
        say(ctx, S("@coins-self"), packOne("coins", digits, dl));
        return;
    }
    int32_t other = basalt_player_by_name(gBuf + 100, t);
    if (other == 0) {
        say(ctx, S("@no-such-player"), packOne("player", gBuf + 100, t));
        return;
    }
    char digits[24];
    int32_t dl = decimal(basalt_data_get_i64(other, S("coins"), 0), digits);
    say(ctx, S("@coins-other"), packTwo("player", gBuf + 100, t, "coins", digits, dl));
}

static void onTop(int32_t ctx) {
    int64_t limit = 10;
    basalt_command_arg_i64(ctx, S("limit"), &limit);
    int32_t size = basalt_store_top(S("coins"), (int32_t)limit, gBuf, 200);
    int32_t at = 0;
    int32_t rank = 1;
    while (at < size) {
        uint32_t nlen = readU32(gBuf + at);
        const char *name = gBuf + at + 4;
        int64_t coins = readI64(name + nlen);
        char r[24];
        char c[24];
        int32_t rl = decimal(rank, r);
        int32_t cl = decimal(coins, c);
        int32_t a = put32(gArgs, 3);
        a += put32(gArgs + a, 4);
        memcpy(gArgs + a, "rank", 4);
        a += 4;
        a += put32(gArgs + a, (uint32_t)rl);
        memcpy(gArgs + a, r, (unsigned long)rl);
        a += rl;
        a += put32(gArgs + a, 6);
        memcpy(gArgs + a, "player", 6);
        a += 6;
        a += put32(gArgs + a, nlen);
        memcpy(gArgs + a, name, nlen);
        a += (int32_t)nlen;
        a += put32(gArgs + a, 5);
        memcpy(gArgs + a, "coins", 5);
        a += 5;
        a += put32(gArgs + a, (uint32_t)cl);
        memcpy(gArgs + a, c, (unsigned long)cl);
        a += cl;
        say(ctx, S("@top-row"), a);
        at += 4 + (int32_t)nlen + 8;
        ++rank;
    }
}

static void onPeriodic(int32_t ctx) {
    (void)ctx;
    ++gFirings;
    char digits[24];
    int32_t dl = decimal(gFirings, digits);
    int32_t n = basalt_translate(S("ticking"), gArgs, packOne("n", digits, dl), gBuf, 64);
    basalt_log(BASALT_LOG_INFO, gBuf, n);
    if (gFirings == 3) {
        int32_t cancelled = basalt_task_cancel(gPeriodic);
        int32_t again = basalt_task_cancel(gPeriodic);
        (void)cancelled;
        (void)again;
    }
}

static void onLater(int32_t ctx) {
    (void)ctx;
    basalt_log(BASALT_LOG_WARN, S("one shot fired"));
}

static void onPlaceholder(int32_t ctx) {
    int32_t player = basalt_placeholder_player(ctx);
    if (player == 0) {
        basalt_placeholder_result(ctx, S("n/a"));
        return;
    }
    int32_t len = decimal(basalt_data_get_i64(player, S("coins"), 0), gBuf);
    basalt_placeholder_result(ctx, gBuf, len);
}

static void onClick(int32_t ctx) {
    int32_t player = basalt_menu_click_player(ctx);
    basalt_player_send(player, S("@bought"), gArgs, packOne("item", "Sparkles", 8));
    int64_t coins = basalt_data_get_i64(player, S("coins"), 0);
    basalt_data_set_i64(player, S("coins"), coins - basalt_config_i64(S("shop.price"), 10));
}

BASALT_EXPORT(basalt_abi_version) int32_t basalt_abi_version(void) { return BASALT_ABI_VERSION; }

BASALT_EXPORT(basalt_init) void basalt_init(void) {
    basalt_log(BASALT_LOG_INFO, S("hello init"));
    basalt_event_on(S("plugin.enable"), BASALT_PRIORITY_NORMAL, kEnable);
    basalt_event_on(S("player.join"), BASALT_PRIORITY_HIGH, kJoin);
    basalt_event_on(S("player.leave"), BASALT_PRIORITY_LOW, kLeave);
    basalt_event_on(S("not.an.event"), BASALT_PRIORITY_LOW, 0);
    basalt_command_register(S("coins [target: word]"), S("hello.coins"), kCoins);
    basalt_command_register(S("coins top [limit: int 1..25]"), 0, 0, kTop);
    basalt_command_register(S("[broken"), 0, 0, 0);
    gPeriodic = basalt_task_every(20, 20, kPeriodic);
    basalt_task_after(5, kLater);
    basalt_placeholder_register(S("coins"), kPlaceholder);
    gMenu = basalt_menu_create(S("<gold>Shop"), 3);
    basalt_menu_set(gMenu, 13, S("minecraft:diamond"), S("<aqua>Sparkles"), S("first line\nsecond line"), kClick);
    basalt_menu_set(gMenu, 99, S("minecraft:stone"), 0, 0, 0, 0, 0);
    int64_t price = basalt_config_i64(S("shop.price"), 1);
    double rate = basalt_config_f64(S("shop.rate"), 0.0);
    int32_t enabled = basalt_config_bool(S("shop.enabled"), 0);
    int32_t n = basalt_config_str(S("shop.name"), gBuf, 64);
    basalt_log(BASALT_LOG_INFO, gBuf, n);
    (void)price;
    (void)rate;
    (void)enabled;
}

BASALT_EXPORT(basalt_callback) void basalt_callback(int32_t token, int32_t ctx) {
    switch (token) {
    case kEnable:
        onEnable(ctx);
        break;
    case kJoin:
        onJoin(ctx);
        break;
    case kLeave:
        onLeave(ctx);
        break;
    case kCoins:
        onCoins(ctx);
        break;
    case kTop:
        onTop(ctx);
        break;
    case kPeriodic:
        onPeriodic(ctx);
        break;
    case kLater:
        onLater(ctx);
        break;
    case kPlaceholder:
        onPlaceholder(ctx);
        break;
    case kClick:
        onClick(ctx);
        break;
    default:
        break;
    }
}
