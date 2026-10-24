static void wait_ban_list(void) {
    long long deadline = now_ms() + BAN_WAIT;
    while (net.run && !ban_synced() && now_ms() < deadline) sleep_ms(50);
}

static void net_fail_sleep(int fails) {
    int ms = 300;
    for (int i = 1; i < fails && ms < 8000; i++) ms *= 2;
    if (ms > 8000)
        ms = 8000;
    sleep_ms(ms + (int)(now_ms() % 250));
}
static void mark_self_banned(const char *nick) {
    int fresh;
    lock();
    fresh = !net.self_banned;
    net.self_banned = 1;
    unlock();
    if (fresh)
        LOGERR("'%s' is banned: online closed", nick);
}

static void room_slot_delete_async(int slot) {
    char *url = (char *)malloc(FB_URL_MAX);
    DSThread t;
    if (!url || !net.base[0]) {
        free(url);
        return;
    }
    snprintf(url, FB_URL_MAX, "%s/rooms/%s/players/%d.json", net.base, net.room, slot);
    t = ds_thread_start(http_delete_job, url);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    free(url);
}
static void *stream_thread(void *arg);
static void *thread_main(void *arg) {
    unsigned gen = (unsigned)(uintptr_t)arg;
    int fails = 0, mine = -1;
    while (net_gen_ok(gen)) {
        long long start = now_ms();
        int slot;
        lock();
        slot = net.slot;
        unlock();
        if (slot < 0) {
            char nick[24];
            status(NET_CONNECTING);

            wait_ban_list();
            lock();
            snprintf(nick, sizeof(nick), "%s", net.me.nick);
            unlock();
            if (nick[0] && ban_check_cloud_sync(nick)) {
                mark_self_banned(nick);
                sleep_ms(500);
                continue;
            }
            slot = claim_slot(gen);
            if (slot < 0) {
                if (++fails > 6) {
                    status(NET_ERROR);
                    LOGERR("network error: cannot claim player slot");
                }
                net_fail_sleep(fails);
                continue;
            }
            lock();
            net.slot = slot;
            net.seq = 0;
            net.players[slot] = net.me;
            net.players[slot].online = 1;
            unlock();
            fails = 0;
            mine = slot;
            LOG("slot %d claimed", (int)net.slot);
        }
        if (!push_state()) {
            if (++fails > 6) {
                status(NET_ERROR);
                LOGERR("network error: failed to push state");
            }
            net_fail_sleep(fails);
            continue;
        }
        fails = 0;
        status(NET_PLAYING);
        long long spent = now_ms() - start;
        if (spent < WRITE_TICK)
            sleep_ms((int)(WRITE_TICK - spent));
    }

    if (net.gen == gen) {
        release_slot(mine);
        status(NET_OFFLINE);
    }
    return NULL;
}
static void *reader_thread(void *arg) {
    char resp[RESP], chat_resp[CHAT_RESP], event_resp[RESP], banner_resp[512];
    long long next_chat = 0, next_event = 0, next_ban = 0, next_prune = 0, next_banner = 0;
    unsigned gen = (unsigned)(uintptr_t)arg;
    int fails = 0;
    while (net_gen_ok(gen)) {
        long long start = now_ms();
        int slot;

        if (start >= next_ban) {
            if (ban_sync()) {
                char nick[24];
                lock();
                snprintf(nick, sizeof(nick), "%s", net.me.nick);
                unlock();
                if (nick[0] && ban_cache_has(nick)) {
                    mark_self_banned(nick);
                    net.run = 0;
                    break;
                }
            }
            next_ban = now_ms() + BAN_TICK;
        }
        lock();
        slot = net.slot;
        unlock();
        if (slot < 0) {
            sleep_ms(50);
            continue;
        }
        int pulled = stream_is_live() ? 2 : pull_state(resp, sizeof(resp));
        if (pulled == 2) {
            fails = 0; /* stream is live, or 304: state unchanged */
        } else if (pulled == 1 && net_gen_ok(gen)) {
            read_players(resp, gen);
            fails = 0;
        } else {
            if (fails < 20)
                fails++;
            net_fail_sleep(fails);
        }
        if (start >= next_chat) {
            if (pull_chat(chat_resp, sizeof(chat_resp)) && net_gen_ok(gen)) {
                parse_and_store_chat(chat_resp, gen);

                if (start >= next_prune) {
                    prune_old_chat(gen);
                    next_prune = now_ms() + CHAT_PRUNE_TICK;
                }
            }
            next_chat = now_ms() + CHAT_TICK;
        }
        if (start >= next_event) {
            if (pull_event(event_resp, sizeof(event_resp)) && net_gen_ok(gen)) {
                int ev = num_int(event_resp, "", 0);
                /* 0 off, 1 disco, 2 plates, 3 Ded Moroz, 4 cube. */
                if (ev < 0)
                    ev = 0;
                if (ev > 4)
                    ev = 4;
                lock();
                /* Preserve local event selection while server write is in flight. */
                if (now_ms() >= event_local_until_ms)
                    net.event = ev;
                unlock();
            }
            next_event = now_ms() + EVENT_TICK;
        }
        if (start >= next_banner) {
            if (pull_banner(banner_resp, sizeof(banner_resp)) && net_gen_ok(gen))
                parse_and_store_banner(banner_resp, gen);
            next_banner = now_ms() + BANNER_TICK;
        }
        long long spent = now_ms() - start;
        if (spent < READ_TICK)
            sleep_ms((int)(READ_TICK - spent));
    }
    return NULL;
}
static void make_uid(void) {
    unsigned long a, b;
    int local = 0;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    a = (unsigned long)time(NULL) ^ ((unsigned long)t.tv_nsec << 8);
    b = (unsigned long)getpid() ^ (unsigned long)(uintptr_t)&local;
    snprintf(net.uid, sizeof(net.uid), "%08lx%08lx", a & 0xfffffffful, b & 0xfffffffful);
}
void net_connect(const char *url, const char *room) {
    size_t n;
    unsigned gen;
    if (!url || !*url)
        return;
    net_fast = 0;
    if (net.started)
        net_disconnect();

    lock();
    gen = ++net.gen;
    unlock();
    memset(&net.me, 0, sizeof(net.me));
    snprintf(net.base, sizeof(net.base), "%s", url);
    n = strlen(net.base);
    while (n && net.base[n - 1] == '/') net.base[--n] = 0;
    snprintf(net.room, sizeof(net.room), "%s", (room && *room) ? room : "main");
    if (!net.uid[0])
        make_uid();
    {
        char snick[LOGIN_NICK_MAX + 1];
        lg_lock();
        if (lg.status == NET_LOGIN_OK)
            snprintf(snick, sizeof(snick), "%s", lg.session_nick);
        else
            snick[0] = 0;
        lg_unlock();
        if (snick[0])
            snprintf(net.me.nick, sizeof(net.me.nick), "%s", snick);
    }
    net.me.cls = (double)pending_cls;
    net.me.level = (double)pending_level;
    net.me.skin = (double)pending_skin;
    lock();
    net.self_banned = 0;
    unlock();

    ban_invalidate();

    state_etag[0] = 0;
    memset(claim_seen_seq, 0, sizeof(claim_seen_seq));
    memset(claim_seen_at, 0, sizeof(claim_seen_at));
    memset(claim_seen_uid, 0, sizeof(claim_seen_uid));
    memset(rp_lseq, 0, sizeof(rp_lseq));
    memset(rp_lch, 0, sizeof(rp_lch));
    memset(rp_kicked, 0, sizeof(rp_kicked));
    memset(&ps_last, 0, sizeof(ps_last));
    ps_last_put = 0;
    ps_have_last = 0;
    lock();
    memset(net.players, 0, sizeof(net.players));
    memset(net.chats, 0, sizeof(net.chats));
    net.chat_count = 0;
    net.count = 0;
    net.slot = -1;
    net.seq = 0;
    net.event = 0;
    net.banner_text[0] = 0;
    net.banner_color[0] = 0;
    net.banner_ts = 0;
    unlock();
    net.started = 1;
    net.status = NET_CONNECTING;
    net.run = 1;
    LOG("connect %s/%s", net.base, net.room);
    net.thread = ds_thread_start(thread_main, (void *)(uintptr_t)gen);
    net.rthread = ds_thread_start(reader_thread, (void *)(uintptr_t)gen);
    net.sthread = ds_thread_start(stream_thread, (void *)(uintptr_t)gen);
}
/* Maintain SSE stream on players.json and reconnect on drop. */
static void *stream_thread(void *arg) {
    unsigned gen = (unsigned)(uintptr_t)arg;
    char url[URL];
    lock();
    snprintf(url, sizeof(url), "%s/rooms/%s/players.json", net.base, net.room);
    net.stream_live_at = 0;
    unlock();
    while (net_gen_ok(gen)) {
        int slot;
        lock();
        slot = net.slot;
        unlock();
        if (slot < 0) {
            sleep_ms(50);
            continue;
        }
        stream_gen = gen;
        memset(&sse, 0, sizeof(sse));
        stream_players_cache[0] = 0;
        LOG("players stream: connecting");
        lock();
        net.stream_active = 1;
        unlock();
        int code = fb_http_stream(url, stream_players_chunk, NULL);
        lock();
        net.stream_live_at = 0;
        net.stream_active = 0;
        unlock();
        if (net_gen_ok(gen)) {
            LOG("players stream: closed (HTTP %d), polling fallback", code);
            sleep_ms(1000);
        }
    }
    return NULL;
}

void net_disconnect(void) {
    DSThread th, rth, sth;
    if (!net.started)
        return;
    net_fast = 1;
    lock();
    net.run = 0;
    net.slot = -1;
    th = net.thread;
    rth = net.rthread;
    sth = net.sthread;
    net.thread = (DSThread)0;
    net.rthread = (DSThread)0;
    net.sthread = (DSThread)0;
    net.stream_live_at = 0;
    net.stream_active = 0;
    unlock();

    ds_thread_detach(th);
    ds_thread_detach(rth);
    ds_thread_detach(sth);
    net.started = 0;
    status(NET_OFFLINE);
    net_fast = 0;
}
void net_publish(double x, double y, double angle, double hp, double alive) {
    lock();
    net.me.x = x;
    net.me.y = y;
    net.me.a = angle;
    net.me.hp = hp;
    net.me.alive = alive;
    if (net.slot >= 0) {
        net.players[net.slot] = net.me;
        net.players[net.slot].online = 1;
    }
    unlock();

