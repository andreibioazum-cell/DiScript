#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
import tempfile
import threading
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from test_settings_storage import ANDROID_LOG_H, JNI_H

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "engine/network.c"

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio; (void)tag; (void)fmt;
    return 0;
}
void ds_console_log(int is_error, const char *format, ...) { (void)is_error; (void)format; }
void ds_log_err(const char *format, ...) { (void)format; }

static const char *CLOUD_NO_EBUC =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":200,\"cls\":0,\"azum\":1,\"santa\":0,"
    "\"level\":0,\"levels\":0}";

static const char *CLOUD_FRESH =
    "{\"nick\":\"tester\",\"cups\":120,\"candies\":50,\"cls\":1,\"azum\":1,\"santa\":0,"
    "\"ebuc\":1,\"level\":0,\"levels\":0}";

static const char *CLOUD_RICH =
    "{\"nick\":\"tester\",\"cups\":5000,\"candies\":300,\"cls\":0,\"azum\":0,\"santa\":0,"
    "\"ebuc\":0,\"level\":0,\"levels\":0}";

static const char *CLOUD_NO_ASTRA =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":20,\"cls\":0,\"azum\":0,\"santa\":0,"
    "\"ebuc\":0,\"level\":0,\"levels\":0}";

static const char *CLOUD_CODECRACKER =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":20,\"cls\":6,\"azum\":0,\"santa\":0,"
    "\"ebuc\":0,\"level\":1,\"levels\":1,\"ordinary_level\":0,\"ordinary_levels\":0,"
    "\"azum_level\":0,\"azum_levels\":0,\"santa_level\":0,\"santa_levels\":0,"
    "\"ebuc_level\":0,\"ebuc_levels\":0,\"astra_level\":0,\"astra_levels\":0,"
    "\"codecracker_level\":1,\"codecracker_levels\":2}";

static const char *CLOUD_CODECRACKER_HI =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":20,\"cls\":6,\"azum\":0,\"santa\":0,"
    "\"ebuc\":0,\"codecracker\":1,\"level\":3,\"levels\":3,\"ordinary_level\":0,\"ordinary_levels\":0,"
    "\"azum_level\":0,\"azum_levels\":0,\"santa_level\":0,\"santa_levels\":0,"
    "\"ebuc_level\":0,\"ebuc_levels\":0,\"astra_level\":0,\"astra_levels\":0,"
    "\"codecracker_level\":3,\"codecracker_levels\":3}";

static const char *CLOUD_LEGACY_ASTRA =
    "{\"nick\":\"tester\",\"cups\":100,\"candies\":20,\"cls\":5,\"azum\":0,\"santa\":0,"
    "\"ebuc\":0,\"astra\":0,\"azum_skin\":1,\"level\":2,\"levels\":3,\"astra_level\":2,"
    "\"astra_levels\":3}";

static int run_first_save(const char *dir) {
    net_set_data_path(dir);

    net_save_progress_all(7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(net_load_ebuc() == 1);
    assert(net_load_azum() == 1);
    assert(net_load_cups() == 7);
    puts("first save: classes bought earlier survive a save issued before any read");
    return 0;
}

static int run_offline_buy(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_ebuc() == 0);
    net_save_progress_all(100, 50, 3, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    /* A save made while logged in could not reach the cloud. */
    cloud_dirty_set(1);
    int push = apply_user_json_keep_local(CLOUD_NO_EBUC);
    assert(push == 1);
    assert(net_load_ebuc() == 1);
    assert(net_load_class() == 3);
    assert(net_load_candies() == 50);
    assert(net_load_cups() == 100);
    puts("offline buy: буК, its selection and the spent candies come back from the device");
    return 0;
}

/* Sign-out saves a zeroed local profile without marking it cloud-dirty. */
static int run_signout(const char *dir) {
    net_set_data_path(dir);
    net_save_progress_all(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(cloud_dirty_get() == 0);
    int push = apply_user_json_keep_local(CLOUD_RICH);
    assert(push == 0);
    assert(net_load_cups() == 5000);
    assert(net_load_candies() == 300);
    puts("sign-out: a zeroed device never pushes zeros over the account");
    return 0;
}

/* Even with a leftover dirty marker, an empty local profile adopts the cloud profile. */
static int run_stale_dirty(const char *dir) {
    net_set_data_path(dir);
    net_save_progress_all(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    cloud_dirty_set(1);
    int push = apply_user_json_keep_local(CLOUD_RICH);
    assert(push == 0);
    assert(net_load_cups() == 5000);
    assert(net_load_candies() == 300);
    assert(cloud_dirty_get() == 0);
    puts("stale dirty marker: an empty device still adopts the cloud balance");
    return 0;
}

/* Offline merge never lowers cloud cup balance; shop purchases update cloud directly. */
static int run_lower_cups(const char *dir) {
    net_set_data_path(dir);
    net_save_progress_all(100, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    cloud_dirty_set(1);
    int push = apply_user_json_keep_local(CLOUD_RICH);
    assert(push == 1);
    assert(net_load_cups() == 5000);
    assert(net_load_candies() == 20);
    puts("stale balance: a local cup count can only raise the account, never lower it");
    return 0;
}

static int run_clean(const char *dir) {
    net_set_data_path(dir);
    net_save_progress_all(100, 50, 3, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    cloud_dirty_set(0);
    int push = apply_user_json_keep_local(CLOUD_FRESH);
    assert(push == 0);
    assert(net_load_class() == 1);
    assert(net_load_cups() == 120);
    assert(net_load_ebuc() == 1);
    puts("clean: with no dirty mark a fresher cloud record wins, ownership intact");
    return 0;
}

static int run_astra(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_astra() == 0);
    net_save_astra(1, 3, 3);
    net_save_progress_all(90, 20, 4, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 3);
    assert(net_load_astra_levels_unlocked() == 3);
    assert(net_load_class() == 4);
    assert(apply_user_json_keep_local(CLOUD_NO_ASTRA) == 1);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 3);
    assert(net_load_class() == 4);
    puts("astra: an offline purchase, selection and levels survive an older cloud profile");
    return 0;
}

static int run_codecracker(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_codecracker() == 0);
    assert(net_load_codecracker_level() == 0);
    assert(net_load_codecracker_levels_unlocked() == 0);
    /* CodeCracker ownership and level branch are saved on their own: the
     * ordinary cube keeps its own levels, and the selected class takes
     * CodeCracker's. */
    net_save_codecracker(1, 2, 2);
    net_save_progress_all(90, 20, 6, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(net_load_codecracker() == 1);
    assert(net_load_class() == 6);
    assert(net_load_codecracker_level() == 2);
    assert(net_load_codecracker_levels_unlocked() == 2);
    assert(net_load_ordinary_level() == 0);
    assert(net_load_ordinary_levels_unlocked() == 0);
    assert(net_load_level() == 2 && net_load_levels_unlocked() == 2);
    /* An older cloud profile without codecracker ownership or with a lower
     * branch does not wipe local ownership, selection or bought levels. */
    assert(apply_user_json_keep_local(CLOUD_CODECRACKER) == 1);
    assert(net_load_codecracker() == 1);
    assert(net_load_class() == 6);
    assert(net_load_codecracker_level() == 2);
    assert(net_load_codecracker_levels_unlocked() == 2);
    /* A higher cloud branch wins, and there is nothing to push back. */
    assert(apply_user_json_keep_local(CLOUD_CODECRACKER_HI) == 0);
    assert(net_load_codecracker() == 1);
    assert(net_load_class() == 6);
    assert(net_load_codecracker_level() == 3);
    assert(net_load_codecracker_levels_unlocked() == 3);
    puts("codecracker: ownership, selection and level branch are saved and restored");
    return 0;
}

static int run_legacy_local(const char *dir) {
    net_set_data_path(dir);
    char path[320];
    snprintf(path, sizeof(path), "%s/progress.dat", dir);
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    /* 18-field save from before per-class Astra progress existed. */
    fputs("100 5 0 0 20 2 3 0 0 0 0 0 0 0 0 0 0 0\n", f);
    fclose(f);
    assert(net_load_class() == 4);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 2);
    assert(net_load_astra_levels_unlocked() == 3);
    assert(net_load_level() == 2 && net_load_levels_unlocked() == 3);
    net_save_progress_all(100, 20, 4, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    puts("legacy local save: class ID 5 is rewritten as ordinary Astra");
    return 0;
}

static int run_legacy_profile(const char *dir) {
    net_set_data_path(dir);
    assert(apply_user_json_keep_local(CLOUD_LEGACY_ASTRA) == 0);
    assert(net_load_class() == 4);
    assert(pending_skin == 0);
    assert(net_load_astra() == 1);
    assert(net_load_astra_level() == 2);
    assert(net_load_astra_levels_unlocked() == 3);
    assert(net_load_level() == 2 && net_load_levels_unlocked() == 3);

    net_save_progress_all(80, 10, 5, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    net_save_astra(0, 1, 1);
    cloud_dirty_set(1);
    assert(apply_user_json_keep_local(CLOUD_NO_ASTRA) == 1);
    assert(net_load_class() == 4 && net_load_astra() == 1);
    puts("legacy Astra class: class ID 5 migrates to the ordinary Astra and survives a dirty local merge");
    return 0;
}

static int run_admin_ban_guard(const char *dir) {
    net_set_data_path(dir);
    net_ban_set("Dimasi4ek229", 1);
    assert(net_is_banned("Dimasi4ek229") == 0);
    char path[320];
    snprintf(path, sizeof(path), "%s/bans.dat", dir);
    FILE *f = fopen(path, "r");
    assert(f == NULL);

    /* Non-admin accounts (including Sanch and qwertyuiopaj1234) are not ban-exempt. */
    net_ban_set("Sanch", 1);
    assert(net_is_banned("Sanch") == 1);
    net_ban_set("Sanch", 0);
    assert(net_is_banned("Sanch") == 0);

    net_ban_set("qwertyuiopaj1234", 1);
    assert(net_is_banned("qwertyuiopaj1234") == 1);
    net_ban_set("qwertyuiopaj1234", 0);
    assert(net_is_banned("qwertyuiopaj1234") == 0);

    net_ban_set("ordinary_player", 1);
    assert(net_is_banned("ordinary_player") == 1);
    net_ban_set("ordinary_player", 0);
    assert(net_is_banned("ordinary_player") == 0);
    puts("moderation: Dimasi4ek229 cannot be banned; qwertyuiopaj1234 and ordinary bans work");
    return 0;
}

static int run_text_cmd(const char *dir) {
    (void)dir;
    /* Canonical quoted form: text "the banner text" [color]. */
    assert(net_chat_is_text_cmd("text \"Sale is on\" red") == 1);
    assert(strcmp(net_chat_text_cmd_text("text \"Sale is on\" red"), "Sale is on") == 0);
    assert(strcmp(net_chat_text_cmd_color("text \"Sale is on\" red"), "red") == 0);
    assert(strcmp(net_chat_text_cmd_text("text \"just text\""), "just text") == 0);
    assert(strcmp(net_chat_text_cmd_color("text \"just text\""), "white") == 0);
    /* A color word inside the quotes is text, not a color. */
    assert(strcmp(net_chat_text_cmd_text("text \"red alert\""), "red alert") == 0);
    assert(strcmp(net_chat_text_cmd_color("text \"red alert\""), "white") == 0);
    /* Legacy unquoted form still works. */
    assert(strcmp(net_chat_text_cmd_text("text hello blue"), "hello") == 0);
    assert(strcmp(net_chat_text_cmd_color("text hello blue"), "blue") == 0);
    assert(strcmp(net_chat_text_cmd_text("text hello"), "hello") == 0);
    /* Empty or unterminated quotes are not commands. */
    assert(net_chat_is_text_cmd("text \"\" red") == 0);
    assert(net_chat_is_text_cmd("text \"oops") == 0);
    assert(net_chat_is_text_cmd("text") == 0);
    puts("text cmd: quoted text, trailing color, legacy form OK");
    return 0;
}

static int run_rooms_parse(const char *dir) {
    (void)dir;
    char names[16][25];
    int n = parse_room_names("{\"main\":true,\"dima-room\":true,\"bad name!\":true,\"ok_2\":false}", names, 16);
    assert(n == 3);
    assert(strcmp(names[0], "main") == 0 && strcmp(names[1], "dima-room") == 0 && strcmp(names[2], "ok_2") == 0);
    assert(parse_room_names("null", names, 16) == 0);
    assert(room_name_valid("main") == 1 && room_name_valid("") == 0 && room_name_valid("bad name") == 0);
    char h1[16], h2[16], h3[16];
    room_pass_hash("s3cret", h1, sizeof(h1));
    room_pass_hash("s3cret", h2, sizeof(h2));
    room_pass_hash("other", h3, sizeof(h3));
    assert(strcmp(h1, h2) == 0 && strcmp(h1, h3) != 0 && strlen(h1) == 8);
    room_pass_hash("", h1, sizeof(h1));
    assert(strcmp(h1, "00000000") == 0); /* empty password = open room */
    puts("rooms: name parsing/validation and password hash OK");
    return 0;
}

static int run_reinstall_levels(const char *dir) {
    char canonical[24] = "";
    LeaderboardEntry top[10];
    char big_lb[4096];
    size_t off = 0;
    int i;
    net_set_data_path(dir);

    /* Selecting a lower unlocked level persists while keeping unlocked count. */
    net_save_astra(1, 3, 3);
    net_save_codecracker(1, 3, 3);
    net_save_progress_all(500, 80, 1, 1, 0, 1, 3, 3, 2, 2, 3, 3, 0, 0, 3, 3, 4, 2);
    net_save_progress_all(500, 80, 1, 1, 0, 1, 1, 3, 1, 2, 1, 3, 0, 0, 2, 3, 4, 2);
    net_save_astra(1, 1, 3);
    net_save_codecracker(1, 2, 3);
    assert(net_load_azum_level() == 1 && net_load_azum_levels_unlocked() == 3);
    assert(net_load_ordinary_level() == 1 && net_load_ordinary_levels_unlocked() == 2);
    assert(net_load_ebuc_level() == 2 && net_load_ebuc_levels_unlocked() == 3);
    assert(net_load_astra_level() == 1 && net_load_astra_levels_unlocked() == 3);
    assert(net_load_codecracker_level() == 2 && net_load_codecracker_levels_unlocked() == 3);

    /* Guest Ordinary levels, Battle Pass, and skin bitmask merge without dirty flag. */
    cloud_dirty_set(0);
    net_set_bp_xp(120);
    assert(apply_user_json_keep_local(
        "{\"nick\":\"tester\",\"cups\":600,\"candies\":90,\"cls\":0,\"azum\":1,\"santa\":0,"
        "\"ebuc\":0,\"level\":0,\"levels\":0,\"ordinary_level\":0,\"ordinary_levels\":0,"
        "\"bp\":1,\"bpx\":20,\"azum_skin\":1}") == 1);
    assert(net_load_ordinary_levels_unlocked() == 2);
    assert(net_load_bp_level() == 4 && net_load_bp_xp() == 120);
    assert(net_load_azum_skin() == 3);

    /* Partial cloud profile with top-level levels restores active Astra/CodeCracker branch. */
    apply_user_json(
        "{\"nick\":\"tester\",\"cups\":200,\"candies\":10,\"cls\":4,\"azum\":0,\"santa\":0,"
        "\"ebuc\":0,\"astra\":1,\"ordinary_level\":0,\"ordinary_levels\":0,\"level\":2,\"levels\":3}");
    assert(net_load_astra_level() == 2 && net_load_astra_levels_unlocked() == 3);

    /* Promo 2027 syncs from cloud and resets locally on sign-out. */
    assert(net_promo_2027_used() == 0);
    apply_user_json("{\"nick\":\"tester\",\"cups\":50,\"candies\":5,\"promo_used\":1}");
    assert(net_promo_2027_used() == 1);
    net_logout();
    assert(net_promo_2027_used() == 0);

    /* Case-variant nick resolution on reinstall. */
    assert(parse_shallow_user_nick("{\"Andrei\":true,\"Bob\":true}", "andrei", canonical, sizeof(canonical)) == 1);
    assert(strcmp(canonical, "Andrei") == 0);

    /* Leaderboard scans beyond 64 alphabetical entries. */
    off += (size_t)snprintf(big_lb + off, sizeof(big_lb) - off, "{");
    for (i = 0; i < 75; i++) {
        off += (size_t)snprintf(big_lb + off, sizeof(big_lb) - off,
                                "%s\"u%03d\":{\"nick\":\"u%03d\",\"cups\":%d}",
                                i ? "," : "", i, i, i == 74 ? 9999 : i);
    }
    snprintf(big_lb + off, sizeof(big_lb) - off, "}");
    assert(parse_leaderboard(big_lb, top, 10) == 10);
    assert(top[0].cups == 9999 && strcmp(top[0].nick, "u074") == 0);
    puts("reinstall levels: lower-level select, guest merge, promo sync, canonical nick, >64 leaderboard OK");
    return 0;
}

#ifdef CB_PC
static int run_http_guards(const char *dir, const char *base_url) {
    char url[URL], resp[RESP];
    net_set_data_path(dir);

    /* HTTP 500 or empty HTTP 200 on login GET must return NET_ERROR without PUT wipe. */
    net.base[0] = 0;
    net_set_firebase_key("");
    assert((int)net_auth(base_url, "Victim500", "pass1234") == NET_ERROR);
    assert((int)net_auth(base_url, "VictimEmpty", "pass1234") == NET_ERROR);
    assert((int)net_auth(base_url, "FreshUser", "pass1234") == NET_LOGIN_OK);
    net_logout();

    /* Dead id_token on HTTP 401 when refresh fails clears id_token and retries GET unauthenticated. */
    snprintf(net.base, sizeof(net.base), "%s", base_url);
    net_set_firebase_key("test_key");
    fb_store_tokens("dead_token", "", 3600);
    snprintf(url, sizeof(url), "%s/public_probe.json", base_url);
    assert(fb_http("GET", url, NULL, resp, sizeof(resp)) == 200);
    assert(fb.id_token[0] == 0);
    assert(num_int(resp, "ok", 0) == 1);

    /* +1000 balance stepping succeeds against strict per-write growth cap. */
    net_set_firebase_key("");
    snprintf(url, sizeof(url), "%s/users/StepUser.json", base_url);
    cloud_dirty_set(1);
    queue_progress_patch(url, "{\"cups\":2500,\"candies\":1250}");
    net_progress_flush();
    assert(cloud_dirty_get() == 0);
    assert(http("GET", url, NULL, resp, sizeof(resp)) == 200);
    assert(num_int(resp, "cups", 0) == 2500 && num_int(resp, "candies", 0) == 1250);
    puts("http guards: no PUT wipe on 500/empty, 401 dead-token GET fallback, +1000 balance stepping OK");
    return 0;
}
#endif

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: test <mode> <dir> [url]\n"); return 2; }
    if (strcmp(argv[1], "first-save") == 0) return run_first_save(argv[2]);
    if (strcmp(argv[1], "offline-buy") == 0) return run_offline_buy(argv[2]);
    if (strcmp(argv[1], "signout") == 0) return run_signout(argv[2]);
    if (strcmp(argv[1], "stale-dirty") == 0) return run_stale_dirty(argv[2]);
    if (strcmp(argv[1], "lower-cups") == 0) return run_lower_cups(argv[2]);
    if (strcmp(argv[1], "clean") == 0) return run_clean(argv[2]);
    if (strcmp(argv[1], "astra") == 0) return run_astra(argv[2]);
    if (strcmp(argv[1], "codecracker") == 0) return run_codecracker(argv[2]);
    if (strcmp(argv[1], "legacy-local") == 0) return run_legacy_local(argv[2]);
    if (strcmp(argv[1], "legacy-profile") == 0) return run_legacy_profile(argv[2]);
    if (strcmp(argv[1], "admin-ban-guard") == 0) return run_admin_ban_guard(argv[2]);
    if (strcmp(argv[1], "text-cmd") == 0) return run_text_cmd(argv[2]);
    if (strcmp(argv[1], "rooms-parse") == 0) return run_rooms_parse(argv[2]);
    if (strcmp(argv[1], "reinstall-levels") == 0) return run_reinstall_levels(argv[2]);
#ifdef CB_PC
    if (strcmp(argv[1], "http-guards") == 0 && argc >= 4) return run_http_guards(argv[2], argv[3]);
#endif
    fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
'''

def check_balance_rules():
    rules_text = (ROOT / "firebase.rules.json").read_text(encoding="utf-8")
    assert "qwertyuiopaj1234" not in rules_text.lower()
    rules = json.loads(rules_text)["rules"]
    for key in ("cups", "candies"):
        rule = rules["users"]["$nick"][key][".validate"]
        assert "data.val() + 1000" in rule, f"{key}: no per-write growth cap"
        assert "dimasi4ek229@cb4.game" in rule, \
            f"{key}: administrator cannot correct a balance"
        assert rule.count("(") == rule.count(")")


def check_class_rules():
    rules = json.loads((ROOT / "firebase.rules.json").read_text(encoding="utf-8"))["rules"]
    assert "codecracker" in rules["users"]["$nick"]
    validators = [
        rules["users"]["$nick"]["cls"][".validate"],
        rules["rooms"]["$room"]["players"]["$slot"]["cls"][".validate"],
    ]
    for rule in validators:
        assert "newData.val() >= 0" in rule and "newData.val() <= 6" in rule
        assert "newData.val() != 5" in rule
        assert "dimasi4ek229@cb4.game" not in rule
        assert rule.count("(") == rule.count(")")


def main():
    check_class_rules()
    check_balance_rules()
    with tempfile.TemporaryDirectory(prefix="cubic-cloud-") as directory:
        temp = Path(directory)
        android = temp / "android"
        android.mkdir()
        (android / "log.h").write_text(ANDROID_LOG_H)
        (android / "asset_manager.h").write_text(
            "typedef struct AAssetManager AAssetManager;\n")
        (android / "native_window.h").write_text("typedef struct ANativeWindow ANativeWindow;\n")
        (temp / "jni.h").write_text(JNI_H)
        (temp / "test.c").write_text(HARNESS)
        subprocess.run([
            *shlex.split(os.environ.get("CC", "clang")), "-std=gnu99", "-O0",
            "-D_POSIX_C_SOURCE=200809L", "-D__ANDROID__",
            "-Werror=implicit-function-declaration",
            "-I", str(temp), "-I", str(ROOT / "src"), "-I", str(ROOT),
            str(temp / "test.c"), "-lm", "-lpthread", "-o", str(temp / "test"),
        ], check=True)
        run = [str(temp / "test")]


        first = temp / "first"
        first.mkdir()
        (first / "progress.dat").write_text(
            "10 3 1 0 5 0 0 0 0 0 0 0 0 0 0 1 0 0\n", encoding="utf-8")
        subprocess.run([*run, "first-save", str(first)], check=True)
        saved = (first / "progress.dat").read_text(encoding="utf-8").split()
        assert saved[2] == "1" and saved[15] == "1", saved

        offline = temp / "offline"
        offline.mkdir()
        subprocess.run([*run, "offline-buy", str(offline)], check=True)
        assert (offline / "progress.dirty").exists()

        clean = temp / "clean"
        clean.mkdir()
        subprocess.run([*run, "clean", str(clean)], check=True)
        assert not (clean / "progress.dirty").exists()

        signout = temp / "signout"
        signout.mkdir()
        subprocess.run([*run, "signout", str(signout)], check=True)
        assert not (signout / "progress.dirty").exists()

        stale = temp / "stale"
        stale.mkdir()
        subprocess.run([*run, "stale-dirty", str(stale)], check=True)
        assert not (stale / "progress.dirty").exists()

        lower = temp / "lower"
        lower.mkdir()
        subprocess.run([*run, "lower-cups", str(lower)], check=True)

        astra = temp / "astra"
        astra.mkdir()
        subprocess.run([*run, "astra", str(astra)], check=True)
        saved = (astra / "progress.dat").read_text(encoding="utf-8").split()
        assert len(saved) == 25 and saved[18:] == ["1", "3", "3", "0", "0", "0", "0"], saved

        codecracker = temp / "codecracker"
        codecracker.mkdir()
        subprocess.run([*run, "codecracker", str(codecracker)], check=True)
        saved = (codecracker / "progress.dat").read_text(encoding="utf-8").split()
        assert len(saved) == 25 and saved[22:25] == ["3", "3", "1"], saved

        legacy_local = temp / "legacy-local"
        legacy_local.mkdir()
        subprocess.run([*run, "legacy-local", str(legacy_local)], check=True)
        saved = (legacy_local / "progress.dat").read_text(encoding="utf-8").split()
        assert saved[1] == "4" and saved[18] == "1", saved

        legacy_class = temp / "legacy-class"
        legacy_class.mkdir()
        subprocess.run([*run, "legacy-profile", str(legacy_class)], check=True)
        saved = (legacy_class / "progress.dat").read_text(encoding="utf-8").split()
        assert saved[1] == "4" and saved[18] == "1", saved

        bans = temp / "bans"
        bans.mkdir()
        subprocess.run([*run, "admin-ban-guard", str(bans)], check=True)

        textcmd = temp / "textcmd"
        textcmd.mkdir()
        subprocess.run([*run, "text-cmd", str(textcmd)], check=True)

        roomsdir = temp / "roomsdir"
        roomsdir.mkdir()
        subprocess.run([*run, "rooms-parse", str(roomsdir)], check=True)

        reinstall = temp / "reinstall"
        reinstall.mkdir()
        subprocess.run([*run, "reinstall-levels", str(reinstall)], check=True)

        put_paths = []
        step_state = {"cups": 100, "candies": 10}

        class GuardHandler(BaseHTTPRequestHandler):
            def log_message(self, fmt, *args):
                pass

            def _send(self, code, body):
                raw = body.encode("utf-8")
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(raw)))
                self.end_headers()
                self.wfile.write(raw)

            def do_GET(self):
                parsed = urlparse(self.path)
                qs = parse_qs(parsed.query)
                if parsed.path == "/users/Victim500.json":
                    self._send(500, '{"error":"internal"}')
                elif parsed.path == "/users/VictimEmpty.json":
                    self._send(200, "")
                elif parsed.path == "/users/FreshUser.json":
                    self._send(200, "null")
                elif parsed.path == "/public_probe.json":
                    if "auth" in qs:
                        self._send(401, '{"error":"Auth token is expired"}')
                    else:
                        self._send(200, '{"ok":1}')
                elif parsed.path == "/users/StepUser.json":
                    self._send(200, json.dumps(step_state))
                else:
                    self._send(404, "null")

            def do_PUT(self):
                parsed = urlparse(self.path)
                n = int(self.headers.get("Content-Length", "0"))
                _ = self.rfile.read(n) if n > 0 else b""
                put_paths.append(parsed.path)
                self._send(200, "{}")

            def do_POST(self):
                if self.headers.get("X-HTTP-Method-Override") == "PATCH":
                    self.do_PATCH()
                else:
                    n = int(self.headers.get("Content-Length", "0"))
                    _ = self.rfile.read(n) if n > 0 else b""
                    self._send(400, '{"error":"invalid_grant"}')

            def do_PATCH(self):
                parsed = urlparse(self.path)
                n = int(self.headers.get("Content-Length", "0"))
                raw = self.rfile.read(n).decode("utf-8") if n > 0 else "{}"
                data = json.loads(raw)
                if parsed.path == "/users/StepUser.json":
                    nc = data.get("cups", step_state["cups"])
                    ncd = data.get("candies", step_state["candies"])
                    if nc > step_state["cups"] + 1000 or ncd > step_state["candies"] + 1000:
                        self._send(400, '{"error":"Permission denied"}')
                        return
                    step_state["cups"] = nc
                    step_state["candies"] = ncd
                self._send(200, json.dumps(step_state))

        subprocess.run([
            *shlex.split(os.environ.get("CC", "clang")), "-std=gnu99", "-O0",
            "-D_POSIX_C_SOURCE=200809L", "-DCB_PC",
            "-Werror=implicit-function-declaration",
            "-I", str(ROOT / "src" / "platform" / "pc" / "shim"),
            "-I", str(ROOT / "src"), "-I", str(ROOT),
            str(temp / "test.c"), "-lm", "-lpthread", "-o", str(temp / "test_pc_net"),
        ], check=True)
        srv = HTTPServer(("127.0.0.1", 0), GuardHandler)
        t = threading.Thread(target=srv.serve_forever, daemon=True)
        t.start()
        try:
            http_dir = temp / "http-guards"
            http_dir.mkdir()
            base_url = f"http://127.0.0.1:{srv.server_address[1]}"
            subprocess.run([str(temp / "test_pc_net"), "http-guards", str(http_dir), base_url], check=True)
            assert "/users/Victim500.json" not in put_paths, put_paths
            assert "/users/VictimEmpty.json" not in put_paths, put_paths
            assert "/users/FreshUser.json" in put_paths, put_paths
        finally:
            srv.shutdown()
            srv.server_close()
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
