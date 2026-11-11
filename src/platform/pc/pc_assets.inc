/* PC AAssetManager file loader and log sink. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

#include "platform/pc/pc_embedded.inc"

AAssetManager *pc_asset_manager(void) { return &pc_amgr_singleton; }

AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode) {
    (void)mgr;
    (void)mode;
    if (!filename || !*filename)
        return NULL;
    char path[1024];
    unsigned char *embedded = NULL;
    long embedded_size = 0;
    AAsset *a;
    snprintf(path, sizeof(path), "%s/%s", pc_asset_root, filename);
    FILE *f = fopen(path, "rb");
    /* Nothing on disk: serve the copy embedded in the executable, so a bare
     * CubicBattle4.exe (no assets folder) still has textures, font and music. */
    if (!f) {
        if (pc_asset_read_embedded(filename, &embedded, &embedded_size) != 1)
            return NULL;
        a = (AAsset *)calloc(1, sizeof(AAsset));
        if (!a) {
            free(embedded);
            return NULL;
        }
        a->data = embedded;
        a->len = (size_t)embedded_size;
        a->pos = 0;
        return a;
    }
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
    a = (AAsset *)calloc(1, sizeof(AAsset));
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

/* Directory listing for the texture pack exporter: the names come from the
 * asset folder when it exists and from the payload embedded in the binary
 * otherwise, so a bare .exe can still hand out its own textures. */
struct AAssetDir {
    int count;
    int next;
    char **names;
};

AAssetDir *AAssetManager_openDir(AAssetManager *mgr, const char *dirname) {
    char path[1024];
    char listing[16384];
    DIR *d;
    struct dirent *e;
    AAssetDir *dir;
    int used = 0;
    (void)mgr;
    listing[0] = '\0';
    if (dirname && *dirname)
        snprintf(path, sizeof(path), "%s/%s", pc_asset_root, dirname);
    else
        snprintf(path, sizeof(path), "%s", pc_asset_root);
    d = opendir(path);
    if (d) {
        while ((e = readdir(d)) != NULL) {
            struct stat st;
            char full[1200];
            int len = (int)strlen(e->d_name);
            if (e->d_name[0] == '.')
                continue;
            snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
            if (stat(full, &st) != 0 || !S_ISREG(st.st_mode))
                continue;
            if (used + len + 2 >= (int)sizeof(listing))
                break;
            memcpy(listing + used, e->d_name, (size_t)len);
            listing[used + len] = '\n';
            listing[used + len + 1] = '\0';
            used = used + len + 1;
        }
        closedir(d);
    }
    /* A bare .exe has no folder at all: take what it carries inside. */
    pc_embedded_dir_list(dirname, listing, (int)sizeof(listing));
    dir = (AAssetDir *)calloc(1, sizeof(AAssetDir));
    if (!dir)
        return NULL;
    dir->names = (char **)calloc(256, sizeof(char *));
    if (!dir->names) {
        free(dir);
        return NULL;
    }
    {
        char *p = listing;
        while (*p && dir->count < 256) {
            char *nl = strchr(p, '\n');
            if (nl)
                *nl = '\0';
            dir->names[dir->count++] = strdup(p);
            if (!nl)
                break;
            p = nl + 1;
        }
    }
    return dir;
}

const char *AAssetDir_getNextFileName(AAssetDir *dir) {
    if (!dir || dir->next >= dir->count)
        return NULL;
    return dir->names[dir->next++];
}

void AAssetDir_close(AAssetDir *dir) {
    int i;
    if (!dir)
        return;
    for (i = 0; i < dir->count; i++)
        free(dir->names[i]);
    free(dir->names);
    free(dir);
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
#include <pthread.h>
static FILE *pc_log_file = NULL;
static int pc_log_tried = 0;
static pthread_mutex_t pc_log_lock = PTHREAD_MUTEX_INITIALIZER;

static void pc_log_sink(const char *line, int urgent) {
    pthread_mutex_lock(&pc_log_lock);
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
        if (urgent)
            fflush(pc_log_file);
    }
    pthread_mutex_unlock(&pc_log_lock);
}

static int pc_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    const char *level = prio >= ANDROID_LOG_ERROR ? "E" : prio >= ANDROID_LOG_WARN ? "W" : "I";
    char line[1024];
    char full[1200];
    vsnprintf(line, sizeof(line), fmt, ap);
    snprintf(full, sizeof(full), "[%s/%s] %s\n", level, tag ? tag : "-", line);
#ifndef _WIN32
    fputs(full, stderr);
    fflush(stderr);
#endif
    pc_log_sink(full, prio >= ANDROID_LOG_ERROR);
    return (int)strlen(line);
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
