/* Event 7 (H4c4k0r raid): a scripted scene where an unskilled hacker breaks
 * into the round. Everything he starts is a broken local dummy: his payload
 * file (.so on PC, .bin on phones) loads per device, his /event commands turn
 * into tiny dead stubs plus one harmless cloud, his menu buttons rain down as
 * solid cover, then the ban wipes the terminals and the players get a New
 * Year candy-cane compensation. The timeline below is scene order only; no
 * timer is ever shown on screen. */

#define HACK_EVENT 7

/* Scene beats in seconds since the raid started. */
#define HACK_GLITCH_TIME 1.2
#define HACK_BTN_START 40.0
#define HACK_BTN_INTERVAL 1.7
#define HACK_BAN_AT 75.0
/* The ban opens with a full blackout: the game looks crashed for this long. */
#define HACK_BLACK_DUR 1.2
#define HACK_WINS 3
#define HACK_RAIN_AT 80.0
#define HACK_TYPE_CPS 22.0
#define HACK_CMD_CPS 30.0
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

/* Compact cmd-style title bar height, shared by logic (drag) and draw. */
static double hack_term_head(void) {
    return clamp(screen_h * 0.032, 18, 28);
}

/* Cmd window width: compact on both form factors, never over the arena. */
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

/* The whole argument, three terminal windows, typed live by both sides. */
static const HackLine hack_script[] = {
    {0.8, 0, HACK_WHO_WARN, "[Warning]: External connection detected...", "[Warning]: External connection detected..."},
    {2.6, 0, HACK_WHO_HACK,
     "Hi there, cubes. I just scanned your system, loaded the right .bin/.so script and now I will switch on every single mode at once!",
     "Hi there, cubes. I just scanned your system, loaded the right .bin/.so script and now I will switch on every single mode at once!"},
    {9.0, 0, HACK_WHO_HACK, "/event 1", "/event 1"},
    {10.5, 0, HACK_WHO_HACK, "/event 2", "/event 2"},
    {12.0, 0, HACK_WHO_HACK, "/event 10", "/event 10"},
    {13.5, 0, HACK_WHO_HACK, "/event 11", "/event 11"},
    {20.0, 0, HACK_WHO_AZUM,
     "HAHAHAHAHAHAHA! Seriously?!",
     "HAHAHAHAHAHAHA! Seriously?!"},
    {22.0, 0, HACK_WHO_AZUM,
     "You loaded your .bin/.so file, typed /event 10, and one pathetic little cloud popped up in the corner of the screen? Your commands do nothing at all! I am falling over laughing!",
     "You loaded your .bin/.so file, typed /event 10, and one pathetic little cloud popped up in the corner of the screen? Your commands do nothing at all! I am falling over laughing!"},
    {28.0, 0, HACK_WHO_HACK,
     "What are you laughing at?! What are you laughing at, anyway?! Big deal, one cloud!",
     "What are you laughing at?! What are you laughing at, anyway?! Big deal, one cloud!"},
    {32.0, 0, HACK_WHO_HACK,
     "I will break your whole game! Every single line of code, walking cube! You will cry for me!",
     "I will break your whole game! Every single line of code, walking cube! You will cry for me!"},
    {40.0, 1, HACK_WHO_HACK,
     "So, done laughing, undertightened?! Look how many buttons are on screen! Press the \"DELETE THE GAME\" button!! Now!!! Why are you not quitting?!",
     "So, done laughing, undertightened?! Look how many buttons are on screen! Press the \"DELETE THE GAME\" button!! Now!!! Why are you not quitting?!"},
    {48.0, 1, HACK_WHO_AZUM,
     "Because they are empty shells, code genius. My players bounce on them like on trampolines.",
     "Because they are empty shells, code genius. My players bounce on them like on trampolines."},
    {53.0, 1, HACK_WHO_HACK,
     "AAAAH, I WILL BREAK YOUR PREMIUM! HEAR ME, CUBE?! I WILL DESTROY THE SERVER!",
     "AAAAH, I WILL BREAK YOUR PREMIUM! HEAR ME, CUBE?! I WILL DESTROY THE SERVER!"},
    {57.0, 1, HACK_WHO_AZUM,
     "Oh please, terminator will not let you. I am tired of reading this. Stay well.",
     "Oh please, terminator will not let you. I am tired of reading this. Stay well."},
    {72.0, 2, HACK_WHO_HACK, "Oh really?! I will do all of this again!", "Oh really?! I will do all of this again!"},
    {73.2, 2, HACK_WHO_HACK, "1... 2... 3...", "1... 2... 3..."},
    {74.6, 2, HACK_WHO_HACK, "1", "1"},
};
#define HACK_LINES ((double)(sizeof(hack_script) / sizeof(hack_script[0])))
#define HACK_WINS 3

/* Payload command schedule: the hacker fires his table in this order. */
static const double hack_cmd_at[4] = {9.0, 10.5, 12.0, 13.5};

/* Parsed payload table (filled by hack_load_payload). */
static double hack_cmd_id[4];
static double hack_cmd_kind[4];
static double hack_cmd_life[4];
static double hack_cmd_n;
static double hack_btn_total;
static double hack_cane_total;
static double hack_cane_candy;
static double hack_boomed;

static double hack_enabled(void) {
    /* Event only in offline (solo) mode, regardless of internet */
    if (game_state == ST_SOLO) {
        return 1;
    }
    return 0;
}

/* The game ships for exactly two devices, so the raid looks at which one it
 * landed on: a Windows PC or an Android phone. Nothing else exists. */
static double hack_detect_platform(void) {
#ifdef CB_PC
    return HACK_PLAT_PC;      /* Windows */
#else
    return HACK_PLAT_PHONE;   /* Android */
#endif
}

/* Read this device's payload container and parse the hacker's command table. */
static double hack_load_payload(void) {
    static unsigned char buf[64];
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
    if (cnt > 4) {
        cnt = 4;
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

/* One typed command: real events come up as dead stubs, clouds as one dummy. */
/* His commands switch nothing on: the timers tick in silence, the cloud just
 * hangs there. No stub visuals, no sounds, no "broken" notes anywhere. */
static void hack_fire_cmd(double id, double kind, double life) {
    /* Event only in offline (solo) mode */
    if (game_state != ST_SOLO) {
        return;
    }
    if (kind == HACK_KIND_EVENT) {
        if (id == 1) {
            hack_stub_disco_t = life;
        } else if (id == 2) {
            hack_stub_plates_t = life;
        }
        
        /* Trigger solo event mode */
        event_mode = (int)id;
        return;
    }
    if (kind == HACK_KIND_CLOUD && life > 0 && hack_cloud_on == 0) {
        /* Cloud removed entirely in draw, but keep logic for compatibility */
        hack_cloud_on = 1;
        hack_cloud_x = screen_w - 62;
        hack_cloud_y = 64;
        hack_cloud_t = 0;
    }
}

/* Force single window (win=0) for all script lines */
static double hack_line_win(double i) {
    return 0; /* All chat merged into one window (top-left) */
}

/* Terminals drag by their green title bar; the grab swallows the pointer so
 * the fight underneath is not disturbed while a window moves. */
static double hack_drag_touch(double x, double y, double action, double id) {
    double w = hack_term_w();
    double head = hack_term_head();
    if (hack_drag_win < 0 && action == 0 && hack_active == 1 && hack_t < HACK_RAIN_AT) {
        double wi = 0;
        while (wi < HACK_WINS) {
            if (hit_rect(x, y, hack_term_x[(int)wi], hack_term_y[(int)wi], w, head) == 1) {
                hack_drag_win = wi;
                hack_drag_id = id;
                hack_drag_dx = x - hack_term_x[(int)wi];
                hack_drag_dy = y - hack_term_y[(int)wi];
                return 1;
            }
            wi = wi + 1;
        }
        return 0;
    }
    if (hack_drag_win < 0 || id != hack_drag_id) {
        return 0;
    }
    if (action == 0 || action == 2) {
        /* Force single window position (top-left) */
        hack_term_x[(int)hack_drag_win] = 4;
        hack_term_y[(int)hack_drag_win] = 4;
    }
    if (action == 1 || action == 3) {
        hack_drag_win = -1;
        hack_drag_id = -1;
    }
    return 1;
}