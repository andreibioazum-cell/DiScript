/* Assets appended to the executable by tools/embed_assets.py.
 *
 * The reader below opens its own binary once, reads the trailer
 * ("CB4EMBED", file count, index size, index offset) and then serves any name that is not
 * on disk. That is what makes a bare CubicBattle4.exe playable: no assets
 * folder, and the game still has its textures, font, music and effects.
 *
 * A file on disk always wins, so a loose assets folder and a texture pack keep
 * overriding the baked-in copy. */

#ifndef PC_EMBED_HEADERS
#define PC_EMBED_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#endif
#endif

#define PC_EMBED_MAGIC "CB4EMBED"
#define PC_EMBED_ENTRY 20   /* name length + data offset + data length */
#define PC_EMBED_MAX_FILES 512

typedef struct {
    char name[160];
    long offset;
    long size;
} PcEmbeddedFile;

static PcEmbeddedFile pc_embedded[PC_EMBED_MAX_FILES];
static unsigned int pc_embedded_n;
static int pc_embedded_tried;
static long pc_embedded_payload; /* where the payload starts in the file */

static unsigned int pc_embed_u32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned long long pc_embed_u64(const unsigned char *p) {
    unsigned long long v = 0;
    int i;
    for (i = 7; i >= 0; i--)
        v = (v << 8) | (unsigned long long)p[i];
    return v;
}

/* Parse the trailer and the index. Missing payload is not an error: a build
 * during development simply has nothing embedded. */
static void pc_embedded_load(void) {
    unsigned char tail[28];
    unsigned char *index;
    FILE *f;
    long long fsize;
    unsigned long long index_offset, index_size;
    unsigned int count, i;
    long pos;
    char exe[1024];
    if (pc_embedded_tried)
        return;
    pc_embedded_tried = 1;
    pc_embedded_n = 0;
#ifdef _WIN32
    {
        DWORD n = GetModuleFileNameA(NULL, exe, sizeof(exe));
        if (n == 0 || n >= sizeof(exe))
            return;
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (n <= 0)
            return;
        exe[n] = '\0';
    }
#endif
    f = fopen(exe, "rb");
    if (!f)
        return;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return;
    }
    fsize = ftell(f);
    if (fsize < (long long)sizeof(tail) || fsize > 0x7FFFFFFFLL) {
        fclose(f);
        return;
    }
    if (fseek(f, (long)(fsize - (long long)sizeof(tail)), SEEK_SET) != 0 ||
        fread(tail, 1, sizeof(tail), f) != sizeof(tail)) {
        fclose(f);
        return;
    }
    if (memcmp(tail, PC_EMBED_MAGIC, 8) != 0) {
        fclose(f);
        return;
    }
    count = pc_embed_u32(tail + 8);
    index_size = pc_embed_u64(tail + 12);
    index_offset = pc_embed_u64(tail + 20);
    if (count == 0 || count > PC_EMBED_MAX_FILES ||
        index_size == 0 || index_size > (unsigned long long)fsize) {
        fclose(f);
        return;
    }
    /* The payload starts right after the image; the index sits at
       payload + index_offset and is index_size bytes long. */
    pc_embedded_payload = (long)(fsize - (long long)sizeof(tail) - (long long)index_size -
                                 (long long)index_offset);
    if (pc_embedded_payload <= 0) {
        fclose(f);
        return;
    }
    index = (unsigned char *)malloc((size_t)index_size);
    if (!index) {
        fclose(f);
        return;
    }
    if (fseek(f, pc_embedded_payload + (long)index_offset, SEEK_SET) != 0 ||
        fread(index, 1, (size_t)index_size, f) != (size_t)index_size) {
        free(index);
        fclose(f);
        return;
    }
    fclose(f);
    pos = 0;
    for (i = 0; i < count; i++) {
        unsigned int name_len;
        unsigned long long offset, size;
        if (pos + PC_EMBED_ENTRY > (long)index_size)
            break;
        name_len = pc_embed_u32(index + pos);
        offset = pc_embed_u64(index + pos + 4);
        size = pc_embed_u64(index + pos + 12);
        pos += PC_EMBED_ENTRY;
        if (name_len == 0 || name_len >= sizeof(pc_embedded[i].name) ||
            pos + (long)name_len > (long)index_size)
            break;
        memcpy(pc_embedded[i].name, index + pos, name_len);
        pc_embedded[i].name[name_len] = '\0';
        pc_embedded[i].offset = (long)offset;
        pc_embedded[i].size = (long)size;
        pos += (long)name_len;
        pc_embedded_n = i + 1;
    }
    free(index);
}

static const PcEmbeddedFile *pc_embedded_find(const char *name) {
    unsigned int i;
    if (!name || !*name)
        return NULL;
    pc_embedded_load();
    for (i = 0; i < pc_embedded_n; i++) {
        if (strcmp(pc_embedded[i].name, name) == 0)
            return &pc_embedded[i];
    }
    return NULL;
}

int pc_assets_embedded(const char *name) { return pc_embedded_find(name) ? 1 : 0; }

/* Append the embedded names under `dir` as newline separated names, the same
 * shape a folder listing has, so both sources can share one reader. */
void pc_embedded_dir_list(const char *dir, char *out, int cap) {
    unsigned int i;
    size_t prefix = (dir && *dir) ? strlen(dir) + 1 : 0;
    if (!out || cap <= 0)
        return;
    pc_embedded_load();
    for (i = 0; i < pc_embedded_n; i++) {
        const char *name = pc_embedded[i].name;
        int used = (int)strlen(out);
        int len;
        if (prefix == 0 && strchr(name, '/'))
            continue; /* only top-level names when no folder was asked for */
        if (prefix > 0) {
            if (strncmp(name, dir, prefix - 1) != 0 || name[prefix - 1] != '/')
                continue;
            name += prefix;
            if (strchr(name, '/'))
                continue;
        }
        len = (int)strlen(name);
        if (used + len + 2 >= cap)
            return;
        memcpy(out + used, name, (size_t)len);
        out[used + len] = '\n';
        out[used + len + 1] = '\0';
    }
}

/* Read an embedded file into a fresh buffer. 1 on success. */
static int pc_asset_read_embedded(const char *name, unsigned char **out, long *size) {
    const PcEmbeddedFile *found = pc_embedded_find(name);
    unsigned char *buffer;
    FILE *f;
    char exe[1024];
    if (!found || found->size <= 0)
        return 0;
#ifdef _WIN32
    if (GetModuleFileNameA(NULL, exe, sizeof(exe)) == 0)
        return 0;
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (n <= 0)
            return 0;
        exe[n] = '\0';
    }
#endif
    buffer = (unsigned char *)malloc((size_t)found->size);
    if (!buffer)
        return 0;
    f = fopen(exe, "rb");
    if (!f) {
        free(buffer);
        return 0;
    }
    if (fseek(f, pc_embedded_payload + found->offset, SEEK_SET) != 0 ||
        fread(buffer, 1, (size_t)found->size, f) != (size_t)found->size) {
        fclose(f);
        free(buffer);
        return 0;
    }
    fclose(f);
    *out = buffer;
    *size = found->size;
    return 1;
}
