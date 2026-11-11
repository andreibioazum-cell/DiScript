#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NET_PACKS_REL_MAX 128
#define NET_FS_DIR_MAX 256

static char net_fs_internal_dir[NET_FS_DIR_MAX];
static char net_fs_external_dir[NET_FS_DIR_MAX];

void net_fs_set_dirs(const char *internal, const char *external) {
    snprintf(net_fs_internal_dir, sizeof(net_fs_internal_dir), "%s", internal ? internal : "");
    snprintf(net_fs_external_dir, sizeof(net_fs_external_dir), "%s", external ? external : "");
}


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int packs_clean_rel(const char *rel, char *out, size_t cap) {
    size_t i = 0;
    if (!rel)
        rel = "";
    if (rel[0] == '/' || rel[0] == '\\')
        return 0;
    while (rel[i]) {
        if (rel[i] == '\\')
            return 0;
        if (rel[i] == '.' && rel[i + 1] == '.')
            return 0;
        i++;
    }
    if (i >= cap || i >= NET_PACKS_REL_MAX)
        return 0;
    memcpy(out, rel, i + 1);
    return 1;
}

int net_packs_root(char *dst, size_t cap) {
    char base[256];
    int n;
    lg_lock();
    snprintf(base, sizeof(base), "%s", lg.path);
    lg_unlock();
    if (base[0])
        n = snprintf(dst, cap, "%s\\packs", base);
    else
        n = snprintf(dst, cap, "packs");
    if (n < 0 || (size_t)n >= cap)
        return 0;
    return 1;
}

static int packs_path(const char *rel, char *dst, size_t cap, int make_root) {
    char cleaned[NET_PACKS_REL_MAX + 1];
    char root[256];
    int n;
    if (!packs_clean_rel(rel, cleaned, sizeof(cleaned)))
        return 0;
    if (!net_packs_root(root, sizeof(root)))
        return 0;
    if (make_root == 1)
        CreateDirectoryA(root, NULL);
    if (cleaned[0])
        n = snprintf(dst, cap, "%s\\%s", root, cleaned);
    else
        n = snprintf(dst, cap, "%s", root);
    if (n < 0 || (size_t)n >= cap)
        return 0;
    return 1;
}

int net_packs_mkdir(const char *rel) {
    char path[512];
    if (!packs_path(rel, path, sizeof(path), 1))
        return 0;
    return CreateDirectoryA(path, NULL) == TRUE || GetLastError() == ERROR_ALREADY_EXISTS;
}

int net_packs_exists(const char *rel) {
    char path[512];
    if (!packs_path(rel, path, sizeof(path), 0))
        return 0;
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

int net_packs_write(const char *rel, const void *data, int len) {
    char path[512];
    FILE *f;
    int wrote;
    if (len < 0 || !data)
        return 0;
    if (!packs_path(rel, path, sizeof(path), 1))
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    wrote = (int)fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return wrote == len;
}

int net_packs_read(const char *rel, void *buf, int cap) {
    char path[512];
    FILE *f;
    int got;
    if (!buf || cap <= 0)
        return -1;
    if (!packs_path(rel, path, sizeof(path), 0))
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -1;
    got = (int)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return got;
}

int net_packs_size(const char *rel) {
    char path[512];
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!packs_path(rel, path, sizeof(path), 0))
        return -1;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info))
        return -1;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return -1;
    return (int)info.nFileSizeLow;
}

int net_packs_list(const char *rel, char *out, int cap) {
    char path[512], mask[600];
    WIN32_FIND_DATAA data;
    HANDLE h;
    int used = 0, n = 0;
    if (!out || cap <= 0)
        return 0;
    out[0] = 0;
    if (!packs_path(rel, path, sizeof(path), 0))
        return 0;
    snprintf(mask, sizeof(mask), "%s\\*", path);
    h = FindFirstFileA(mask, &data);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        int len = (int)strlen(data.cFileName);
        if (data.cFileName[0] == '.')
            continue;
        if (used + len + 2 >= cap)
            break;
        memcpy(out + used, data.cFileName, (size_t)len);
        used += len;
        out[used++] = '\n';
        out[used] = 0;
        n++;
    } while (FindNextFileA(h, &data));
    FindClose(h);
    return n;
}

int net_packs_remove(const char *rel) {
    char path[512];
    if (!packs_path(rel, path, sizeof(path), 0))
        return 0;
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        return 0;
    if (RemoveDirectoryA(path))
        return 1;
    return DeleteFileA(path) == TRUE;
}

int net_fs_list(const char *dir, char *out, int cap) {
    char mask[600];
    WIN32_FIND_DATAA data;
    HANDLE h;
    int used = 0, n = 0;
    if (!dir || !out || cap <= 0)
        return 0;
    out[0] = 0;
    snprintf(mask, sizeof(mask), "%s\\*", dir);
    h = FindFirstFileA(mask, &data);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        int len = (int)strlen(data.cFileName);
        int is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (data.cFileName[0] == '.' || len <= 0)
            continue;
        if (used + len + 3 >= cap)
            break;
        memcpy(out + used, data.cFileName, (size_t)len);
        used += len;
        if (is_dir)
            out[used++] = '/';
        out[used++] = '\n';
        out[used] = 0;
        n++;
    } while (FindNextFileA(h, &data));
    FindClose(h);
    return n;
}

int net_fs_read(const char *path, void *buf, int cap) {
    FILE *f;
    int got;
    if (!path || !buf || cap <= 0)
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -1;
    got = (int)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return got;
}

int net_fs_write(const char *path, const void *data, int len) {
    FILE *f;
    int wrote;
    if (!path || !data || len < 0)
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    wrote = (int)fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return wrote == len;
}

int net_fs_dir_readable(const char *dir) {
    WIN32_FIND_DATAA data;
    HANDLE h;
    char mask[600];
    if (!dir || !dir[0])
        return 0;
    snprintf(mask, sizeof(mask), "%s\\*", dir);
    h = FindFirstFileA(mask, &data);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    FindClose(h);
    return 1;
}

int net_fs_read_fd(int fd, void *buf, int cap) {
    (void)fd;
    (void)buf;
    (void)cap;
    return -1;
}

int net_fs_write_fd(int fd, const void *data, int len) {
    (void)fd;
    (void)data;
    (void)len;
    return 0;
}

void net_fs_close_fd(int fd) { (void)fd; }

int net_fs_take_fd(int *kind) {
    if (kind)
        *kind = 0;
    return -1;
}

int net_fs_pick_import(void) { return 0; }

int net_fs_pick_export(const char *suggested) {
    (void)suggested;
    return 0;
}

int net_fs_home(char *dst, size_t cap) {
    const char *home = getenv("USERPROFILE");
    if (!dst || cap == 0)
        return 0;
    if (!home || !*home)
        return 0;
    snprintf(dst, cap, "%s\\Downloads", home);
    if (GetFileAttributesA(dst) != INVALID_FILE_ATTRIBUTES)
        return 1;
    snprintf(dst, cap, "%s", home);
    return 1;
}
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static int packs_clean_rel(const char *rel, char *out, size_t cap) {
    size_t i = 0;
    if (!rel)
        rel = "";
    if (rel[0] == '/' || rel[0] == '\\')
        return 0;
    while (rel[i]) {
        if (rel[i] == '\\')
            return 0;
        if (rel[i] == '.' && rel[i + 1] == '.')
            return 0;
        i++;
    }
    if (i >= cap || i >= NET_PACKS_REL_MAX)
        return 0;
    memcpy(out, rel, i + 1);
    return 1;
}

int net_packs_root(char *dst, size_t cap) {
    char base[256];
    int n;
    lg_lock();
    snprintf(base, sizeof(base), "%s", lg.path);
    lg_unlock();
    if (base[0])
        n = snprintf(dst, cap, "%s/packs", base);
    else
        n = snprintf(dst, cap, "packs");
    if (n < 0 || (size_t)n >= cap)
        return 0;
    return 1;
}

static int packs_path(const char *rel, char *dst, size_t cap, int make_root) {
    char cleaned[NET_PACKS_REL_MAX + 1];
    char root[256];
    int n;
    if (!packs_clean_rel(rel, cleaned, sizeof(cleaned)))
        return 0;
    if (!net_packs_root(root, sizeof(root)))
        return 0;
    if (make_root == 1)
        mkdir(root, 0777);
    if (cleaned[0])
        n = snprintf(dst, cap, "%s/%s", root, cleaned);
    else
        n = snprintf(dst, cap, "%s", root);
    if (n < 0 || (size_t)n >= cap)
        return 0;
    return 1;
}

int net_packs_mkdir(const char *rel) {
    char path[512];
    if (!packs_path(rel, path, sizeof(path), 1))
        return 0;
    if (mkdir(path, 0777) == 0)
        return 1;
    return 0;
}

int net_packs_exists(const char *rel) {
    char path[512];
    struct stat st;
    if (!packs_path(rel, path, sizeof(path), 0))
        return 0;
    return stat(path, &st) == 0;
}

int net_packs_write(const char *rel, const void *data, int len) {
    char path[512];
    FILE *f;
    int wrote;
    if (len < 0 || !data)
        return 0;
    if (!packs_path(rel, path, sizeof(path), 1))
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    wrote = (int)fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return wrote == len;
}

int net_packs_read(const char *rel, void *buf, int cap) {
    char path[512];
    FILE *f;
    int got;
    if (!buf || cap <= 0)
        return -1;
    if (!packs_path(rel, path, sizeof(path), 0))
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -1;
    got = (int)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return got;
}

int net_packs_size(const char *rel) {
    char path[512];
    struct stat st;
    if (!packs_path(rel, path, sizeof(path), 0))
        return -1;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return -1;
    return (int)st.st_size;
}

int net_packs_list(const char *rel, char *out, int cap) {
    char path[512];
    DIR *d;
    struct dirent *e;
    int used = 0, n = 0;
    if (!out || cap <= 0)
        return 0;
    out[0] = 0;
    if (!packs_path(rel, path, sizeof(path), 0))
        return 0;
    d = opendir(path);
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        int len = (int)strlen(e->d_name);
        if (e->d_name[0] == '.')
            continue;
        if (used + len + 2 >= cap)
            break;
        memcpy(out + used, e->d_name, (size_t)len);
        used += len;
        out[used++] = '\n';
        out[used] = 0;
        n++;
    }
    closedir(d);
    return n;
}

int net_packs_remove(const char *rel) {
    char path[512];
    struct stat st;
    if (!packs_path(rel, path, sizeof(path), 0))
        return 0;
    if (stat(path, &st) != 0)
        return 0;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        struct dirent *e;
        char child[NET_PACKS_REL_MAX + 1];
        if (d) {
            while ((e = readdir(d)) != NULL) {
                int n = snprintf(child, sizeof(child), "%s%s%s", rel && rel[0] ? rel : "",
                                 (rel && rel[0]) ? "/" : "", e->d_name);
                if (e->d_name[0] == '.' || n < 0 || (size_t)n >= sizeof(child))
                    continue;
                net_packs_remove(child);
            }
            closedir(d);
        }
        return rmdir(path) == 0;
    }
    return unlink(path) == 0;
}

int net_fs_list(const char *dir, char *out, int cap) {
    DIR *d;
    struct dirent *e;
    int used = 0, n = 0;
    if (!dir || !out || cap <= 0)
        return 0;
    out[0] = 0;
    d = opendir(dir);
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        char full[900];
        struct stat st;
        int len = (int)strlen(e->d_name);
        int is_dir = 0;
        if (e->d_name[0] == '.' || len <= 0)
            continue;
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
            is_dir = 1;
        if (used + len + 3 >= cap)
            break;
        memcpy(out + used, e->d_name, (size_t)len);
        used += len;
        if (is_dir)
            out[used++] = '/';
        out[used++] = '\n';
        out[used] = 0;
        n++;
    }
    closedir(d);
    return n;
}

int net_fs_read(const char *path, void *buf, int cap) {
    FILE *f;
    int got;
    if (!path || !buf || cap <= 0)
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -1;
    got = (int)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return got;
}

int net_fs_write(const char *path, const void *data, int len) {
    FILE *f;
    int wrote;
    if (!path || !data || len < 0)
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    wrote = (int)fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return wrote == len;
}

int net_fs_dir_readable(const char *dir) {
    DIR *d;
    if (!dir || !dir[0])
        return 0;
    d = opendir(dir);
    if (!d)
        return 0;
    closedir(d);
    return 1;
}

#if !defined(__ANDROID__)
int net_fs_pick_import(void) { return 0; }

int net_fs_pick_export(const char *suggested) {
    (void)suggested;
    return 0;
}

int net_fs_take_fd(int *kind) {
    if (kind)
        *kind = 0;
    return -1;
}
#endif

int net_fs_read_fd(int fd, void *buf, int cap) {
    int got = 0, n;
    if (fd < 0 || !buf || cap <= 0)
        return -1;
    while (got < cap) {
        n = (int)read(fd, (char *)buf + got, (size_t)(cap - got));
        if (n <= 0)
            break;
        got += n;
    }
    return got;
}

int net_fs_write_fd(int fd, const void *data, int len) {
    int put = 0, n;
    if (fd < 0 || !data || len < 0)
        return 0;
    while (put < len) {
        n = (int)write(fd, (const char *)data + put, (size_t)(len - put));
        if (n <= 0)
            return 0;
        put += n;
    }
    return 1;
}

void net_fs_close_fd(int fd) {
    if (fd >= 0)
        close(fd);
}

int net_fs_home(char *dst, size_t cap) {
    const char *home = getenv("HOME");
    char probe[600];
    if (!dst || cap == 0)
        return 0;
#if defined(__ANDROID__)
    /* Never scan shared storage here: on scoped storage that can stall for seconds. */
    if (net_fs_external_dir[0] && net_fs_dir_readable(net_fs_external_dir) == 1) {
        snprintf(dst, cap, "%s", net_fs_external_dir);
        return 1;
    }
    if (net_fs_internal_dir[0] && net_fs_dir_readable(net_fs_internal_dir) == 1) {
        snprintf(dst, cap, "%s", net_fs_internal_dir);
        return 1;
    }
    return 0;
#else
    if (!home || !*home)
        return 0;
    snprintf(probe, sizeof(probe), "%s/Downloads", home);
    if (access(probe, F_OK) == 0) {
        snprintf(dst, cap, "%s", probe);
        return 1;
    }
    snprintf(dst, cap, "%s", home);
    return 1;
#endif
}
#endif
