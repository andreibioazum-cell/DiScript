/* Single-threaded PC keyboard state implementing runtime keyboard hooks. */
#define KB_BUF 512

/* Subset of Android keycodes handled by keyboard_handle_key(). */
enum {
    PC_KEY_ENTER = 66,
    PC_KEY_DEL = 67,
    PC_KEY_SPACE = 62,
    PC_KEY_A = 29,
    PC_KEY_Z = 54,
    PC_KEY_0 = 7,
    PC_KEY_9 = 16
};

static char kb_text[KB_BUF] = {0};
static int kb_len = 0;
static int kb_show = 0;
static int kb_enter = 0;
static BackspaceRepeat kb_del_repeat = {0, 0};
static uint64_t kb_clock_ms = 0;

void ds_set_activity(void *act) { (void)act; }

int keyboard_uses_editor(void) { return kb_show; }

void keyboard_show(void) { kb_show = 1; }

void keyboard_hide(void) {
    kb_show = 0;
    kb_enter = 0;
    keyboard_cancel_repeat();
}

/* Copy current text input buffer into caller-owned storage. */
void keyboard_get_text_to(char *dst, size_t cap) {
    if (!dst || cap == 0)
        return;
    snprintf(dst, cap, "%s", kb_text);
}

void keyboard_clear(void) {
    kb_text[0] = '\0';
    kb_len = 0;
    kb_enter = 0;
    keyboard_cancel_repeat();
}

int keyboard_visible(void) { return kb_show; }

int keyboard_enter_pressed(void) {
    int enter = kb_enter;
    kb_enter = 0;
    return enter;
}

static void kb_append_cp(unsigned int cp) {
    char u[5];
    int n = 0;
    if (cp < 0x80) {
        u[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        u[0] = (char)(0xC0 | (cp >> 6));
        u[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        u[0] = (char)(0xE0 | (cp >> 12));
        u[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else if (cp <= 0x10FFFF) {
        u[0] = (char)(0xF0 | (cp >> 18));
        u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        u[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    if (n > 0 && kb_len + n < KB_BUF) {
        memcpy(kb_text + kb_len, u, (size_t)n);
        kb_len += n;
        kb_text[kb_len] = '\0';
    }
}

void keyboard_type(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!p)
        return;
    while (*p) {
        unsigned int cp;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = (unsigned int)((*p & 0x1F) << 6) | (unsigned int)(p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = (unsigned int)((*p & 0x0F) << 12) | (unsigned int)((p[1] & 0x3F) << 6) | (unsigned int)(p[2] & 0x3F);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = (unsigned int)((*p & 0x07) << 18) | (unsigned int)((p[1] & 0x3F) << 12) |
                 (unsigned int)((p[2] & 0x3F) << 6) | (unsigned int)(p[3] & 0x3F);
            p += 4;
        } else {
            p++;
            continue;
        }
        /* Ignore ASCII/C1 control characters; Enter and Backspace use key events. */
        if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F))
            continue;
        kb_append_cp(cp);
    }
}

void keyboard_backspace(void) {
    if (kb_len <= 0)
        return;
    /* Delete trailing UTF-8 codepoint. */
    int n = 1;
    while (n < kb_len && n < 4 && (kb_text[kb_len - n] & 0xC0) == 0x80)
        n++;
    kb_len -= n;
    if (kb_len < 0)
        kb_len = 0;
    kb_text[kb_len] = '\0';
}

static void kb_erase_one_cb(void *userdata) {
    (void)userdata;
    keyboard_backspace();
}

void keyboard_cancel_repeat(void) {
    backspace_repeat_up(&kb_del_repeat);
}

void keyboard_tick(uint64_t now_ms) {
    kb_clock_ms = now_ms;
    if (!kb_show) {
        keyboard_cancel_repeat();
        return;
    }
    backspace_repeat_tick(&kb_del_repeat, now_ms, kb_erase_one_cb, NULL);
}

/* Process Android-numbered key events and Enter key presses. */
int keyboard_handle_key(int keycode, int action, int meta) {
    (void)meta;
    if (keycode == PC_KEY_DEL) {
        if (action == 0) {
            backspace_repeat_down(&kb_del_repeat, kb_clock_ms, kb_erase_one_cb, NULL);
        } else if (action == 1) {
            backspace_repeat_up(&kb_del_repeat);
        }
        return 1;
    }
    if (action != 0 && action != 2) /* down / multiple only */
        return 0;
    if (keycode == PC_KEY_ENTER) {
        kb_enter = 1;
        return 1;
    }
    if (keycode == PC_KEY_SPACE) {
        kb_append_cp(' ');
        return 1;
    }
    if (keycode >= PC_KEY_A && keycode <= PC_KEY_Z) {
        kb_append_cp((unsigned int)('a' + keycode - PC_KEY_A));
        return 1;
    }
    if (keycode >= PC_KEY_0 && keycode <= PC_KEY_9) {
        kb_append_cp((unsigned int)('0' + keycode - PC_KEY_0));
        return 1;
    }
    return 0;
}

/* pc_main: physical Enter without Android key codes. */
void pc_keyboard_enter(void) { kb_enter = 1; }
