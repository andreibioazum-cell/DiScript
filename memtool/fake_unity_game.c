/*
 * fake_unity_game - a stand-in "Unity game" process for memtool tests/demos,
 * in the same spirit as test/fake_firebase.py (practise without touching
 * anything real).
 *
 * It pretends to be an IL2CPP game:
 *   - mmaps files literally named libunity.so / libil2cpp.so, so the daemon's
 *     engine and Unity-version detection see the usual module layout;
 *   - keeps a Player object on the heap in IL2CPP shape
 *     ([klass ptr][monitor][fields...]) with coins/gems/health/score/ammo
 *     and a UTF-16 nickname;
 *   - allocates thousands of decoy objects, many sharing the same coins
 *     value, so first scan finds a crowd and "next scan" has something to
 *     narrow down;
 *   - a "static field" int inside the libil2cpp.so mapping, so module+offset
 *     saved addresses can be exercised;
 *   - every tick it writes its real values + the ground-truth coins address
 *     to a state JSON file (the test reads it; humans can peek too) and,
 *     with --dec, spends 3 coins per tick so freeze/next-scan demos work.
 *
 * Usage: fake_unity_game <state.json> [--dir DIR] [--tick-ms N] [--dec]
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    void *klass;          /* IL2CPP object header: class pointer */
    void *monitor;        /* IL2CPP object header: monitor */
    int32_t coins;        /* +0x10 */
    int32_t gems;         /* +0x14 */
    float health;         /* +0x18 */
    float max_health;     /* +0x1c */
    double score;         /* +0x20 */
    int32_t ammo;         /* +0x28 */
    int32_t level;        /* +0x2c */
    uint16_t nick[24];    /* +0x30, UTF-16LE */
    float pos[3];         /* Vector3 */
} Player;

static volatile int g_stop;
static void on_sig(int s) { (void)s; g_stop = 1; }

static void set_nick(Player *p, const char *ascii)
{
    memset(p->nick, 0, sizeof p->nick);
    for (int i = 0; ascii[i] && i < 23; i++) p->nick[i] = (uint16_t)(unsigned char)ascii[i];
}

/* create file of given size filled with a marker string, mmap it */
static void *map_fake_so(const char *path, size_t size, const char *marker, int prot)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open fake .so"); exit(1); }
    if (ftruncate(fd, (off_t)size) != 0) { perror("ftruncate"); exit(1); }
    void *rw = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rw == MAP_FAILED) { perror("mmap rw"); exit(1); }
    memset(rw, 0, size);
    memcpy(rw, "\x7f""ELF", 4);
    strcpy((char *)rw + 0x100, marker);
    strcpy((char *)rw + 0x200, "2022.3.15f1"); /* fake Unity version string */
    munmap(rw, size);
    void *m = mmap(NULL, size, prot, prot & PROT_WRITE ? MAP_SHARED : MAP_PRIVATE, fd, 0);
    if (m == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);
    return m;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: fake_unity_game <state.json> [--dir D] [--tick-ms N] [--dec]\n"); return 2; }
    const char *state_path = argv[1];
    const char *dir = ".";
    int tick_ms = 1000;
    bool dec = false;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--dir") && i + 1 < argc) dir = argv[++i];
        else if (!strcmp(argv[i], "--tick-ms") && i + 1 < argc) tick_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dec")) dec = true;
    }
    signal(SIGTERM, on_sig);
    signal(SIGINT, on_sig);
    setvbuf(stdout, NULL, _IOLBF, 0);
    srand(1234);

    char pathbuf[1024];
    snprintf(pathbuf, sizeof pathbuf, "%s/libunity.so", dir);
    volatile uint8_t *libunity = map_fake_so(pathbuf, 1 << 20, "FakeUnityPlayer", PROT_READ);
    snprintf(pathbuf, sizeof pathbuf, "%s/libil2cpp.so", dir);
    volatile uint8_t *libil2cpp = map_fake_so(pathbuf, 1 << 20, "FakeIl2Cpp", PROT_READ | PROT_WRITE);
    /* "static field" at libil2cpp.so+0x8000, like an IL2CPP .bss counter */
    volatile int32_t *static_coins = (volatile int32_t *)(libil2cpp + 0x8000);
    *static_coins = 424242;

    /* decoys: a big anonymous "GC heap" full of objects */
    const int NDECOY = 20000;
    Player *heap = mmap(NULL, sizeof(Player) * (size_t)NDECOY, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heap == MAP_FAILED) { perror("mmap heap"); return 1; }
    for (int i = 0; i < NDECOY; i++) {
        heap[i].klass = (void *)libil2cpp;
        heap[i].monitor = NULL;
        heap[i].coins = (i % 7 == 0) ? 1337 : (rand() % 5000);
        heap[i].gems = rand() % 100;
        heap[i].health = 50.0f + (float)(rand() % 50);
        heap[i].max_health = 100.0f;
        heap[i].score = (double)(rand() % 100000) / 3.0;
        heap[i].ammo = rand() % 60;
    }

    Player *player = malloc(sizeof(Player));
    memset(player, 0, sizeof *player);
    player->klass = (void *)libil2cpp;
    player->coins = 1337;
    player->gems = 77;
    player->health = 95.5f;
    player->max_health = 100.0f;
    player->score = 12345.5;
    player->ammo = 30;
    player->level = 12;
    set_nick(player, "Dimasi4ek229");
    player->pos[0] = 1.5f; player->pos[1] = 64.0f; player->pos[2] = -3.25f;

    printf("[fake-game] pid=%d coins=%d at %p (static at libil2cpp+0x8000=%p)\n",
           (int)getpid(), player->coins, (void *)&player->coins, (void *)static_coins);

    char tmp_path[1200];
    snprintf(tmp_path, sizeof tmp_path, "%s.tmp", state_path);
    while (!g_stop) {
        FILE *f = fopen(tmp_path, "w");
        if (f) {
            fprintf(f,
                    "{\"pid\":%d,\"coins\":%d,\"gems\":%d,\"health\":%.3f,\"score\":%.3f,"
                    "\"ammo\":%d,\"static_coins\":%d,\"player\":\"0x%lx\",\"coins_addr\":\"0x%lx\"}\n",
                    (int)getpid(), player->coins, player->gems, player->health, player->score,
                    player->ammo, *static_coins,
                    (unsigned long)player, (unsigned long)&player->coins);
            fclose(f);
            rename(tmp_path, state_path);
        }
        printf("[fake-game] coins=%d gems=%d health=%.1f score=%.1f ammo=%d static=%d\n",
               player->coins, player->gems, player->health, player->score, player->ammo, *static_coins);
        struct timespec ts = { tick_ms / 1000, (long)(tick_ms % 1000) * 1000000L };
        nanosleep(&ts, NULL);
        if (dec && player->coins > 10) player->coins -= 3;   /* game spends coins */
        if (player->health > 1.0f) player->health -= 0.5f;   /* poison, why not */
        /* a few decoys also change, but never the 1337 crowd (except player) */
        for (int k = 0; k < 50; k++) {
            int i = rand() % NDECOY;
            if (heap[i].coins != 1337) heap[i].coins += 1;
        }
        (void)libunity;
    }
    printf("[fake-game] bye\n");
    return 0;
}
