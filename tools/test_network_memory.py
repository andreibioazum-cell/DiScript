#!/usr/bin/env python3
"""Offline ASan/UBSan regression checks for native network parsers."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
from test_promo import ANDROID_LOG_H, JNI_H

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/network.c"
int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio; (void)tag; (void)fmt; return 0;
}
void ds_console_log(int error, const char *fmt, ...) { (void)error; (void)fmt; }
static void check_strings(void) {
    char *tiny = malloc(1);
    assert(tiny);
    *tiny = 'x';
    strv("\"value\"", "", tiny, 0);
    assert(*tiny == 'x');
    strv("\"value\"", "", NULL, 0);
    strv("\"value\"", "", NULL, 1);
    strv("\"value\"", "", tiny, 1);
    assert(*tiny == 0);
    free(tiny);
}
static void check_leaderboard(void) {
    LeaderboardEntry *one = malloc(sizeof(*one));
    assert(one);
    const char *two = "{\"a\":{\"nick\":\"a\",\"cups\":1},\"b\":{\"nick\":\"b\",\"cups\":2}}";
    assert(parse_leaderboard(two, one, 1) == 1);
    assert(one->cups == 2 && strcmp(one->nick, "b") == 0);
    assert(parse_leaderboard(two, NULL, 0) == 0);
    assert(parse_leaderboard(two, one, 0) == 0);
    assert(parse_leaderboard(two, one, -1) == 0);
    free(one);
}
static void check_numbers(void) {
    LeaderboardEntry out[64];
    const char *large = "{\"a\":{\"cups\":1e300}}";
    assert(parse_leaderboard(large, out, 64) == 1);
    assert(out[0].cups == 0);
    assert(num_int("{\"v\":2147483647}", "v", 7) == 2147483647);
    assert(num_int("{\"v\":-2147483648}", "v", 7) == (-2147483647 - 1));
    assert(num_int("{\"v\":1e300}", "v", 7) == 7);
    assert(num_int("{\"v\":-1e300}", "v", 7) == 7);
    assert(num_ll("{\"v\":1e300}", "v", 17) == 17);
    assert(num_ll("{\"v\":-1e300}", "v", 17) == 17);
    assert(num_ulong("{\"v\":1e300}", "v", 19) == 19);
    assert(num_ulong("{\"v\":-1}", "v", 19) == 19);
    assert(sidx(NAN) == -1 && sidx(INFINITY) == -1 && sidx(-1) == -1);
    assert(sidx(2.9) == 2);
}
static void check_profile(void) {
    net_set_data_path(".");
    apply_user_json("{\"cups\":1e300,\"candies\":-1e300,\"cls\":1e300,"
                    "\"azum\":1e300,\"santa\":1e300,\"ebuc\":1e300,"
                    "\"level\":1e300,\"levels\":1e300,\"ach_flags\":1e300}");
    pg_lock();
    assert(pg.cups == 0 && pg.candies == 0 && pg.cls == 0);
    assert(pg.azum == 0 && pg.santa == 0 && pg.ebuc == 0);
    assert(pg.level == 0 && pg.levels_unlocked == 0 && pg.loaded == 1);
    pg_unlock();
}
static void check_expiry(void) {
    long long before = now_ms();
    fb_store_tokens("id", "refresh", 1e300);
    long long after = now_ms();
    assert(strcmp(fb.id_token, "id") == 0 && strcmp(fb.refresh_token, "refresh") == 0);
    assert(fb.expires_at >= before + 3500000 && fb.expires_at <= after + 3600000);
    fb_store_tokens("", "", NAN);
    after = now_ms();
    assert(fb.expires_at <= after + 3600000);
}
static void check_gifts(void) {
    assert(gift_nick_ok("Bob_1"));
    assert(gift_nick_ok("abcdefghijklmnop"));
    assert(!gift_nick_ok("abcdefghijklmnopq"));
    assert(!gift_nick_ok("bad nick"));
    assert(!gift_nick_ok(""));
    assert(!gift_nick_ok(NULL));
    assert(gift_nick_same("Bob", "bob"));
    assert(!gift_nick_same("bob", "bobby"));
    assert(!gift_nick_same("bob", NULL));
    assert(strcmp(gift_id(0), "poop") == 0);
    assert(strcmp(gift_id(1), "cake") == 0);
    assert(strcmp(gift_id(2), "bear") == 0);
    assert(strcmp(gift_id(3), "rocket") == 0);
    assert(strcmp(gift_id(4), "emerald") == 0);
    assert(strcmp(gift_id(5), "") == 0);
    assert(strcmp(gift_id(-1), "") == 0);
    assert(gift_price(0) == 25 && gift_price(1) == 100 && gift_price(2) == 250 && gift_price(3) == 400);
    assert(gift_price(4) == 500 && gift_price(5) == 0 && gift_price(-1) == 0);
    assert(net_gift_price(0) == 25 && net_gift_price(3) == 400);
    assert(net_gift_price(4) == 500 && net_gift_price(5) == 0 && net_gift_price(-2) == 0);
    assert(net_gift_types() == 5);
    /* A local change moves only its own counter: selling a poop must not
     * touch the emerald. */
    s_gift_counts[0] = 2;
    s_gift_counts[4] = 1;
    net_gift_count_add(0, -1);
    assert(net_gift_count(0) == 1 && net_gift_count(4) == 1);
    net_gift_count_add(4, 1);
    assert(net_gift_count(0) == 1 && net_gift_count(4) == 2);
    /* A refresh must not blank the collection: the numbers stay on screen
     * until the fresh ones arrive, so no sticker looks sold by accident. */
    net_gift_refresh();
    assert(net_gift_count(0) == 1 && net_gift_count(4) == 2);
    net_gift_count_add(0, -5);
    assert(net_gift_count(0) == 0 && net_gift_count(4) == 2);
    net_gift_count_add(9, 1);
    net_gift_count_add(-1, 1);
    assert(net_gift_count(0) == 0 && net_gift_count(4) == 2);
    /* The sticker counter is written as a path ("gifts/poop"), not as a nested
     * object: a REST PATCH replaces the named subtree, so the nested form
     * wiped the other counters (the emerald with them). */
    char body[128];
    gift_body(body, sizeof(body), 137, 0, 0);
    assert(strstr(body, "\"cups\":137") != NULL);
    assert(strstr(body, "\"gifts/poop\":0") != NULL);
    assert(strstr(body, "\"gifts\":") == NULL);
    gift_body(body, sizeof(body), 387, 4, 3);
    assert(strstr(body, "\"cups\":387") != NULL);
    assert(strstr(body, "\"gifts/emerald\":3") != NULL);
    assert(strstr(body, "\"gifts/poop\"") == NULL);
}
static void check_bpx(void) {
    net_set_data_path(".");
    apply_user_json("{\"bp\":7,\"bpx\":123}");
    assert(net_load_bp_level() == 7);
    assert(net_load_bp_xp() == 123);
    net_set_bp_xp(200);
    assert(net_load_bp_xp() == 200);
    net_set_bp_xp(50); /* XP never goes down */
    assert(net_load_bp_xp() == 200);
}
static void check_progress(void) {
    /* Malformed delimiters must not trap member() in a non-advancing loop. */
    assert(member("{\"a\":]}", "missing") == NULL);
    assert(member("{\"a\":,}", "missing") == NULL);
    assert(member("{\"a\":1 \"b\":2}", "b") == NULL);
}
static void check_unescape(void) {
    char out[64];
    /* Decode JSON \uXXXX escapes into UTF-8 Cyrillic. */
    strv("{\"nick\":\"\\u0414\\u0438\\u043c\\u0430\"}", "nick", out, sizeof(out));
    assert(strcmp(out, "Дима") == 0);
    strv("{\"text\":\"\\u043f\\u0440\\u0438\\u0432\\u0435\\u0442\"}", "text", out, sizeof(out));
    assert(strcmp(out, "привет") == 0);
    /* Decode standard backslash escapes. */
    strv("{\"s\":\"a\\\"b\\\\c\\/d\\ne\\tf\"}", "s", out, sizeof(out));
    assert(strcmp(out, "a\"b\\c/d\ne\tf") == 0);
    /* Decode UTF-16 surrogate pair into 4-byte UTF-8. */
    strv("{\"s\":\"\\ud83d\\ude00\"}", "s", out, sizeof(out));
    assert(strcmp(out, "\xf0\x9f\x98\x80") == 0);
    /* Handle lone surrogates and invalid hex digits safely. */
    strv("{\"s\":\"\\ud83d!\"}", "s", out, sizeof(out));
    assert(strcmp(out, "\xef\xbf\xbd!") == 0);
    strv("{\"s\":\"\\uZZZZ\"}", "s", out, sizeof(out));
    assert(strcmp(out, "uZZZZ") == 0);
    /* Respect output buffer capacity on multi-byte characters. */
    memset(out, 'x', sizeof(out));
    strv("{\"s\":\"\\u0414\\u0414\\u0414\\u0414\"}", "s", out, sizeof(out));
    assert(strlen(out) + 1 <= sizeof(out));
}
static void check_broken_chat(void) {
    /* Skip malformed chat entries missing text field. */
    ChatMsg chats[4];
    const char *json = "{\"-1\":{\"nick\":\"Broken\"},"
                       "\"-2\":{\"uid\":\"u\",\"nick\":\"Bob\",\"text\":\"hi\"}}";
    assert(parse_chat_list(json, chats, 4) == 1);
    assert(strcmp(chats[0].nick, "Bob") == 0 && strcmp(chats[0].text, "hi") == 0);
}
static unsigned fuzz_state = 0x4c617270u;
static unsigned fuzz_next(void) {
    fuzz_state ^= fuzz_state << 13;
    fuzz_state ^= fuzz_state >> 17;
    fuzz_state ^= fuzz_state << 5;
    return fuzz_state;
}
static void check_fuzz(void) {
    char json[1024], out[32];
    ChatMsg chats[4];
    LeaderboardEntry entries[64];
    for (int pass = 0; pass < 20000; pass++) {
        size_t n = fuzz_next() % (sizeof(json) - 1);
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)(fuzz_next() & 0xff);
            json[i] = c ? (char)c : 'x';
        }
        json[n] = 0;
        (void)skip_val(json);
        (void)path_val(json, "0/uid");
        (void)num_int(json, "0/cups", 0);
        strv(json, "nick", out, sizeof(out));
        (void)parse_chat_list(json, chats, 4);
        (void)parse_leaderboard(json, entries, 64);
        read_players(json, 0);
        parse_and_store_banner(json, 0);
    }
}
static void check_truncation(void) {
    const char *samples[] = {
        "{\"0\":{\"uid\":\"test\",\"nick\":\"Azum\",\"text\":\"hello\"}}",
        "{\"a\":[1,{\"b\":\"escaped \\\" quote\"}],\"x\":true}",
        "{\"nick\":\"Русский текст\",\"cups\":123}",
        "null", "[]", "{}"
    };
    for (size_t s = 0; s < sizeof(samples)/sizeof(samples[0]); s++) {
        size_t length = strlen(samples[s]);
        for (size_t n = 0; n <= length; n++) {
            char *json = malloc(n + 1);
            assert(json);
            memcpy(json, samples[s], n);
            json[n] = 0;
            char out[8];
            ChatMsg chats[2];
            LeaderboardEntry entries[64];
            (void)skip_val(json);
            (void)num(json, "0/hp", 0);
            strv(json, "nick", out, sizeof(out));
            (void)parse_chat_list(json, chats, 2);
            (void)parse_leaderboard(json, entries, 64);
            free(json);
        }
    }
}

static void check_merge(void) {
    char dst[256];
    snprintf(dst, sizeof(dst), "%s", "{\"0\":{\"x\":1},\"1\":{\"x\":2}}");
    assert(json_object_merge(dst, sizeof(dst), "{\"0\":{\"x\":9}}"));
    assert(num(dst, "0/x", 0) == 9 && num(dst, "1/x", 0) == 2);
    assert(json_object_merge(dst, sizeof(dst), "{\"2\":{\"x\":3}}"));
    assert(num(dst, "2/x", 0) == 3 && num(dst, "0/x", 0) == 9);
    assert(json_object_merge(dst, sizeof(dst), "{\"0\":null}"));
    assert(member(dst, "0") == NULL && num(dst, "1/x", 0) == 2 && num(dst, "2/x", 0) == 3);
    assert(json_object_merge(dst, sizeof(dst), "{\"2\":null}"));
    assert(member(dst, "2") == NULL && num(dst, "1/x", 0) == 2);
    snprintf(dst, sizeof(dst), "%s", "{}");
    assert(json_object_merge(dst, sizeof(dst), "{\"5\":{\"uid\":\"u\"}}"));
    assert(member(dst, "5") != NULL);
    /* Too-small destination must fail cleanly, not corrupt. */
    char tiny[16];
    snprintf(tiny, sizeof(tiny), "%s", "{\"a\":1}");
    assert(!json_object_merge(tiny, sizeof(tiny), "{\"b\":{\"long\":\"valuevaluevalue\"}}"));
}
static void check_passwd(void) {
    assert(net_chat_is_passwd("passwd bob secret1") == 1);
    assert(net_chat_is_passwd("passwd bob") == 0);
    assert(net_chat_is_passwd("passwd") == 0);
    assert(net_chat_is_passwd("password bob secret1") == 0);
    assert(strcmp(net_chat_passwd_target("passwd bob secret1"), "bob") == 0);
    assert(strcmp(net_chat_passwd_new("passwd bob secret1"), "secret1") == 0);
    assert(strcmp(net_chat_passwd_target("passwd   alice_9   p4ss"), "alice_9") == 0);
    assert(strcmp(net_chat_passwd_new("passwd   alice_9   p4ss"), "p4ss") == 0);
}
static void check_stream(void) {
    char dst[256];
    (void)dst;
    net.run = 1;
    net.gen = 7;
    stream_gen = 7;
    net.slot = -1;
    snprintf(net.uid, sizeof(net.uid), "%s", "meuid");
    memset(&sse, 0, sizeof(sse));
    stream_players_cache[0] = 0;
    const char *put1 = "event: put\ndata: {\"path\":\"/\",\"data\":{\"0\":{\"uid\":\"u1\",\"nick\":\"Bob\",\"x\":0.5,\"y\":0.25,\"alive\":1,\"seq\":1,\"hp\":10}}}\n\n";
    /* Feed in awkward chunks, split mid-line, like TCP would. */
    assert(stream_players_chunk(NULL, put1, 7) == 1);
    assert(stream_players_chunk(NULL, put1 + 7, 31) == 1);
    assert(stream_players_chunk(NULL, put1 + 38, strlen(put1) - 38) == 1);
    assert(net.players[0].online == 1);
    assert(fabs(net.players[0].x - 0.5) < 1e-9);
    const char *patch = "event: patch\ndata: {\"path\":\"/\",\"data\":{\"0\":{\"uid\":\"u1\",\"nick\":\"Bob\",\"x\":0.9,\"y\":0.25,\"alive\":1,\"seq\":2,\"hp\":9}}}\n\n";
    assert(stream_players_chunk(NULL, patch, strlen(patch)) == 1);
    assert(fabs(net.players[0].x - 0.9) < 1e-9);
    assert(stream_players_chunk(NULL, "event: keep-alive\ndata: {}\n\n", 28) == 1);
    assert(fabs(net.players[0].x - 0.9) < 1e-9);
    const char *del = "event: put\ndata: {\"path\":\"/0\",\"data\":null}\n\n";
    assert(stream_players_chunk(NULL, del, strlen(del)) == 1);
    assert(net.players[0].online == 0);
    net.run = 0;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1], "strings")) check_strings();
    else if (!strcmp(argv[1], "leaderboard")) check_leaderboard();
    else if (!strcmp(argv[1], "numbers")) check_numbers();
    else if (!strcmp(argv[1], "profile")) check_profile();
    else if (!strcmp(argv[1], "expiry")) check_expiry();
    else if (!strcmp(argv[1], "progress")) check_progress();
    else if (!strcmp(argv[1], "truncation")) check_truncation();
    else if (!strcmp(argv[1], "fuzz")) check_fuzz();
    else if (!strcmp(argv[1], "merge")) check_merge();
    else if (!strcmp(argv[1], "stream")) check_stream();
    else if (!strcmp(argv[1], "passwd")) check_passwd();
    else if (!strcmp(argv[1], "bpx")) check_bpx();
    else if (!strcmp(argv[1], "gifts")) check_gifts();
    else if (!strcmp(argv[1], "unescape")) check_unescape();
    else if (!strcmp(argv[1], "brokenchat")) check_broken_chat();
    else return 2;
    printf("network memory: %s OK\n", argv[1]);
    return 0;
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="cb4-network-memory-") as directory:
        temp = Path(directory)
        (temp / "android").mkdir()
        (temp / "android/log.h").write_text(ANDROID_LOG_H)
        (temp / "android/asset_manager.h").write_text("typedef struct AAssetManager AAssetManager;\n")
        (temp / "android/native_window.h").write_text("typedef struct ANativeWindow ANativeWindow;\n")
        (temp / "jni.h").write_text(JNI_H)
        (temp / "test.c").write_text(HARNESS)
        binary = temp / "test"
        subprocess.run([
            *shlex.split(os.environ.get("CC", "clang")), "-std=gnu99", "-O1", "-g",
            "-fsanitize=address,undefined,float-cast-overflow", "-fno-sanitize-recover=all",
            "-fno-omit-frame-pointer", "-fno-pie", "-no-pie",
            "-D_POSIX_C_SOURCE=200809L", "-D__ANDROID__",
            "-Werror=implicit-function-declaration",
            "-I", str(temp), "-I", str(ROOT / "src"), "-I", str(ROOT),
            str(temp / "test.c"), "-lm", "-lpthread", "-o", str(binary),
        ], check=True)
        failed = False
        for name in ("strings", "leaderboard", "numbers", "profile", "expiry", "progress", "truncation",
                     "fuzz", "merge", "stream", "passwd", "bpx", "gifts", "unescape", "brokenchat"):
            try:
                result = subprocess.run([str(binary), name], cwd=temp, capture_output=True, text=True, timeout=5)
                print(result.stdout, end="")
                if result.returncode:
                    print(result.stderr[:4000])
                    failed = True
            except subprocess.TimeoutExpired:
                print(f"network memory: {name} timed out")
                failed = True
        if failed:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
