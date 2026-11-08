/* Headless fixed-step simulation loop, scripted input runner, and BMP dumper. */

typedef struct {
    int frame;
    int kind; /* 0..14 scripted harness event code */
    float x, y;
    int action, id;
    double state;
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
            if (got == 4)
                id = 1;
            e->kind = 0;
            e->frame = frame;
            e->x = x;
            e->y = y;
            e->action = action;
            e->id = id;
        } else if (strcmp(name, "goto") == 0) {
            int frame;
            double state;
            if (sscanf(rest, "%d:%lf", &frame, &state) != 2)
                continue;
            e->kind = 1;
            e->frame = frame;
            e->state = state;
        } else if (strcmp(name, "type") == 0) {
            int frame = 0;
            const char *colon = strchr(rest, ':');
            if (!colon)
                continue;
            frame = atoi(rest);
            e->kind = 2;
            e->frame = frame;
            snprintf(e->text, sizeof(e->text), "%s", colon + 1);
        } else if (strcmp(name, "room") == 0) {
            int frame = 0, create = 0;
            const char *colon = strchr(rest, ':');
            if (!colon)
                continue;
            frame = atoi(rest);
            e->kind = 14;
            e->frame = frame;
            n = 0;
            colon++;
            while (colon[n] && colon[n] != ':' && n < PC_EVENT_LEN - 1) {
                e->text[n] = colon[n];
                n++;
            }
            e->text[n] = '\0';
            if (colon[n] == ':')
                create = atoi(colon + n + 1);
            e->state = create;
        } else if (strcmp(name, "enter") == 0) {
            e->kind = 3;
            e->frame = atoi(rest);
        } else if (strcmp(name, "back") == 0) {
            e->kind = 4;
            e->frame = atoi(rest);
        } else if (strcmp(name, "dump") == 0) {
            int frame = 0;
            const char *colon = strchr(rest, ':');
            if (!colon)
                continue;
            frame = atoi(rest);
            e->kind = 5;
            e->frame = frame;
            snprintf(e->path, sizeof(e->path), "%s", colon + 1);
        } else if (strcmp(name, "report") == 0) {
            e->kind = 6;
            e->frame = atoi(rest);
        } else if (strcmp(name, "login") == 0) {
            /* Parse login FRAME:NICK:PASSWORD event. */
            int frame = 0;
            char nick[64];
            const char *p = rest;
            const char *colon1, *colon2;
            nick[0] = '\0';
            if (sscanf(p, "%d:", &frame) != 1)
                continue;
            colon1 = strchr(p, ':');
            if (!colon1)
                continue;
            colon2 = strchr(colon1 + 1, ':');
            if (!colon2)
                continue;
            {
                size_t n = (size_t)(colon2 - (colon1 + 1));
                if (n >= sizeof(nick))
                    n = sizeof(nick) - 1;
                memcpy(nick, colon1 + 1, n);
                nick[n] = '\0';
                snprintf(e->text, sizeof(e->text), "%s|%s", nick, colon2 + 1);
            }
            e->kind = 11;
            e->frame = frame;
        } else if (strcmp(name, "class") == 0) {
            int frame = 0;
            double cls = 0;
            if (sscanf(rest, "%d:%lf", &frame, &cls) != 2)
                continue;
            e->kind = 13;
            e->frame = frame;
            e->state = cls;
        } else if (strcmp(name, "logininfo") == 0) {
            e->kind = 12;
            e->frame = atoi(rest);
        } else if (strcmp(name, "wheel") == 0) {
            if (sscanf(rest, "%d:%f", &e->frame, &e->x) != 2)
                continue;
            e->kind = 16;
        } else if (strcmp(name, "stick") == 0) {
            if (sscanf(rest, "%d:%f:%f", &e->frame, &e->x, &e->y) != 3)
                continue;
            e->kind = 7;
        } else if (strcmp(name, "atk") == 0 || strcmp(name, "blk") == 0 || strcmp(name, "super") == 0) {
            int frame = 0, down = 0;
            if (sscanf(rest, "%d:%d", &frame, &down) != 2)
                continue;
            e->kind = name[0] == 'a' ? 8 : name[0] == 'b' ? 9 : 10;
            e->frame = frame;
            e->action = down;
        } else {
            continue;
        }
        pc_event_count++;
    }
}

static void pc_report_state(int frame) {
    double px = 0, py = 0;
    game_pc_player_xy(&px, &py);
    printf("frame %d: state=%.0f battle=%d hp=%.2f foe=%.2f finished=%.0f x=%.1f y=%.1f login=%.0f nick=%s chaos=%.0f "
           "ball=%.0f bounces=%.0f rbricks=%.0f charge=%.0f cls=%.0f block=%.0f mode=%.0f event=%.0f disco=%d hack_t=%.2f hack_ban=%.2f hack_lock=%d error=%d\n",
           frame,
           game_pc_state(), game_pc_in_battle(), game_pc_player_hp(), game_pc_enemy_hp(), game_pc_finished(), px, py,
           game_pc_login_status(), game_pc_login_nick()[0] ? game_pc_login_nick() : "-", game_pc_chaos(),
           game_pc_ball(), game_pc_bounces(), game_pc_remote_bricks(), game_pc_charge(), game_pc_class(), game_pc_block(), game_pc_ult_mode(),
           game_pc_event(), game_pc_disco_music(), game_pc_hack_time(), game_pc_hack_ban(), game_pc_hack_lock(), ds_game_has_error() ? 1 : 0);
    fflush(stdout);
}

static int pc_login_retry_at = 0;
static int pc_login_retries = 0;

/* Retry login submit tap when local test server connection transiently drops. */
static void pc_login_retry_tick(int frame) {
    float nx = 0, ny = 0, px = 0, py = 0, bx = 0, by = 0, bw = 0, bh = 0;
    if (pc_login_retries <= 0 || pc_login_retry_at == 0 || frame < pc_login_retry_at)
        return;
    if (game_pc_login_status() != 4) {
        pc_login_retry_at = 0;
        pc_login_retries = 0;
        return;
    }
    game_pc_login_fields(&nx, &ny, &px, &py, &bx, &by, &bw, &bh);
    (void)nx;
    (void)ny;
    (void)px;
    (void)py;
    pc_touch(bx + bw * 0.5f, by + bh * 0.5f, 0, 1);
    pc_touch(bx + bw * 0.5f, by + bh * 0.5f, 1, 1);
    pc_login_retries--;
    pc_login_retry_at = pc_login_retries > 0 ? frame + 30 : 0;
}

static void pc_apply_event(PcEvent *e, int frame) {
    switch (e->kind) {
    case 0:
        pc_touch(e->x, e->y, e->action, e->id);
        break;
    case 1:
        game_pc_goto(e->state);
        break;
    case 2:
        keyboard_type(e->text);
        break;
    case 3:
        pc_keyboard_enter();
        break;
    case 4:
        (void)pc_back();
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
    case 14:
        game_pc_join_room(e->text, e->state);
        break;
    case 7:
        pc_ctl_stick(e->x, e->y);
        break;
    case 8:
        pc_ctl_attack(e->action);
        break;
    case 9:
        pc_ctl_block(e->action);
        break;
    case 10:
        pc_ctl_super(e->action);
        break;
    case 13:
        game_pc_pick_class(e->state);
        break;
    case 15:
        pc_ctl_snowball(e->action);
        break;
    case 16:
        game_pc_wheel(e->x);
        break;
    case 11: {
        float nx = 0, ny = 0, px = 0, py = 0, bx = 0, by = 0, bw = 0, bh = 0;
        const char *sep;
        game_pc_login_fields(&nx, &ny, &px, &py, &bx, &by, &bw, &bh);
        sep = strchr(e->text, '|');
        if (!sep) {
            fprintf(stderr, "harness: login needs NICK:PASSWORD\n");
            break;
        }
        /* Tap consent checkbox, fill nick and password fields, and submit. */
        {
            float cx = 0, cy = 0, cs = 0;
            game_pc_login_consent(&cx, &cy, &cs);
            (void)cs;
            pc_touch(cx, cy, 0, 1);
            pc_touch(cx, cy, 1, 1);
        }
        pc_touch(nx + 40, ny + bh * 0.5f, 0, 1);
        pc_touch(nx + 40, ny + bh * 0.5f, 1, 1);
        {
            char nick[64];
            size_t n = (size_t)(sep - e->text);
            if (n >= sizeof(nick))
                n = sizeof(nick) - 1;
            memcpy(nick, e->text, n);
            nick[n] = '\0';
            keyboard_type(nick);
        }
        pc_touch(px + 40, py + bh * 0.5f, 0, 1);
        pc_touch(px + 40, py + bh * 0.5f, 1, 1);
        keyboard_type(sep + 1);
        pc_touch(bx + bw * 0.5f, by + bh * 0.5f, 0, 1);
        pc_touch(bx + bw * 0.5f, by + bh * 0.5f, 1, 1);
        pc_login_retry_at = frame + 30;
        pc_login_retries = 3;
        break;
    }
    case 12: {
        float nx = 0, ny = 0, px = 0, py = 0, bx = 0, by = 0, bw = 0, bh = 0, cx = 0, cy = 0, cs = 0;
        game_pc_login_fields(&nx, &ny, &px, &py, &bx, &by, &bw, &bh);
        game_pc_login_consent(&cx, &cy, &cs);
        printf("logininfo: nick=(%.0f,%.0f) pass=(%.0f,%.0f) consent=(%.0f,%.0f %.0f) button=(%.0f,%.0f %.0fx%.0f)\n",
               nx, ny, px, py, cx, cy, cs, bx, by, bw, bh);
        printf("logininfo: status=%.0f nick='%s'\n", game_pc_login_status(), game_pc_login_nick());
        fflush(stdout);
        break;
    }
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
        tex(260, 300, "ordinary.png", 0.0f, 2.0f);
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
    int failed = 0;
    pc_fixed_step_ms = 16;
    pc_event_parse(args);
    pc_boot(args);
    if (!init_done) {
        fprintf(stderr, "harness: renderer did not start: %s\n", ds_graphics_failure());
        return 2;
    }
    /* Advance optional warmup frames before running scripted events. */
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
        pc_login_retry_tick(frame);
        game_pc_room_tick();
        (void)pc_step();
        if (ds_game_has_error())
            failed = 1;
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
    if (failed || ds_game_has_error()) {
        fprintf(stderr, "harness: game error: %s\n", ds_runtime_error_message());
        pc_shutdown();
        return 1;
    }
    pc_shutdown();
    return 0;
}
