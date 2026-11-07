void net_set_java_vm(JavaVM *vm) {
    net.vm = vm;
}

/* Persistent HttpURLConnection JNI transport and SSE stream reader. */
typedef int (*DSStreamChunkFn)(void *ctx, const char *data, size_t len);

typedef struct {
    jclass url, conn, out, in;
    jmethodID url_init, url_open;
    jmethodID c_method, c_prop, c_ctmo, c_rtmo, c_cache, c_doout, c_fixed, c_getout, c_code, c_header, c_getin,
        c_geterr;
    jmethodID o_write, o_close;
    jmethodID i_read, i_close;
    int ready;
} HttpJni;
static HttpJni http_jni;
static DSMutex http_jni_lock = DS_MUTEX_INIT;

static jclass http_global_class(JNIEnv *env, const char *name) {
    jclass local = (*env)->FindClass(env, name);
    if (!local || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return NULL;
    }
    return (jclass)(*env)->NewGlobalRef(env, local);
}

static int http_jni_resolve(JNIEnv *env) {
    int ready;
    ds_mutex_lock(http_jni_lock);
    ready = http_jni.ready;
    ds_mutex_unlock(http_jni_lock);
    if (ready)
        return 1;
    jclass url = http_global_class(env, "java/net/URL");
    jclass conn = http_global_class(env, "java/net/HttpURLConnection");
    jclass out = http_global_class(env, "java/io/OutputStream");
    jclass in = http_global_class(env, "java/io/InputStream");
    if (!url || !conn || !out || !in)
        return 0;
    HttpJni j;
    memset(&j, 0, sizeof(j));
    j.url = url;
    j.conn = conn;
    j.out = out;
    j.in = in;
    j.url_init = (*env)->GetMethodID(env, url, "<init>", "(Ljava/lang/String;)V");
    j.url_open = (*env)->GetMethodID(env, url, "openConnection", "()Ljava/net/URLConnection;");
    j.c_method = (*env)->GetMethodID(env, conn, "setRequestMethod", "(Ljava/lang/String;)V");
    j.c_prop = (*env)->GetMethodID(env, conn, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    j.c_ctmo = (*env)->GetMethodID(env, conn, "setConnectTimeout", "(I)V");
    j.c_rtmo = (*env)->GetMethodID(env, conn, "setReadTimeout", "(I)V");
    j.c_cache = (*env)->GetMethodID(env, conn, "setUseCaches", "(Z)V");
    j.c_doout = (*env)->GetMethodID(env, conn, "setDoOutput", "(Z)V");
    j.c_fixed = (*env)->GetMethodID(env, conn, "setFixedLengthStreamingMode", "(I)V");
    j.c_getout = (*env)->GetMethodID(env, conn, "getOutputStream", "()Ljava/io/OutputStream;");
    j.c_code = (*env)->GetMethodID(env, conn, "getResponseCode", "()I");
    j.c_header = (*env)->GetMethodID(env, conn, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;");
    j.c_getin = (*env)->GetMethodID(env, conn, "getInputStream", "()Ljava/io/InputStream;");
    j.c_geterr = (*env)->GetMethodID(env, conn, "getErrorStream", "()Ljava/io/InputStream;");
    j.o_write = (*env)->GetMethodID(env, out, "write", "([B)V");
    j.o_close = (*env)->GetMethodID(env, out, "close", "()V");
    j.i_read = (*env)->GetMethodID(env, in, "read", "([B)I");
    j.i_close = (*env)->GetMethodID(env, in, "close", "()V");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return 0;
    }
    if (!j.url_init || !j.url_open || !j.c_method || !j.c_prop || !j.c_getout || !j.c_code || !j.c_getin ||
        !j.o_write || !j.i_read)
        return 0;
    j.ready = 1;
    ds_mutex_lock(http_jni_lock);
    http_jni = j;
    ds_mutex_unlock(http_jni_lock);
    return 1;
}

static pthread_key_t http_jni_tls_key;
static pthread_once_t http_jni_tls_once = PTHREAD_ONCE_INIT;

static void http_jni_tls_detach(void *val) {
    if (val && net.vm)
        (*net.vm)->DetachCurrentThread(net.vm);
}

static void http_jni_tls_init(void) {
    (void)pthread_key_create(&http_jni_tls_key, http_jni_tls_detach);
}

/* Keep worker threads attached across requests; detach automatically on thread exit. */
static JNIEnv *http_env(void) {
    JNIEnv *env = NULL;
    if (!net.vm)
        return NULL;
    if ((*net.vm)->GetEnv(net.vm, (void **)&env, JNI_VERSION_1_6) == JNI_OK)
        return env;
    (void)pthread_once(&http_jni_tls_once, http_jni_tls_init);
    if ((*net.vm)->AttachCurrentThread(net.vm, &env, NULL) != JNI_OK)
        return NULL;
    (void)pthread_setspecific(http_jni_tls_key, (void *)1);
    return env;
}

static void http_log_exception(JNIEnv *env, const char *method) {
    if (!(*env)->ExceptionCheck(env))
        return;
    jthrowable ex = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    if (net_log_ok() && ex) {
        jclass tcls = (*env)->GetObjectClass(env, ex);
        jmethodID gm = tcls ? (*env)->GetMethodID(env, tcls, "getMessage", "()Ljava/lang/String;") : NULL;
        if (gm) {
            jstring jmsg = (jstring)(*env)->CallObjectMethod(env, ex, gm);
            if (jmsg && !(*env)->ExceptionCheck(env)) {
                const char *s = (*env)->GetStringUTFChars(env, jmsg, NULL);
                if (s) {
                    LOGERR("http %s: %s", method, s);
                    (*env)->ReleaseStringUTFChars(env, jmsg, s);
                }
            }
        }
    }
    (*env)->ExceptionClear(env);
}

static int http_ex_once(const char *method, const char *url, const char *body, char *out, size_t cap, const char *header,
                        const char *value, char *etag, size_t etag_cap) {
    JNIEnv *env = NULL;
    jobject conn = NULL, stream = NULL, urlobj = NULL;
    jclass urlc, connc, streamc;
    jbyteArray buf;
    jstring ju, jm;
    int code = 0, ok = 0, cached;
    size_t total = 0;
    if (out && cap)
        out[0] = '\0';
    if (etag && etag_cap)
        etag[0] = '\0';
    env = http_env();
    if (!env)
        return 0;
#define JNI_CHECK()                                                                                                    \
    do {                                                                                                               \
        if ((*env)->ExceptionCheck(env))                                                                               \
            goto done;                                                                                                 \
    } while (0)
    if ((*env)->PushLocalFrame(env, 32) != 0)
        goto done;
    cached = http_jni_resolve(env);
    urlc = cached ? http_jni.url : (*env)->FindClass(env, "java/net/URL");
    JNI_CHECK();
    connc = cached ? http_jni.conn : (*env)->FindClass(env, "java/net/HttpURLConnection");
    JNI_CHECK();
    ju = (*env)->NewStringUTF(env, url);
    JNI_CHECK();
    urlobj = (*env)->NewObject(env, urlc,
                               cached ? http_jni.url_init : (*env)->GetMethodID(env, urlc, "<init>", "(Ljava/lang/String;)V"),
                               ju);
    JNI_CHECK();
    conn = (*env)->CallObjectMethod(
        env, urlobj,
        cached ? http_jni.url_open
               : (*env)->GetMethodID(env, urlc, "openConnection", "()Ljava/net/URLConnection;"));
    JNI_CHECK();
    {
        const char *send_method = method;
        const char *override = NULL;
        if (!strcmp(method, "PATCH")) {
            send_method = "POST";
            override = "PATCH";
        }
        jm = (*env)->NewStringUTF(env, send_method);
        JNI_CHECK();
        (*env)->CallVoidMethod(env, conn,
                               cached ? http_jni.c_method
                                      : (*env)->GetMethodID(env, connc, "setRequestMethod", "(Ljava/lang/String;)V"),
                               jm);
        JNI_CHECK();
        if (override) {
            jmethodID set_override = cached ? http_jni.c_prop
                                            : (*env)->GetMethodID(env, connc, "setRequestProperty",
                                                                  "(Ljava/lang/String;Ljava/lang/String;)V");
            jstring ok = (*env)->NewStringUTF(env, "X-HTTP-Method-Override"), ov = (*env)->NewStringUTF(env, override);
            (*env)->CallVoidMethod(env, conn, set_override, ok, ov);
            JNI_CHECK();
        }
    }
    {
        int tmo = net_fast ? 3000 : TIMEOUT;
        jmethodID set_ctmo = cached ? http_jni.c_ctmo : (*env)->GetMethodID(env, connc, "setConnectTimeout", "(I)V");
        jmethodID set_rtmo = cached ? http_jni.c_rtmo : (*env)->GetMethodID(env, connc, "setReadTimeout", "(I)V");
        (*env)->CallVoidMethod(env, conn, set_ctmo, tmo);
        JNI_CHECK();
        (*env)->CallVoidMethod(env, conn, set_rtmo, tmo);
        JNI_CHECK();
    }
    (*env)->CallVoidMethod(env, conn,
                           cached ? http_jni.c_cache : (*env)->GetMethodID(env, connc, "setUseCaches", "(Z)V"),
                           JNI_FALSE);
    JNI_CHECK();
    {
        jmethodID set_header = cached ? http_jni.c_prop
                                      : (*env)->GetMethodID(env, connc, "setRequestProperty",
                                                            "(Ljava/lang/String;Ljava/lang/String;)V");
        jstring k = (*env)->NewStringUTF(env, "Content-Type"), v = (*env)->NewStringUTF(env, "application/json");
        (*env)->CallVoidMethod(env, conn, set_header, k, v);
        JNI_CHECK();
        if (header && value) {
            k = (*env)->NewStringUTF(env, header);
            v = (*env)->NewStringUTF(env, value);
            (*env)->CallVoidMethod(env, conn, set_header, k, v);
            JNI_CHECK();
        }
    }
    if (body && *body) {
        jobject os = NULL;
        jbyteArray data;
        jsize len = (jsize)strlen(body);
        (*env)->CallVoidMethod(env, conn,
                               cached ? http_jni.c_doout : (*env)->GetMethodID(env, connc, "setDoOutput", "(Z)V"),
                               JNI_TRUE);
        JNI_CHECK();
        (*env)->CallVoidMethod(env, conn,
                               cached ? http_jni.c_fixed
                                      : (*env)->GetMethodID(env, connc, "setFixedLengthStreamingMode", "(I)V"),
                               len);
        JNI_CHECK();
        os = (*env)->CallObjectMethod(
            env, conn,
            cached ? http_jni.c_getout : (*env)->GetMethodID(env, connc, "getOutputStream", "()Ljava/io/OutputStream;"));
        JNI_CHECK();
        if (!os)
            goto done;
        data = (*env)->NewByteArray(env, len);
        JNI_CHECK();
        (*env)->SetByteArrayRegion(env, data, 0, len, (const jbyte *)body);
        {
            jclass osc = cached ? http_jni.out : (*env)->GetObjectClass(env, os);
            jmethodID wr = cached ? http_jni.o_write : (*env)->GetMethodID(env, osc, "write", "([B)V");
            jmethodID cl = cached ? http_jni.o_close : (*env)->GetMethodID(env, osc, "close", "()V");
            (*env)->CallVoidMethod(env, os, wr, data);
            JNI_CHECK();
            (*env)->CallVoidMethod(env, os, cl);
            JNI_CHECK();
        }
    }
    code = (int)(*env)->CallIntMethod(
        env, conn, cached ? http_jni.c_code : (*env)->GetMethodID(env, connc, "getResponseCode", "()I"));
    JNI_CHECK();
    if (etag && etag_cap) {
        jstring key = (*env)->NewStringUTF(env, "ETag");
        JNI_CHECK();
        jstring val = (jstring)(*env)->CallObjectMethod(
            env, conn,
            cached ? http_jni.c_header
                   : (*env)->GetMethodID(env, connc, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;"),
            key);
        JNI_CHECK();
        if (val) {
            const char *s = (*env)->GetStringUTFChars(env, val, NULL);
            if (s) {
                snprintf(etag, etag_cap, "%s", s);
                (*env)->ReleaseStringUTFChars(env, val, s);
            }
        }
    }
    stream = (*env)->CallObjectMethod(
        env, conn,
        code >= 400 ? (cached ? http_jni.c_geterr : (*env)->GetMethodID(env, connc, "getErrorStream", "()Ljava/io/InputStream;"))
                    : (cached ? http_jni.c_getin : (*env)->GetMethodID(env, connc, "getInputStream", "()Ljava/io/InputStream;")));
    JNI_CHECK();
    if (!stream)
        goto closeconn;
    streamc = cached ? http_jni.in : (*env)->GetObjectClass(env, stream);
    JNI_CHECK();
    buf = (*env)->NewByteArray(env, 2048);
    JNI_CHECK();
    for (;;) {
        jint n = (*env)->CallIntMethod(
            env, stream, cached ? http_jni.i_read : (*env)->GetMethodID(env, streamc, "read", "([B)I"), buf);
        JNI_CHECK();
        if (n <= 0)
            break;
        if (out && cap && total + 1 < cap) {
            jsize take = (total + (size_t)n < cap) ? n : (jsize)(cap - 1 - total);
            if (take > 0) {
                (*env)->GetByteArrayRegion(env, buf, 0, take, (jbyte *)(out + total));
                total += (size_t)take;
                out[total] = '\0';
            }
        }
    }
    (*env)->CallVoidMethod(env, stream,
                           cached ? http_jni.i_close : (*env)->GetMethodID(env, streamc, "close", "()V"));
    JNI_CHECK();
closeconn:
    /* Drain and close stream without disconnect() so keep-alive reuses the socket. */
    ok = 1;
done:
    http_log_exception(env, method);
    (*env)->PopLocalFrame(env, NULL);
    return ok ? code : 0;
#undef JNI_CHECK
}

/* Retry once on a transport error for methods that are safe to repeat. */
static int http_retry_safe(const char *method) {
    const char *m = method ? method : "GET";
    return strcmp(m, "GET") == 0 || strcmp(m, "PUT") == 0 || strcmp(m, "PATCH") == 0 || strcmp(m, "DELETE") == 0;
}

static int http_ex(const char *method, const char *url, const char *body, char *out, size_t cap, const char *header,
                   const char *value, char *etag, size_t etag_cap) {
    int code = http_ex_once(method, url, body, out, cap, header, value, etag, etag_cap);
    if (code == 0 && !net_fast && http_retry_safe(method))
        code = http_ex_once(method, url, body, out, cap, header, value, etag, etag_cap);
    return code;
}

/* Stream Server-Sent Events via GET; returns HTTP status or 0 on transport error. */
static int http_stream(const char *url, const char *accept, DSStreamChunkFn cb, void *ctx) {
    JNIEnv *env = NULL;
    jobject conn = NULL, stream = NULL, urlobj = NULL;
    jbyteArray buf;
    int code = 0, cached;
    env = http_env();
    if (!env || !cb)
        return 0;
    if ((*env)->PushLocalFrame(env, 32) != 0)
        goto done;
    cached = http_jni_resolve(env);
    if (!cached)
        goto done;
    {
        jstring ju = (*env)->NewStringUTF(env, url);
        if ((*env)->ExceptionCheck(env))
            goto done;
        urlobj = (*env)->NewObject(env, http_jni.url, http_jni.url_init, ju);
        if ((*env)->ExceptionCheck(env))
            goto done;
        conn = (*env)->CallObjectMethod(env, urlobj, http_jni.url_open);
        if ((*env)->ExceptionCheck(env) || !conn)
            goto done;
        jstring jm = (*env)->NewStringUTF(env, "GET");
        (*env)->CallVoidMethod(env, conn, http_jni.c_method, jm);
        (*env)->CallVoidMethod(env, conn, http_jni.c_ctmo, TIMEOUT);
        /* Use long read timeout for persistent SSE stream; caller reconnects on drop. */
        (*env)->CallVoidMethod(env, conn, http_jni.c_rtmo, 120000);
        (*env)->CallVoidMethod(env, conn, http_jni.c_cache, JNI_FALSE);
        if (accept) {
            jstring k = (*env)->NewStringUTF(env, "Accept"), v = (*env)->NewStringUTF(env, accept);
            (*env)->CallVoidMethod(env, conn, http_jni.c_prop, k, v);
        }
        if ((*env)->ExceptionCheck(env))
            goto done;
        code = (int)(*env)->CallIntMethod(env, conn, http_jni.c_code);
        if ((*env)->ExceptionCheck(env) || code != 200)
            goto done;
        stream = (*env)->CallObjectMethod(env, conn, http_jni.c_getin);
        if ((*env)->ExceptionCheck(env) || !stream)
            goto done;
        if (!cb(ctx, "", 0)) /* connected ping, lets the caller go live */
            goto done;
        buf = (*env)->NewByteArray(env, 2048);
        if ((*env)->ExceptionCheck(env))
            goto done;
        for (;;) {
            jint n = (*env)->CallIntMethod(env, stream, http_jni.i_read, buf);
            if ((*env)->ExceptionCheck(env))
                break;
            if (n <= 0)
                break;
            char chunk[2048];
            if (n > (jint)sizeof(chunk))
                n = (jint)sizeof(chunk);
            (*env)->GetByteArrayRegion(env, buf, 0, n, (jbyte *)chunk);
            if (!cb(ctx, chunk, (size_t)n))
                break;
        }
        (*env)->CallVoidMethod(env, stream, http_jni.i_close);
        (*env)->ExceptionClear(env);
    }
done:
    http_log_exception(env, "STREAM");
    (*env)->PopLocalFrame(env, NULL);
    return code;
}
