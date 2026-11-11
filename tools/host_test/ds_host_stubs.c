/* Minimal host stand-ins for Android logging + a software renderer fallback so
 * the DiScript VM can be exercised on Linux without a device. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s] ", tag ? tag : "?");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    return 0;
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    (void)prio;
    fprintf(stderr, "[%s] ", tag ? tag : "?");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    return 0;
}

/* Wall-clock hook used by the DiScript stdlib (time.wall). */
long ds_wall_clock(void) { return (long)time(NULL); }
