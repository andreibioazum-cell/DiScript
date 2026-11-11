/* Command-line parser for PC window and headless harness builds. */
#ifndef PC_ARGS_H
#define PC_ARGS_H

#define PC_MAX_EVENTS 192
#define PC_EVENT_LEN 200
#define PC_PATH_LEN 1024

typedef struct {
    int frames;
    int warmup;
    int pace_ms;
    int width, height;
    int headless;
    int verbose;
    char assets[PC_PATH_LEN];
    char data[PC_PATH_LEN];
    int eventc;
    char events[PC_MAX_EVENTS][PC_EVENT_LEN];
} PcArgs;

static void pc_args_default(PcArgs *a) {
    memset(a, 0, sizeof(*a));
    a->frames = 180;
    a->warmup = 0;
    a->width = 540;
    a->height = 960;
    a->headless = 0;
    snprintf(a->assets, sizeof(a->assets), "assets");
    snprintf(a->data, sizeof(a->data), "cb4_data");
}

static void pc_args_push_event(PcArgs *a, const char *name, const char *value) {
    if (a->eventc >= PC_MAX_EVENTS)
        return;
    snprintf(a->events[a->eventc], PC_EVENT_LEN, "%s %s", name, value ? value : "");
    a->eventc++;
}

/* Tokenize command line on whitespace, respecting double quotes. */
#define PC_MAX_TOKENS 1024

static void pc_args_parse(const char *cmdline, PcArgs *a) {
    char buf[4096];
    char *p, *tok[PC_MAX_TOKENS];
    int n = 0, i;
    if (!cmdline)
        return;
    snprintf(buf, sizeof(buf), "%s", cmdline);
    p = buf;
    while (*p && n < (int)(sizeof(tok) / sizeof(tok[0]))) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (*p == '"') {
            p++;
            tok[n++] = p;
            while (*p && *p != '"')
                p++;
            if (*p == '"')
                *p++ = '\0';
        } else {
            tok[n++] = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = '\0';
        }
    }
    for (i = 1; i < n; i++) {
        const char *k = tok[i];
        const char *v = i + 1 < n ? tok[i + 1] : "";
        if (strcmp(k, "--headless") == 0) {
            a->headless = 1;
            continue;
        }
        if (strcmp(k, "--verbose") == 0) {
            a->verbose = 1;
            continue;
        }
        if (!*v)
            continue;
        if (strcmp(k, "--frames") == 0) {
            a->frames = atoi(v);
            if (a->frames < 1)
                a->frames = 1;
            i++;
        } else if (strcmp(k, "--warmup") == 0) {
            a->warmup = atoi(v);
            if (a->warmup < 0)
                a->warmup = 0;
            i++;
        } else if (strcmp(k, "--pace-ms") == 0) {
            a->pace_ms = atoi(v);
            if (a->pace_ms < 0)
                a->pace_ms = 0;
            i++;
        } else if (strcmp(k, "--size") == 0) {
            int w = 0, h = 0;
            if (sscanf(v, "%dx%d", &w, &h) == 2 && w >= 200 && h >= 200 && w <= 4096 && h <= 4096) {
                a->width = w;
                a->height = h;
            }
            i++;
        } else if (strcmp(k, "--assets") == 0) {
            snprintf(a->assets, sizeof(a->assets), "%s", v);
            i++;
        } else if (strcmp(k, "--data") == 0) {
            snprintf(a->data, sizeof(a->data), "%s", v);
            i++;
        } else if (strcmp(k, "--touch") == 0 || strcmp(k, "--goto") == 0 || strcmp(k, "--type") == 0 ||
                   strcmp(k, "--enter") == 0 || strcmp(k, "--back") == 0 || strcmp(k, "--dump") == 0 ||
                   strcmp(k, "--report") == 0 || strcmp(k, "--stick") == 0 || strcmp(k, "--atk") == 0 ||
                   strcmp(k, "--blk") == 0 || strcmp(k, "--super") == 0 || strcmp(k, "--login") == 0 ||
                   strcmp(k, "--class") == 0 || strcmp(k, "--room") == 0 || strcmp(k, "--wheel") == 0 ||
                   strcmp(k, "--logininfo") == 0) {
            pc_args_push_event(a, k + 2, v);
            i++;
        }
    }
}

#endif

/* Join argv into space-separated command string within `cap` bytes. */
static void pc_args_join(int argc, char **argv, char *dst, size_t cap) {
    size_t used = 0;
    int i;
    if (!dst || cap < 2)
        return;
    dst[0] = '\0';
    snprintf(dst, cap, "cb4_headless ");
    used = strlen(dst);
    for (i = 1; i < argc; i++) {
        size_t need = strlen(argv[i]) + 1;
        if (need + 1 > cap - used)
            break;
        memcpy(dst + used, argv[i], need - 1);
        used += need - 1;
        dst[used++] = ' ';
        dst[used] = '\0';
    }
}

/* The --gfx-test path from the argument string (gfx_test.bmp by default). */
static void pc_args_dump_path(const char *cmdline, char *dst, size_t cap) {
    const char *dump = "gfx_test.bmp";
    const char *p = cmdline ? strstr(cmdline, "--dump ") : NULL;
    size_t n;
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon)
            dump = colon + 1;
    }
    n = strcspn(dump, " \t\n");
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, dump, n);
    dst[n] = '\0';
}
