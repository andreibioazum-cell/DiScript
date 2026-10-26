typedef struct {
    Actor last;
    long long last_put;
    int have_last;
} PushCache;

static int net_gen_ok(unsigned gen) {
    return net.run && net.gen == gen;
}

static int push_state(PushCache *pc, unsigned gen) {
    Actor a;
    int slot, urgent;
    unsigned long seq;
    char url[URL], body[BODY * 2], enick[64], base[256], room[48], uid[24];
    lock();
    if (!net_gen_ok(gen) || net.slot < 0) {
        unlock();
        return 0;
    }
    a = net.me;
    slot = net.slot;
    urgent = net.urgent;
    if (!urgent && pc->have_last && actor_same(&a, &pc->last) && now_ms() - pc->last_put < HEARTBEAT_TICK) {
        unlock();
        return 1;
    }
    net.urgent = 0;
    seq = ++net.seq;
    snprintf(base, sizeof(base), "%s", net.base);
    snprintf(room, sizeof(room), "%s", net.room);
    snprintf(uid, sizeof(uid), "%s", net.uid);
    unlock();

    json_escape(a.nick, enick, sizeof(enick));
    snprintf(url, sizeof(url), "%s/rooms/%s/players/%d.json", base, room, slot);
    snprintf(body, sizeof(body),
             "{\"uid\":\"%s\",\"nick\":\"%s\",\"x\":%.5f,\"y\":%.5f,\"angle\":%.5f,\"hp\":%.2f,\"alive\":%.0f,\"seq\":%"
             "lu,\"px\":%.5f,\"py\":%.5f,\"pdx\":%.5f,\"pdy\":%.5f,\"punch\":%.0f,\"sx\":%.5f,\"sy\":%.5f,\"sdx\":%.5f,"
             "\"sdy\":%.5f,\"snow\":%.0f,\"stx\":%.5f,\"sty\":%.5f,\"sthp\":%.0f,\"s2x\":%.5f,\"s2y\":%.5f,\"s2hp\":%."
             "0f,\"s3x\":%.5f,\"s3y\":%.5f,\"s3hp\":%.0f,\"station\":%.0f,\"dax\":%.5f,\"day\":%.5f,\"dadx\":%.5f,"
             "\"dady\":%.5f,\"dash\":%.0f,\"gbx\":%.5f,\"gby\":%.5f,\"gbdx\":%.5f,\"gbdy\":%.5f,\"grab\":%.0f,\"uzx\":%"
             ".5f,\"uzy\":%.5f,\"universe\":%.0f,\"thud\":%.0f,\"blk\":%.0f,\"ccu\":%.0f,\"bkx\":%.5f,\"bky\":%.5f,"
             "\"bkdx\":%.5f,\"bkdy\":%.5f,\"brick\":%.0f,\"cls\":%.0f,\"level\":%.0f,\"skin\":%.0f}",
             uid, enick, clamp01(a.x), clamp01(a.y), safe(a.a), safe(a.hp), safe(a.alive), seq, clamp01(a.punch_x),
             clamp01(a.punch_y), clamp11(a.punch_dx), clamp11(a.punch_dy), safe(a.punch), clamp01(a.snow_x),
             clamp01(a.snow_y), clamp11(a.snow_dx), clamp11(a.snow_dy), safe(a.snow), clamp01(a.station_x),
             clamp01(a.station_y), safe(a.station_hp), clamp01(a.station2_x), clamp01(a.station2_y),
             safe(a.station2_hp), clamp01(a.station3_x), clamp01(a.station3_y), safe(a.station3_hp), safe(a.station),
             clamp01(a.dash_x), clamp01(a.dash_y), clamp11(a.dash_dx), clamp11(a.dash_dy), safe(a.dash),
             clamp01(a.grab_x), clamp01(a.grab_y), clamp11(a.grab_dx), clamp11(a.grab_dy), safe(a.grab),
             clamp01(a.universe_x), clamp01(a.universe_y), safe(a.universe), safe(a.thud), safe(a.block), safe(a.ccu),
             clamp01(a.brick_x), clamp01(a.brick_y), clamp11(a.brick_dx), clamp11(a.brick_dy), safe(a.brick),
             safe(a.cls), safe(a.level), safe(a.skin));
    net_seal_body(body, sizeof(body));
    int c = fb_http("PUT", url, body, NULL, 0);
    if (c != 200 && net_log_ok())
        LOGERR("push state: HTTP %d", c);
    if (c == 200) {
        pc->last = a;
        pc->last_put = now_ms();
        pc->have_last = 1;
    }
    return c == 200;
}

/* Conditional ETag GET helper: 1 = 200 body, 2 = 304 unchanged, 0 = error. */
static int pull_etag_node(const char *url, char *resp, size_t cap, char *etag, size_t etag_cap, const char *tag) {
    char fresh[96];
    int c;
    fresh[0] = 0;
    if (etag && etag[0])
        c = fb_http_ex("GET", url, NULL, resp, cap, "if-none-match", etag, fresh, sizeof(fresh));
    else
        c = fb_http_ex("GET", url, NULL, resp, cap, "X-Firebase-ETag", "true", fresh, sizeof(fresh));
    if (fresh[0] && etag && etag_cap)
        snprintf(etag, etag_cap, "%s", fresh);
    if (c == 304)
        return 2;
    if (c != 200 && tag && net_log_ok())
        LOGERR("pull %s: HTTP %d", tag, c);
    return c == 200 ? 1 : 0;
}

static int pull_state(char *resp, size_t cap, char *etag, size_t etag_cap) {
    char url[URL];
    snprintf(url, sizeof(url), "%s/rooms/%s/players.json", net.base, net.room);
    return pull_etag_node(url, resp, cap, etag, etag_cap, "state");
}

static int pull_event(char *resp, size_t cap, char *etag, size_t etag_cap) {
    char url[URL];
    snprintf(url, sizeof(url), "%s/event.json", net.base);
    return pull_etag_node(url, resp, cap, etag, etag_cap, "event");
}

static void parse_and_store_banner(const char *json, unsigned gen) {
    char text[CHAT_TEXT_MAX], color[BANNER_COLOR_MAX];
    long long ts;
    if (!json || !*json || !strncmp(json, "null", 4)) {
        lock();
        if (net_gen_ok(gen)) {
            net.banner_ts = 0;
            net.banner_text[0] = 0;
        }
        unlock();
        return;
    }
    ts = num_ll(json, "ts", 0);
    if (ts <= 0)
        return;
    strv(json, "text", text, sizeof(text));
    strv(json, "color", color, sizeof(color));
    if (!text[0])
        return;
    lock();
    if (!net_gen_ok(gen)) {
        unlock();
        return;
    }
    snprintf(net.banner_text, sizeof(net.banner_text), "%s", text);
    snprintf(net.banner_color, sizeof(net.banner_color), "%s", color[0] ? color : "white");
    net.banner_ts = ts;
    unlock();
}

static int pull_banner(char *resp, size_t cap, char *etag, size_t etag_cap) {
    char url[URL];
    snprintf(url, sizeof(url), "%s/banner.json", net.base);
    return pull_etag_node(url, resp, cap, etag, etag_cap, NULL);
}

static void release_slot_at(const char *base, const char *room, int slot) {
    char url[URL];
    if (slot < 0 || !base || !base[0] || !room || !room[0])
        return;
    snprintf(url, sizeof(url), "%s/rooms/%s/players/%d.json", base, room, slot);
    fb_http("DELETE", url, NULL, NULL, 0);
}

static int claim_slot(unsigned gen) {
    char resp[RESP], url[URL], body[BODY], uid[24], etag[96];
    long long t = now_ms();
    int slot;
    for (slot = 0; slot < NET_SLOTS; slot++) {
        unsigned long seq;
        int claim = 0, code;
        if (!net_gen_ok(gen))
            return -1;
        snprintf(url, sizeof(url), "%s/rooms/%s/players/%d.json", net.base, net.room, slot);
        code = fb_http_ex("GET", url, NULL, resp, sizeof(resp), "X-Firebase-ETag", "true", etag, sizeof(etag));
        if (code != 200) {
            if (net_log_ok())
                LOGERR("claim slot %d: HTTP %d", slot, code);
            continue;
        }
        strv(resp, "uid", uid, sizeof(uid));
        if (uid[0] && !strcmp(uid, net.uid))
            return slot;
        seq = num_ulong(resp, "seq", 0);
        if (!uid[0])
            claim = 1;
        else if (etag[0] && (strcmp(uid, claim_seen_uid[slot]) || seq != claim_seen_seq[slot])) {
            snprintf(claim_seen_uid[slot], sizeof(claim_seen_uid[slot]), "%s", uid);
            claim_seen_seq[slot] = seq;
            claim_seen_at[slot] = t;
        } else if (etag[0] && t - claim_seen_at[slot] >= STALE)
            claim = 1;
        if (!claim)
            continue;
        if (!etag[0] && net_log_ok())
            LOG("claim slot %d: no ETag", slot);
        {
            char enick[64];
            json_escape(net.me.nick, enick, sizeof(enick));
            snprintf(
                body, sizeof(body),
                "{\"uid\":\"%s\",\"nick\":\"%s\",\"x\":0,\"y\":0,\"angle\":0,\"hp\":0,\"alive\":0,\"seq\":0,\"px\":0,"
                "\"py\":0,\"pdx\":0,\"pdy\":0,\"punch\":0,\"sx\":0,\"sy\":0,\"sdx\":0,\"sdy\":0,\"snow\":0,\"stx\":0,"
                "\"sty\":0,\"sthp\":0,\"s2x\":0,\"s2y\":0,\"s2hp\":0,\"s3x\":0,\"s3y\":0,\"s3hp\":0,\"station\":0,"
                "\"dax\":0,\"day\":0,\"dadx\":0,\"dady\":0,\"dash\":0,\"gbx\":0,\"gby\":0,\"gbdx\":0,\"gbdy\":0,"
                "\"grab\":0,\"uzx\":0,\"uzy\":0,\"universe\":0,\"thud\":0,\"blk\":0,\"ccu\":0,\"bkx\":0,\"bky\":0,"
                "\"bkdx\":0,\"bkdy\":0,\"brick\":0,\"cls\":%.0f,\"level\":%.0f,\"skin\":%.0f}",
                net.uid, enick, safe(net.me.cls), safe(net.me.level), safe(net.me.skin));
        }
        net_seal_body(body, sizeof(body));
        code = fb_http_ex("PUT", url, body, NULL, 0, etag[0] ? "if-match" : NULL, etag[0] ? etag : NULL, NULL, 0);
        if (code == 200) {
            claim_seen_uid[slot][0] = 0;
            claim_seen_seq[slot] = 0;
            claim_seen_at[slot] = 0;
            LOG("slot %d uid %s", slot, net.uid);
            return slot;
        }
        if (net_log_ok())
            LOGERR("claim slot %d: PUT failed, HTTP %d", slot, code);
    }
    return -1;
}

static void parse_slot_actor(const char *sjson, Actor *a) {
    a->x = num(sjson, "x", 0);
    a->y = num(sjson, "y", 0);
    a->a = num(sjson, "angle", 0);
    a->hp = num(sjson, "hp", 0);
    a->alive = num(sjson, "alive", 0);
    a->punch_x = num(sjson, "px", 0);
    a->punch_y = num(sjson, "py", 0);
    a->punch_dx = num(sjson, "pdx", 0);
    a->punch_dy = num(sjson, "pdy", 0);
    a->punch = num(sjson, "punch", 0);
    a->snow_x = num(sjson, "sx", 0);
    a->snow_y = num(sjson, "sy", 0);
    a->snow_dx = num(sjson, "sdx", 0);
    a->snow_dy = num(sjson, "sdy", 0);
    a->snow = num(sjson, "snow", 0);
    a->station_x = num(sjson, "stx", 0);
    a->station_y = num(sjson, "sty", 0);
    a->station_hp = num(sjson, "sthp", 0);
    a->station2_x = num(sjson, "s2x", 0);
    a->station2_y = num(sjson, "s2y", 0);
    a->station2_hp = num(sjson, "s2hp", 0);
    a->station3_x = num(sjson, "s3x", 0);
    a->station3_y = num(sjson, "s3y", 0);
    a->station3_hp = num(sjson, "s3hp", 0);
    a->station = num(sjson, "station", 0);
    a->dash_x = num(sjson, "dax", 0);
    a->dash_y = num(sjson, "day", 0);
    a->dash_dx = num(sjson, "dadx", 0);
    a->dash_dy = num(sjson, "dady", 0);
    a->dash = num(sjson, "dash", 0);
    a->grab_x = num(sjson, "gbx", 0);
    a->grab_y = num(sjson, "gby", 0);
    a->grab_dx = num(sjson, "gbdx", 0);
    a->grab_dy = num(sjson, "gbdy", 0);
    a->grab = num(sjson, "grab", 0);
    a->universe_x = num(sjson, "uzx", 0);
    a->universe_y = num(sjson, "uzy", 0);
    a->universe = num(sjson, "universe", 0);
    a->thud = num(sjson, "thud", 0);
    a->block = num(sjson, "blk", 0);
    a->ccu = num(sjson, "ccu", 0);
    a->brick_x = num(sjson, "bkx", 0);
    a->brick_y = num(sjson, "bky", 0);
    a->brick_dx = num(sjson, "bkdx", 0);
    a->brick_dy = num(sjson, "bkdy", 0);
    a->brick = num(sjson, "brick", 0);
    a->cls = num(sjson, "cls", 0);
    a->level = num(sjson, "level", 0);
    a->skin = num(sjson, "skin", 0);
}

static void read_players(const char *resp, unsigned gen) {
    Actor ps[NET_SLOTS];
    char uids[NET_SLOTS][24], my_uid[24];
    unsigned long seqs[NET_SLOTS];
    int valid[NET_SLOTS], banned[NET_SLOTS], kick_now[NET_SLOTS];
    long long t = now_ms();
    int count = 0, slot;
    memset(ps, 0, sizeof(ps));
    memset(uids, 0, sizeof(uids));
    memset(seqs, 0, sizeof(seqs));
    memset(valid, 0, sizeof(valid));
    memset(banned, 0, sizeof(banned));
    memset(kick_now, 0, sizeof(kick_now));
    lock();
    snprintf(my_uid, sizeof(my_uid), "%s", net.uid);
    unlock();
    for (slot = 0; slot < NET_SLOTS; slot++) {
        char bp[16], sjson[2048];
        const char *sobj, *send;
        size_t slen;
        snprintf(bp, sizeof(bp), "%d", slot);
        sobj = path_val(resp, bp);
        sobj = skip_ws(sobj);
        if (!sobj || *sobj != '{')
            continue;
        send = skip_val(sobj);
        if (!send || send <= sobj)
            continue;
        slen = (size_t)(send - sobj);
        if (slen >= sizeof(sjson))
            continue;
        memcpy(sjson, sobj, slen);
        sjson[slen] = 0;
        strv(sjson, "uid", uids[slot], sizeof(uids[slot]));
        if (!uids[slot][0] || !strcmp(uids[slot], my_uid))
            continue;
        seqs[slot] = num_ulong(sjson, "seq", 0);
        strv(sjson, "nick", ps[slot].nick, sizeof(ps[slot].nick));
        if (ban_cache_has(ps[slot].nick)) {
            banned[slot] = 1;
            continue;
        }
        parse_slot_actor(sjson, &ps[slot]);
        valid[slot] = 1;
    }

    lock();
    if (net_gen_ok(gen)) {
        for (slot = 0; slot < NET_SLOTS; slot++) {
            if (slot == net.slot) {
                net.players[slot] = net.me;
                net.players[slot].online = 1;
                count++;
                continue;
            }
            if (banned[slot]) {
                if (t - rp_kicked[slot] >= BAN_KICK_TICK) {
                    rp_kicked[slot] = t;
                    kick_now[slot] = 1;
                }
                rp_lseq[slot] = 0;
                rp_lch[slot] = 0;
                rp_luid[slot][0] = 0;
                memset(&net.players[slot], 0, sizeof(Actor));
                continue;
            }
            if (!valid[slot]) {
                rp_lseq[slot] = 0;
                rp_lch[slot] = 0;
                rp_luid[slot][0] = 0;
                memset(&net.players[slot], 0, sizeof(Actor));
                continue;
            }
            if (strcmp(uids[slot], rp_luid[slot]) != 0) {
                snprintf(rp_luid[slot], sizeof(rp_luid[slot]), "%s", uids[slot]);
                rp_lseq[slot] = seqs[slot];
                rp_lch[slot] = t;
            } else if (seqs[slot] > rp_lseq[slot]) {
                rp_lseq[slot] = seqs[slot];
                rp_lch[slot] = t;
            } else if (seqs[slot] > 0 && seqs[slot] < rp_lseq[slot]) {
                /* Ignore out-of-order snapshot older than already applied state. */
                if (t - rp_lch[slot] < TIMEOUT && net.players[slot].online)
                    count++;
                else
                    memset(&net.players[slot], 0, sizeof(Actor));
                continue;
            } else if (!rp_lch[slot]) {
                rp_lch[slot] = t;
            }
            if (t - rp_lch[slot] < TIMEOUT) {
                ps[slot].online = 1;
                net.players[slot] = ps[slot];
                count++;
            } else {
                memset(&net.players[slot], 0, sizeof(Actor));
            }
        }
        net.count = count;
    }
    unlock();

    for (slot = 0; slot < NET_SLOTS; slot++) {
        if (kick_now[slot]) {
            LOG("slot %d: '%s' is banned, kicking", slot, ps[slot].nick);
            room_slot_delete_async(slot);
        }
    }
}
