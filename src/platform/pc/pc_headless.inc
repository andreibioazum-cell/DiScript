/* Headless fixed-step harness for the editor: scripted input, state reports and
 * BMP dumps. This is what makes the desktop build testable without a window. */

typedef struct {
    int frame;
    int kind; /* scripted event code */
    float x, y;
    int action, id;
    char text[PC_EVENT_LEN];
    char path[260];
} PcEvent;

static PcEvent pc_events[PC_MAX_EVENTS];
static int pc_event_count = 0;

static void pc_event_parse(PcArgs *a) {
    int i;
    pc_event_count = 0;
    for (i = 0; i < a->eventc && pc_event_count < PC_MAX_EVENTS; i++) {
        const char *line = a->events[i];
        char name[32];
        const char *rest;
        PcEvent *e = &pc_events[pc_event_count];
        int n = 0;
        memset(e, 0, sizeof(*e));
        while (line[n] && line[n] != ' ' && n < (int)sizeof(name) - 1) {
            name[n] = line[n];
            n++;
        }
        name[n] = '\0';
        rest = line[n] == ' ' ? line + n + 1 : line + n;
        if (strcmp(name, "touch") == 0) {
            int frame, id = 1;
            float x, y;
            int action;
            int got = sscanf(rest, "%d:%f:%f:%d:%d", &frame, &x, &y, &action, &id);
            if (got < 4)
                continue;
            e->kind = 0;
            e->frame = frame;
            e->x = x;
            e->y = y;
            e->action = action;
            e->id = id;
        } else if (strcmp(name, "type") == 0) {
            const char *colon = strchr(rest, ':');
            if (!colon)
                continue;
            e->kind = 2;
            e->frame = atoi(rest);
            snprintf(e->text, sizeof(e->text), "%s", colon + 1);
        } else if (strcmp(name, "enter") == 0) {
            e->kind = 3;
            e->frame = atoi(rest);
        } else if (strcmp(name, "tab") == 0) {
            e->kind = 4;
            e->frame = atoi(rest);
        } else if (strcmp(name, "dump") == 0) {
            const char *colon = strchr(rest, ':');
            if (!colon)
                continue;
            e->kind = 5;
            e->frame = atoi(rest);
            snprintf(e->path, sizeof(e->path), "%s", colon + 1);
        } else if (strcmp(name, "report") == 0) {
            e->kind = 6;
            e->frame = atoi(rest);
        } else if (strcmp(name, "wheel") == 0) {
            if (sscanf(rest, "%d:%f", &e->frame, &e->x) != 2)
                continue;
            e->kind = 7;
        } else if (strcmp(name, "key") == 0) {
            /* key FRAME:NAME — "backspace", "left", "delete", "undo", ... */
            const char *colon = strchr(rest, ':');
            if (!colon)
                continue;
            e->kind = 8;
            e->frame = atoi(rest);
            snprintf(e->text, sizeof(e->text), "%s", colon + 1);
        } else if (strcmp(name, "open") == 0) {
            const char *colon = strchr(rest, ':');
            if (!colon)
                continue;
            e->kind = 9;
            e->frame = atoi(rest);
            snprintf(e->text, sizeof(e->text), "%s", colon + 1);
        } else if (strcmp(name, "run") == 0) {
            e->kind = 10;
            e->frame = atoi(rest);
        } else {
            continue;
        }
        pc_event_count++;
    }
}

/* One line of machine-readable state so the harness can assert on the editor
 * the same way it used to assert on the game. */
static void pc_report_state(int frame) {
    IDEApp *app = pc_ide_app();
    if (!app) {
        printf("frame %d: ready=0\n", frame);
        fflush(stdout);
        return;
    }
    IDEView *v = &app->view;
    int idx = ide_view_active(v);
    IDEBuffer *b = idx >= 0 ? &v->tabs[idx].buf : NULL;
    int lines = 0, console = v->cons_count, dirty = 0;
    const char *path = "-";
    size_t len = 0, caret = 0;
    if (b) {
        ide_lines_refresh(b);
        lines = b->line_count;
        len = b->len;
        caret = b->caret;
        path = v->tabs[idx].path[0] ? v->tabs[idx].path : "untitled.ds";
        dirty = v->tabs[idx].dirty;
    }
    printf("frame %d: ready=1 tabs=%d active=%d lines=%d len=%zu caret=%zu dirty=%d console=%d running=%d path=%s\n",
           frame, v->tab_count, v->active, lines, len, caret, dirty, console, app->running, path);
    /* Also show the buffer itself: it catches scripted-text mangling. */
    if (b && b->len) {
        char dump[512];
        size_t n = b->len < sizeof(dump) - 1 ? b->len : sizeof(dump) - 1;
        for (size_t i = 0; i < n; i++)
            dump[i] = b->text[i] == '\n' ? '|' : b->text[i];
        dump[n] = '\0';
        printf("  src: %s\n", dump);
    }
    /* Dump the console with --report so a test can assert on program output. */
    for (int i = 0; i < console; i++)
        printf("  out[%d]%s: %s\n", i, v->console[i].is_error ? " ERR" : "", v->console[i].text ? v->console[i].text : "");
    fflush(stdout);
}

static void pc_apply_event(PcEvent *e, int frame) {
    IDEApp *app = pc_ide_app();
    if (!app && e->kind != 5)
        return;
    switch (e->kind) {
    case 0:
        ide_app_pointer_sized(app, e->x, e->y, e->action == 0 ? 0 : 1, screen_w, screen_h);
        break;
    case 2:
        ide_app_text(app, e->text);
        break;
    case 3:
        ide_app_enter(app);
        break;
    case 4:
        ide_app_tab(app);
        break;
    case 5:
        if (g_frame.pixels) {
            if (!pc_write_bmp(e->path, g_frame.pixels, g_frame.width, g_frame.height, g_frame.stride))
                fprintf(stderr, "harness: cannot write %s\n", e->path);
        } else
            fprintf(stderr, "harness: no framebuffer for %s\n", e->path);
        break;
    case 6:
        pc_report_state(frame);
        break;
    case 7:
        ide_app_scroll(app, -e->x, screen_w, screen_h);
        break;
    case 8:
        ide_app_special_key(app, e->text);
        break;
    case 9:
        ide_app_open_path(app, e->text);
        break;
    case 10:
        ide_app_run(app);
        break;
    }
}

#ifndef _WIN32
/* Render standalone primitives and text to verify software rasterizer output. */
static int pc_gfx_test(const PcArgs *args, const char *bmp) {
    pc_fixed_step_ms = 16;
    pc_boot(args);
    if (!init_done)
        return 2;
    g_frame.width = screen_w;
    g_frame.height = screen_h;
    g_frame.stride = screen_w;
    g_frame.pixels = NULL;
    if (ds_graphics_begin_frame(&g_frame)) {
        rect(0, 0, (float)g_frame.width, (float)g_frame.height, 0x1A1A2Eu);
        rect(20, 20, 120, 60, 0xE94560u);
        circle(240, 90, 50, 0x00FF00u);
        ring(240, 90, 70, 6, 0xFFFF00u);
        line(20, 200, 500, 260, 5, 0x00FFFFu);
        roundrect(20, 300, 200, 80, 18, 0x3366FFu);
        text_scaled("rasterizer OK", 20, 420, 0xFFFFFFu, 1.0f);
        ds_graphics_end_frame();
    }
    if (bmp && g_frame.pixels)
        pc_write_bmp(bmp, g_frame.pixels, g_frame.width, g_frame.height, g_frame.stride);
    pc_shutdown();
    return 0;
}
#endif

static int pc_run_headless(PcArgs *args) {
    int frame;
    pc_fixed_step_ms = 16;
    pc_event_parse(args);
    pc_boot(args);
    if (!init_done) {
        fprintf(stderr, "harness: renderer did not start: %s\n", ds_graphics_failure());
        return 2;
    }
    if (args->warmup) {
        int i;
        for (i = 0; i < args->warmup; i++)
            (void)pc_step();
    }
    for (frame = 0; frame < args->frames; frame++) {
        uint64_t t0 = args->pace_ms > 0 ? monotonic_ns() : 0;
        int i;
        for (i = 0; i < pc_event_count; i++)
            if (pc_events[i].frame == frame)
                pc_apply_event(&pc_events[i], frame);
        (void)pc_step();
        if (args->pace_ms > 0) {
            uint64_t target = (uint64_t)args->pace_ms * 1000000ull;
            uint64_t used = monotonic_ns() - t0;
            if (used < target) {
                uint64_t rem = target - used;
#ifdef _WIN32
                Sleep((DWORD)(rem / 1000000ull));
#else
                struct timespec ts;
                ts.tv_sec = (time_t)(rem / 1000000000ull);
                ts.tv_nsec = (long)(rem % 1000000000ull);
                nanosleep(&ts, NULL);
#endif
            }
        }
    }
    {
        int i;
        for (i = 0; i < pc_event_count; i++) /* events parked on the last frame */
            if (pc_events[i].frame == args->frames)
                pc_apply_event(&pc_events[i], args->frames);
    }
    pc_report_state(args->frames);
    pc_shutdown();
    return 0;
}
