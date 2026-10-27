#!/usr/bin/env python3
"""Host tests for the live Firebase flags (AzumBuff, FlashSale, Christmas),
without Firebase access or real threads."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int DSMutex;
typedef int DSThread;
#define DS_MUTEX_INIT 0
#define ds_mutex_lock(m) ((void)(m))
#define ds_mutex_unlock(m) ((void)(m))
#define NET_LOGIN_OK 2
#define NET_FIREBASE_URL "https://test.invalid"
static long long clock_ms = 100;
static int thread_ok = 1, reads;
static int azum_code = 200, sale_code = 200, xmas_code = 200;
static const char *azum_response = "1", *sale_response = "0", *xmas_response = "0";
static void *(*pending[4])(void *);
static void *pending_arg[4];
static int pending_n;
static long long now_ms(void) { return clock_ms; }
static DSThread ds_thread_start(void *(*fn)(void *), void *arg) {
    if (!thread_ok) return 0;
    assert(pending_n < 4);
    pending[pending_n] = fn;
    pending_arg[pending_n] = arg;
    pending_n++;
    return 1;
}
static void ds_thread_detach(DSThread t) { (void)t; }
static int fb_http(const char *method, const char *url, const char *body, char *out, size_t cap) {
    assert(strcmp(method, "GET") == 0);
    assert(body == NULL);
    reads++;
    if (strcmp(url, NET_FIREBASE_URL "/AzumBuff.json") == 0) {
        snprintf(out, cap, "%s", azum_response);
        return azum_code;
    }
    if (strcmp(url, NET_FIREBASE_URL "/FlashSale.json") == 0) {
        snprintf(out, cap, "%s", sale_response);
        return sale_code;
    }
    assert(strcmp(url, NET_FIREBASE_URL "/Christmas.json") == 0);
    snprintf(out, cap, "%s", xmas_response);
    return xmas_code;
}
#include "engine/network/live_flags.inc"
static void complete_all(void) {
    while (pending_n) {
        void *(*fn)(void *) = pending[0];
        void *arg = pending_arg[0];
        int i;
        pending_n--;
        for (i = 0; i < pending_n; i++) {
            pending[i] = pending[i + 1];
            pending_arg[i] = pending_arg[i + 1];
        }
        fn(arg);
    }
}
static void poll_azum(const char *json, int code) {
    azum_response = json;
    azum_code = code;
    clock_ms += LIVE_FLAG_POLL_MS;
    (void)net_azum_buff();
    complete_all();
}
int main(void) {
    /* No account is involved at all: the very first call already fetches. */
    assert(net_azum_buff() == 0 && pending_n == 1);
    /* No parallel jobs and no blocking I/O on the game thread. */
    assert(net_azum_buff() == 0 && reads == 0);
    complete_all();
    assert(net_azum_buff() == 1 && reads == 1);
    poll_azum("0", 200); assert(net_azum_buff() == 0);
    poll_azum("null", 200); assert(net_azum_buff() == 0);
    poll_azum("{\"AzumBuff\":1}", 200); assert(net_azum_buff() == 0);
    poll_azum("1.1", 200); assert(net_azum_buff() == 0);
    poll_azum("1 trailing", 200); assert(net_azum_buff() == 0);
    poll_azum(" \n1.0\t", 200); assert(net_azum_buff() == 1);
    /* The console often stores these spellings; all three mean "on". */
    poll_azum("\"1\"", 200); assert(net_azum_buff() == 1);
    poll_azum("true", 200); assert(net_azum_buff() == 1);
    /* One failed read must not kill a live flag; it expires by TTL instead. */
    poll_azum("1", 403); assert(net_azum_buff() == 1);
    poll_azum("", -1); assert(net_azum_buff() == 1);
    clock_ms += LIVE_FLAG_TTL_MS;
    assert(net_azum_buff() == 0);
    complete_all();
    /* FlashSale is an independent node with the same rules. */
    assert(net_flash_sale() == 0);
    complete_all(); /* sale fetch answered "0" */
    assert(net_flash_sale() == 0);
    sale_response = "1";
    clock_ms += LIVE_FLAG_POLL_MS;
    assert(net_flash_sale() == 0);
    complete_all();
    assert(net_flash_sale() == 1);
    assert(net_azum_buff() == 0); /* AzumBuff stayed off */
    /* Entering the shop forces a fresh read: stale values stop counting
     * until the new replies land, then reflect the new state. */
    sale_response = "0";
    sale_code = 200;
    azum_response = "1";
    azum_code = 200;
    net_live_flags_refresh();
    assert(net_flash_sale() == 0 && net_azum_buff() == 0);
    complete_all();
    assert(net_flash_sale() == 0 && net_azum_buff() == 1);
    /* Christmas is an independent third node with the same rules. */
    assert(net_christmas() == 0);
    complete_all(); /* Christmas fetch answered "0" */
    assert(net_christmas() == 0);
    xmas_response = "1";
    clock_ms += LIVE_FLAG_POLL_MS;
    assert(net_christmas() == 0);
    complete_all();
    assert(net_christmas() == 1);
    xmas_response = "0";
    net_live_flags_refresh();
    assert(net_christmas() == 0);
    complete_all();
    assert(net_christmas() == 0);
    /* The flag simply follows the node; there is no account state that
     * could keep it off for one player and on for another. */
    assert(net_azum_buff() == 1);
    thread_ok = 0;
    clock_ms += LIVE_FLAG_POLL_MS;
    assert(net_azum_buff() == 0 && !pending_n);
    puts("live flags: async polling, exact values, errors, expiry, account-free, shop refresh OK");
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="cb4-flags-") as directory:
        temp = Path(directory)
        source = temp / "test.c"
        source.write_text(HARNESS)
        executable = temp / "test"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra",
            "-Werror", "-pedantic", "-I", str(ROOT / "src"),
            str(source), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
