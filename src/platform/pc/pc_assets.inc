/* PC AAssetManager file loader and log sink. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char pc_asset_root[512] = "assets";

void pc_assets_set_root(const char *dir) {
    if (!dir || !*dir)
        return;
    snprintf(pc_asset_root, sizeof(pc_asset_root), "%s", dir);
    ds_log("assets: root folder '%s'", pc_asset_root);
}

struct AAssetManager {
    int unused;
};

struct AAsset {
    uint8_t *data;
    size_t len;
    size_t pos;
};

static AAssetManager pc_amgr_singleton = {0};

AAssetManager *pc_asset_manager(void) { return &pc_amgr_singleton; }

AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode) {
    (void)mgr;
    (void)mode;
    if (!filename || !*filename)
        return NULL;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", pc_asset_root, filename);
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    AAsset *a = (AAsset *)calloc(1, sizeof(AAsset));
    if (!a) {
        fclose(f);
        return NULL;
    }
    a->data = (uint8_t *)malloc((size_t)sz > 0 ? (size_t)sz : 1);
    if (!a->data) {
        free(a);
        fclose(f);
        return NULL;
    }
    size_t got = fread(a->data, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        free(a->data);
        free(a);
        return NULL;
    }
    a->len = (size_t)sz;
    a->pos = 0;
    return a;
}

off_t AAsset_getLength(AAsset *asset) { return asset ? (off_t)asset->len : 0; }

int AAsset_read(AAsset *asset, void *buf, size_t count) {
    if (!asset || !buf)
        return -1;
    size_t left = asset->len - asset->pos;
    size_t n = count < left ? count : left;
    if (n == 0)
        return 0;
    memcpy(buf, asset->data + asset->pos, n);
    asset->pos += n;
    return (int)n;
}

int AAsset_close(AAsset *asset) {
    if (!asset)
        return -1;
    free(asset->data);
    free(asset);
    return 0;
}

/* Mirror log output to cb4.log alongside stderr. */
static FILE *pc_log_file = NULL;
static int pc_log_tried = 0;

static void pc_log_sink(const char *line) {
    if (!pc_log_tried) {
        char path[1200];
        pc_log_tried = 1;
#ifdef PC_LOG_PATH
        snprintf(path, sizeof(path), "%s", PC_LOG_PATH);
#else
        {
            char dir[1024];
            pc_exe_dir(dir, sizeof(dir));
            snprintf(path, sizeof(path), "%s/cb4.log", dir);
        }
#endif
        pc_log_file = fopen(path, "a");
    }
    if (pc_log_file) {
        fputs(line, pc_log_file);
        fflush(pc_log_file);
    }
}

static int pc_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    const char *level = prio >= ANDROID_LOG_ERROR ? "E" : prio >= ANDROID_LOG_WARN ? "W" : "I";
    char line[1024];
    int n;
    vsnprintf(line, sizeof(line), fmt, ap);
    fprintf(stderr, "[%s/%s] %s\n", level, tag ? tag : "-", line);
    fflush(stderr);
    {
        char full[1200];
        snprintf(full, sizeof(full), "[%s/%s] %s\n", level, tag ? tag : "-", line);
        pc_log_sink(full);
    }
    n = (int)strlen(line);
    return n;
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    return pc_log_vprint(prio, tag, fmt, ap);
}

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
    return n;
}
