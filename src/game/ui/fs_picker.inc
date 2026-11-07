/* Built-in file manager: pick a .zip from the phone or the disk and import it
 * as a texture pack. On Android the system document picker handles folders the
 * game cannot list by itself. */

static double fs_page;
static char fs_dir[256];
static char fs_names[4096];
static int fs_count;
static unsigned char fs_scratch[PACK_ZIP_MAX];

static const char *tr_fs_title(void) { return packs_tr("Импорт пака (.zip)", "Import pack (.zip)"); }
static const char *tr_fs_home(void) { return packs_tr("Загрузки", "Downloads"); }
static const char *tr_fs_packs(void) { return packs_tr("Паки", "Packs"); }
static const char *tr_fs_up(void) { return packs_tr("Наверх", "Up"); }
static const char *tr_fs_close(void) { return packs_tr("Закрыть", "Close"); }
static const char *tr_fs_refresh(void) { return packs_tr("Обновить", "Refresh"); }
#if defined(__ANDROID__)
static const char *tr_fs_system(void) { return packs_tr("Телефон", "Phone"); }
#else
static const char *tr_fs_system(void) { return packs_tr("Проводник", "Explorer"); }
#endif
static const char *tr_fs_no_access(void) { return packs_tr("Нет доступа к этой папке", "No access to this folder"); }
static const char *tr_fs_empty(void) { return packs_tr("Тут нет .zip и папок", "No .zip or folders here"); }

static double fs_rows(void) { return 12; }
static double fs_row_h(void) { return 40; }
static double fs_list_y(void) { return packs_card_y() + 132; }
static double fs_btn_y(double row) { return packs_card_y() + packs_card_h() - 24 - (3 - row) * (btn_h + 10); }

static void fs_copy(char *dst, size_t cap, const char *src) {
    if (!dst || !cap)
        return;
    snprintf(dst, cap, "%s", src ? src : "");
}

static int fs_keep(const char *name) {
    int len = (int)str_len(name);
    if (len > 0 && name[len - 1] == '/')
        return 1;
    return packs_ends_with(name, ".zip");
}

static void fs_refresh(void) {
    static char raw[4096];
    static char items[64][128];
    int n, p = 0, count = 0, i, j, used = 0;
    fs_count = 0;
    fs_names[0] = 0;
    n = net_fs_list(fs_dir, raw, (int)sizeof(raw));
    while (p < (int)str_len(raw) && count < 64) {
        char one[128];
        int k = 0;
        while (raw[p] && raw[p] != '\n' && k < 127)
            one[k++] = raw[p++];
        one[k] = 0;
        if (raw[p] == '\n')
            p = p + 1;
        if (k == 0 || fs_keep(one) != 1)
            continue;
        memcpy(items[count], one, (size_t)k + 1);
        count = count + 1;
    }
    (void)n;
    if (count == 0 && fs_dir[0] && strcmp(fs_dir, ".") != 0 && net_fs_dir_readable(fs_dir) != 1)
        packs_msg_set(tr_fs_no_access());
    for (i = 1; i < count; i++) {
        char key[128];
        memcpy(key, items[i], sizeof(key));
        j = i - 1;
        while (j >= 0) {
            int a_dir = items[j][(int)str_len(items[j]) - 1] == '/';
            int b_dir = key[(int)str_len(key) - 1] == '/';
            if (a_dir > b_dir || (a_dir == b_dir && strcmp(items[j], key) <= 0))
                break;
            memcpy(items[j + 1], items[j], sizeof(key));
            j = j - 1;
        }
        memcpy(items[j + 1], key, sizeof(key));
    }
    for (i = 0; i < count; i++) {
        int len = (int)str_len(items[i]);
        if (used + len + 2 >= (int)sizeof(fs_names))
            break;
        memcpy(fs_names + used, items[i], (size_t)len);
        used = used + len;
        fs_names[used++] = '\n';
        fs_names[used] = 0;
        fs_count = fs_count + 1;
    }
}

static void fs_home_dir(void) {
    if (net_fs_home(fs_dir, sizeof(fs_dir)) != 1)
        fs_copy(fs_dir, sizeof(fs_dir), ".");
}

static void fs_open_picker(void) {
    fs_home_dir();
    fs_page = 0;
    packs_msg_t = 0;
    packs_fs_open = 1;
    fs_refresh();
    ds_log("pack picker opened: %s", fs_dir);
}

static void fs_close_picker(void) { packs_fs_open = 0; }

static void fs_enter(const char *name) {
    char next[256];
    int len = (int)str_len(name);
    while (len > 0 && name[len - 1] == '/')
        len = len - 1;
    if (str_starts_with(name, "/") == 1 || (len > 1 && name[1] == ':'))
        snprintf(next, sizeof(next), "%.*s", len, name);
    else
        snprintf(next, sizeof(next), "%s/%s", fs_dir, name);
    fs_copy(fs_dir, sizeof(fs_dir), next);
    fs_page = 0;
    fs_refresh();
}

static void fs_up_dir(void) {
    int len = (int)str_len(fs_dir);
    while (len > 1 && (fs_dir[len - 1] == '/' || fs_dir[len - 1] == '\\'))
        len = len - 1;
    while (len > 1 && fs_dir[len - 1] != '/' && fs_dir[len - 1] != '\\')
        len = len - 1;
    while (len > 1 && (fs_dir[len - 1] == '/' || fs_dir[len - 1] == '\\'))
        len = len - 1;
    if (len < 1)
        len = 1;
    fs_dir[len] = 0;
    fs_page = 0;
    fs_refresh();
}

static void fs_goto_packs(void) {
    if (net_packs_root(fs_dir, sizeof(fs_dir)) != 1)
        fs_copy(fs_dir, sizeof(fs_dir), ".");
    fs_page = 0;
    fs_refresh();
}

static int fs_entry(double row, char *name, int cap, int *is_dir) {
    int index = (int)(fs_page * fs_rows() + row);
    int i = 0, p = 0;
    if (index < 0 || index >= fs_count)
        return 0;
    while (p < (int)str_len(fs_names) && i < index) {
        if (fs_names[p] == '\n')
            i = i + 1;
        p = p + 1;
    }
    i = 0;
    *is_dir = 0;
    while (fs_names[p] && fs_names[p] != '\n' && i < cap - 1)
        name[i++] = fs_names[p++];
    name[i] = 0;
    if (i > 0 && name[i - 1] == '/') {
        *is_dir = 1;
        name[i - 1] = 0;
    }
    return 1;
}

static void fs_install(const char *name) {
    char path[512];
    int n;
    snprintf(path, sizeof(path), "%s/%s", fs_dir, name);
    n = net_fs_read(path, fs_scratch, PACK_ZIP_MAX);
    if (n <= 0) {
        packs_msg_set(packs_tr("Файл не читается", "Cannot read the file"));
        return;
    }
    packs_import_zip(fs_scratch, n, name);
    fs_close_picker();
}

static void fs_system_dialog(void) {
    char target[512];
#if defined(__ANDROID__)
    if (net_fs_pick_import() == 1)
        packs_msg_set(tr_fs_title());
    else
        packs_msg_set(tr_fs_no_access());
    return;
#endif
    if (ds_pick_open_file(target, (int)sizeof(target)) == 1) {
        int n = net_fs_read(target, fs_scratch, PACK_ZIP_MAX);
        if (n > 0)
            packs_import_zip(fs_scratch, n, target);
        else
            packs_msg_set(packs_tr("Файл не читается", "Cannot read the file"));
        return;
    }
    packs_msg_set(packs_tr("Системного диалога нет", "No system dialog here"));
}

static void draw_fs_list(void) {
    double cx = packs_card_x(), i, rows = fs_rows();
    i = 0;
    while (i < rows) {
        char name[128];
        int is_dir = 0;
        double y = fs_list_y() + i * fs_row_h();
        if (fs_entry(i, name, sizeof(name), &is_dir) != 1)
            break;
        roundrect(cx + 20, y, packs_card_w() - 40, fs_row_h() - 6, 10, is_dir ? RGB(74,58,120) : RGB(53,53,74));
        text_scaled(name, cx + 34, y + 8, text_color, 0.48);
        i = i + 1;
    }
}

static void draw_fs_picker(void) {
    double cx = packs_card_x(), cy = packs_card_y(), cw = packs_card_w(), ch = packs_card_h();
    double rows = fs_rows();
    const char *path = fs_dir;
    double page_max = (fs_count > 0) ? (double)((fs_count - 1) / (int)rows) : 0;
    char page[24];
    rect(0, 0, screen_w, screen_h, 0xCC000000);
    roundrect(cx, cy, cw, ch, 18, card_bg);
    ctext_scaled(tr_fs_title(), cy + 18, text_color, 0.7);
    if (str_len(path) > 42)
        path = path + (int)str_len(path) - 42;
    ctext_scaled(path, cy + 74, RGB(152,152,176), 0.38);
    if (fs_count == 0)
        ctext_scaled(tr_fs_empty(), fs_list_y() + 20, RGB(184,184,200), 0.45);
    draw_fs_list();
    if (page_max > 0) {
        snprintf(page, sizeof(page), "%d/%d", (int)fs_page + 1, (int)page_max + 1);
        text_scaled(page, cx + cw - 70, cy + 74, RGB(152,152,176), 0.38);
    }
    button_scaled("<", packs_action_x(0), fs_btn_y(0), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_fs_refresh(), packs_action_x(1), fs_btn_y(0), packs_action_w(), btn_h, 0.44);
    button_scaled(">", packs_action_x(2), fs_btn_y(0), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_fs_up(), packs_action_x(0), fs_btn_y(1), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_fs_home(), packs_action_x(1), fs_btn_y(1), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_fs_packs(), packs_action_x(2), fs_btn_y(1), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_fs_system(), packs_action_x(0), fs_btn_y(2), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_fs_close(), packs_action_x(1), fs_btn_y(2), packs_action_w(), btn_h, 0.5);
    packs_msg_t = tick_down(packs_msg_t);
    if (packs_msg_t > 0 && packs_msg[0])
        ctext_scaled(packs_msg, cy + 104, RGB(184,184,200), 0.42);
}

static void touch_fs_picker(double x, double y, double action, double id) {
    double i, rows = fs_rows();
    double page_max = (fs_count > 0) ? (double)((fs_count - 1) / (int)rows) : 0;
    if (action == 3) {
        press_points_drop(id);
        return;
    }
    if (action != 0)
        return;
    i = 0;
    while (i < rows) {
        char name[128];
        int is_dir = 0;
        double ry = fs_list_y() + i * fs_row_h();
        if (fs_entry(i, name, sizeof(name), &is_dir) != 1)
            break;
        if (hit_rect(x, y, packs_card_x() + 20, ry, packs_card_w() - 40, fs_row_h() - 6) == 1) {
            play_button_click();
            if (is_dir == 1)
                fs_enter(name);
            else
                fs_install(name);
            return;
        }
        i = i + 1;
    }
    if (hit_button(x, y, packs_action_x(0), fs_btn_y(0), packs_action_w(), btn_h) == 1) {
        play_button_click();
        if (fs_page > 0)
            fs_page = fs_page - 1;
        return;
    }
    if (hit_button(x, y, packs_action_x(2), fs_btn_y(0), packs_action_w(), btn_h) == 1) {
        play_button_click();
        if (fs_page < page_max)
            fs_page = fs_page + 1;
        return;
    }
    if (hit_button(x, y, packs_action_x(1), fs_btn_y(0), packs_action_w(), btn_h) == 1) {
        play_button_click();
        fs_refresh();
        return;
    }
    if (hit_button(x, y, packs_action_x(0), fs_btn_y(1), packs_action_w(), btn_h) == 1) {
        play_button_click();
        fs_up_dir();
        return;
    }
    if (hit_button(x, y, packs_action_x(1), fs_btn_y(1), packs_action_w(), btn_h) == 1) {
        play_button_click();
        fs_home_dir();
        fs_page = 0;
        fs_refresh();
        return;
    }
    if (hit_button(x, y, packs_action_x(2), fs_btn_y(1), packs_action_w(), btn_h) == 1) {
        play_button_click();
        fs_goto_packs();
        return;
    }
    if (hit_button(x, y, packs_action_x(0), fs_btn_y(2), packs_action_w(), btn_h) == 1) {
        play_button_click();
        fs_system_dialog();
        return;
    }
    if (hit_button(x, y, packs_action_x(1), fs_btn_y(2), packs_action_w(), btn_h) == 1) {
        play_button_click();
        fs_close_picker();
    }
}
