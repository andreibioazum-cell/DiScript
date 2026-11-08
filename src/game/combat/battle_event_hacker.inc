/* Event 7 (H4c4k0r raid): a local, scripted admin-vs-hacker battle.
 * Nothing here executes code or changes server permissions. The payload is
 * only a tiny command table; every "event" it asks for becomes a decorative,
 * inert prop. Azum's second console opens after the props are actually on
 * screen, and the final ban is a long blackout followed by an animated
 * terminal shutdown. The existing button cover / cane compensation remains. */

#define HACK_EVENT 7

/* Scene beats in seconds. Timings are never printed on screen. */
#define HACK_GLITCH_TIME 1.2
#define HACK_SECOND_AT 46.0
#define HACK_REPLY_AT 49.0
#define HACK_BTN_START 70.0
#define HACK_BTN_INTERVAL 1.25
#define HACK_BAN_AT 100.0
/* A deliberately long, silent crash-like blackout (was 1.2 s). */
#define HACK_BLACK_DUR 18.0
#define HACK_BAN_CPS 24.0
#define HACK_CMD_CPS 30.0
#define HACK_TYPE_CPS 22.0
#define HACK_CANE_SPEED 250.0
#define HACK_CANE_INTERVAL 0.35
#define HACK_CANE_LIFE 22.0
#define HACK_FINISH_GRACE 1.6
#define HACK_BTN_GRAVITY 1500.0

/* Payload container ids (assets/hack/, see tools/gen_hack_payload.py). */
#define HACK_PLAT_PHONE 1
#define HACK_PLAT_PC 2
#define HACK_KIND_EVENT 1
#define HACK_KIND_CLOUD 2
#define HACK_CMD_MAX 8
#define HACK_STUB_MAX 6

/* Compact cmd-style title bar height, shared by logic (drag) and draw. */
static double hack_term_head(void) {
    return clamp(screen_h * 0.032, 18, 28);
}

static double hack_term_w(void) {
#ifdef CB_PC
    return clamp(screen_w * 0.34, 280, 480);
#else
    return clamp(screen_w * 0.70, 200, 380);
#endif
}

/* Terminal speakers. */
#define HACK_WHO_WARN 0
#define HACK_WHO_HACK 1
#define HACK_WHO_AZUM 2

/* Junk button slots. */
#define HB_ON 0
#define HB_LABEL 1
#define HB_X 2
#define HB_Y 3
#define HB_W 4
#define HB_H 5
#define HB_VY 6
#define HB_REST 7
#define HB_LAND 8
#define HB_PRESS 9
#define HB_FIELDS 10
#define HACK_BTN_MAX 16

/* Compensation cane slots. */
#define HC_ON 0
#define HC_X 1
#define HC_Y 2
#define HC_DX 3
#define HC_LAND 4
#define HC_POP 5
#define HC_FIELDS 6
#define HACK_CANE_MAX 10

typedef struct {
    double at;
    double win;
    double who;
    const char *ru;
    const char *en;
} HackLine;

/* Window 0: hacker. Window 1: Azum, opened later by hack_second_open(). */
static const HackLine hack_script[] = {
    {0.8, 0, HACK_WHO_WARN, "[WArNiNg]: External connection detected...", "[WArNiNg]: External connection detected..."},
    {2.6, 0, HACK_WHO_HACK,
     "Ну привет, кубики. Я просканировал систему, подгрузил .bin/.so скрипт и сейчас включу все ваши режимы по очереди!",
     "Hi there, cubes. I scanned the system, loaded the .bin/.so script and now I will switch on every mode, one by one!"},
    {10.0, 0, HACK_WHO_HACK, "/event 1", "/event 1"},
    {14.0, 0, HACK_WHO_HACK, "/event 2", "/event 2"},
    {18.0, 0, HACK_WHO_HACK, "/event 3", "/event 3"},
    {22.0, 0, HACK_WHO_HACK, "/event 4", "/event 4"},
    {26.0, 0, HACK_WHO_HACK, "/event 5", "/event 5"},
    {30.0, 0, HACK_WHO_HACK, "/event 6", "/event 6"},
    {36.0, 0, HACK_WHO_HACK, "/event 10", "/event 10"},
    {40.0, 0, HACK_WHO_HACK, "/event 11", "/event 11"},
    {46.5, 1, HACK_WHO_AZUM, "Защищённый канал подключён. Я вижу твои пустышки.", "Secure channel attached. I see your empty props."},
    {49.0, 1, HACK_WHO_AZUM, "ХАХАХАХАХАХА! Серьёзно?!", "HAHAHAHAHAHAHA! Seriously?!"},
    {51.5, 1, HACK_WHO_AZUM,
     "Вот теперь вижу: одно жалкое облачко и торнадо, который даже не крутит. Диско молчит, плиты пустые, Дед Мороз, куб и Колонки стоят как манекены. Ни одна твоя команда не работает!",
     "Now I see it: one pathetic cloud and a tornado that does not even spin. Disco is silent, plates are empty, Santa, the cube and Columns stand like dummies. None of your commands work!"},
    {60.0, 0, HACK_WHO_HACK,
     "Что ты смеёшься?! Подумаешь, облачко! Я сломаю всю твою игру, каждую строчку кода!",
     "What are you laughing at?! Big deal, one cloud! I will break your whole game, every line of code!"},
    {65.0, 0, HACK_WHO_HACK,
     "Ладно... режимы не слушаются. Тогда я завалю вас кнопками! Нажмите \"УДАЛИТЬ ИГРУ\"! Быстро! Почему вы не выходите?!",
     "Fine... the modes ignore me. Then I will bury you in buttons! Press \"DELETE THE GAME\"! Now! Why are you not quitting?!"},
    {78.0, 1, HACK_WHO_AZUM,
     "Потому что это пустышки, гений кода. Мои игроки прыгают на них как на батутах.",
     "Because they are empty shells, code genius. My players bounce on them like trampolines."},
    {83.0, 0, HACK_WHO_HACK,
     "ААААА! Я УНИЧТОЖУ СЕРВЕР! Я СЛОМАЮ ВАШ ПРЕМИУМ!",
     "AAAAH! I WILL DESTROY THE SERVER! I WILL BREAK YOUR PREMIUM!"},
    {88.0, 1, HACK_WHO_AZUM,
     "Хватит. Я перехватил твоё соединение. Сейчас закрою твой терминал.",
     "Enough. I intercepted your connection. Now I am closing your terminal."},
    {94.0, 0, HACK_WHO_HACK, "Нет! Подожди! Я успею переподключиться: 3... 2... 1...", "No! Wait! I can reconnect in time: 3... 2... 1..."},
};
#define HACK_LINES ((double)(sizeof(hack_script) / sizeof(hack_script[0])))

/* The payload fires only after the matching /event line has finished typing. */
static const double hack_cmd_at[HACK_CMD_MAX] = {10.6, 14.6, 18.6, 22.6, 26.6, 30.6, 36.6, 40.6};
static double hack_cmd_id[HACK_CMD_MAX];
static double hack_cmd_kind[HACK_CMD_MAX];
static double hack_cmd_life[HACK_CMD_MAX];
static double hack_cmd_n;
static double hack_btn_total;
static double hack_cane_total;
static double hack_cane_candy;
static double hack_boomed;

static double hack_enabled(void) {
    if (game_state != ST_SOLO && game_state != ST_ONLINE) {
        return 0;
    }
    if (event_mode == HACK_EVENT) {
        return 1;
    }
    return admin_cheat_hack();
}

static double hack_detect_platform(void) {
#ifdef CB_PC
    return HACK_PLAT_PC;
#else
    return HACK_PLAT_PHONE;
#endif
}

static double hack_load_payload(void) {
    static unsigned char buf[96];
    const char *name;
    double plat;
    int n;
    int i;
    unsigned sum = 0;
    plat = hack_detect_platform();
    name = plat == HACK_PLAT_PC ? "hack/h4c4k0r.so" : "hack/h4c4k0r.bin";
    hack_platform = plat;
    n = ds_asset_read(name, buf, (int)sizeof(buf));
    hack_payload_bytes = n > 0 ? (double)n : 0;
    if (n < 16) {
        return 0;
    }
    if (plat == HACK_PLAT_PC) {
        if (memcmp(buf, "H4CKSO1", 7) != 0 || buf[7] != 0 || buf[8] != HACK_PLAT_PC) {
            return 0;
        }
    } else if (memcmp(buf, "H4CKBIN1", 8) != 0 || buf[8] != HACK_PLAT_PHONE) {
        return 0;
    }
    for (i = 0; i < n - 1; i++) {
        sum = sum + (unsigned)buf[i];
    }
    if ((sum & 0xFFu) != (unsigned)buf[n - 1]) {
        return 0;
    }
    hack_cmd_n = 0;
    double cnt = buf[10];
    if (cnt > HACK_CMD_MAX) {
        cnt = HACK_CMD_MAX;
    }
    i = 0;
    while (i < (int)cnt && 11 + i * 3 + 2 < n) {
        hack_cmd_id[i] = buf[11 + i * 3];
        hack_cmd_kind[i] = buf[11 + i * 3 + 1];
        hack_cmd_life[i] = buf[11 + i * 3 + 2];
        hack_cmd_n = hack_cmd_n + 1;
        i = i + 1;
    }
    int tail = 11 + (int)cnt * 3;
    if (tail + 2 < n) {
        hack_btn_total = buf[tail];
        hack_cane_total = buf[tail + 1];
        hack_cane_candy = buf[tail + 2];
    }
    if (hack_btn_total > HACK_BTN_MAX) {
        hack_btn_total = HACK_BTN_MAX;
    }
    if (hack_cane_total > HACK_CANE_MAX) {
        hack_cane_total = HACK_CANE_MAX;
    }
    ds_log("h4c4k0r: payload %s ok (%d bytes, %g cmds, %g buttons, %g canes)", name, n, hack_cmd_n,
           hack_btn_total, hack_cane_total);
    return 1;
}

/* Inert props only: no event_mode switch, no sound, no collision, no reward. */
static void hack_fire_cmd(double id, double kind, double life) {
    (void)life;
    if (kind == HACK_KIND_EVENT && id >= 1 && id <= HACK_STUB_MAX) {
        int slot = (int)id - 1;
        if (hack_stub_on[slot] == 0) {
            hack_stub_on[slot] = 1;
            hack_stub_t[slot] = hack_t;
        }
        return;
    }
    if (kind == HACK_KIND_CLOUD && id == 10 && hack_tornado_on == 0) {
        hack_tornado_on = 1;
        hack_tornado_t = hack_t;
        return;
    }
    if (kind == HACK_KIND_CLOUD && id == 11 && hack_cloud_on == 0) {
        hack_cloud_on = 1;
        /* Below the first console and above Azum's later window: always visible. */
        hack_cloud_x = screen_w - 58;
        hack_cloud_y = screen_h * 0.33;
        hack_cloud_t = 0;
    }
}

/* Hacker uses window 0; Azum's replies use the delayed window 1. */
static double hack_line_win(double i) {
    int k = (int)i;
    if (k < 0 || k >= (int)HACK_LINES) {
        return 0;
    }
    return hack_script[k].who == HACK_WHO_AZUM ? 1 : 0;
}

static double hack_second_open(void) {
    return hack_active == 1 && hack_cloud_on == 1 && hack_cloud_t >= 4.0 && hack_t >= HACK_SECOND_AT;
}

/* Ban dialogue lives in the draw layer but its timing is shared with logic. */
static const char *hack_ban_l1(void);
static const char *hack_ban_l2(void);
static const char *hack_ban_reconnect(void);
static const char *hack_ban_denied(void);
static const char *hack_ban_exit(void);
static double hack_ban_verdict_end(void);
static double hack_ban_reply_end(void);
static double hack_ban_exit_end(void);
static double hack_ban_click_at(void);
static double hack_ban_closed_at(void);
static double hack_ban_done_at(void);
static double hack_ban_scene_at(void);
static double hack_rain_at(void);

/* Freeze the fight while the screen is black / the ban terminal closes. */
static double hack_cinematic_lock(void) {
    return hack_active == 1 && hack_ban_t > 0 && hack_ban_t < hack_ban_done_at();
}

/* The merged console drags by its title bar; the grab swallows the pointer. */
static double hack_drag_touch(double x, double y, double action, double id) {
    double w = hack_term_w();
    double head = hack_term_head();
    if (hack_cinematic_lock() == 1) {
        return 1;
    }
    if (hack_drag_win < 0 && action == 0 && hack_active == 1 && hack_t < hack_rain_at()) {
        int win = hack_second_open() == 1 ? 1 : 0;
        while (win >= 0) {
            if (hit_rect(x, y, hack_term_x[win], hack_term_y[win], w, head) == 1) {
                hack_drag_win = win;
                hack_drag_id = id;
                hack_drag_dx = x - hack_term_x[win];
                hack_drag_dy = y - hack_term_y[win];
                return 1;
            }
            win = win - 1;
        }
        return 0;
    }
    if (hack_drag_win < 0 || id != hack_drag_id) {
        return 0;
    }
    int win = (int)hack_drag_win;
    if (action == 0 || action == 2) {
        hack_term_x[win] = clamp(x - hack_drag_dx, 4, screen_w - w - 4);
        hack_term_y[win] = clamp(y - hack_drag_dy, 4, screen_h - head - 40);
    }
    if (action == 1 || action == 3) {
        hack_drag_win = -1;
        hack_drag_id = -1;
    }
    return 1;
}
