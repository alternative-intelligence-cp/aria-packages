/*
 * aria_libc_time — Time and clock operations for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_time.so aria_libc_time.c
 */
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ── Epoch / Clock ───────────────────────────────────────────────── */

int64_t aria_libc_time_now(void) {
    return (int64_t)time(NULL);
}

int64_t aria_libc_time_clock(void) {
    return (int64_t)clock();
}

int64_t aria_libc_time_clocks_per_sec(void) {
    return (int64_t)CLOCKS_PER_SEC;
}

double aria_libc_time_difftime(int64_t t1, int64_t t0) {
    return difftime((time_t)t1, (time_t)t0);
}

/* ── Monotonic (POSIX) ───────────────────────────────────────────── */

int64_t aria_libc_time_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int64_t aria_libc_time_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
}

/* ── UTC Decomposition ───────────────────────────────────────────── */

static struct tm g_tm;

static void decompose_utc(int64_t epoch) {
    time_t t = (time_t)epoch;
    gmtime_r(&t, &g_tm);
}

int32_t aria_libc_time_year(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)(g_tm.tm_year + 1900);
}

int32_t aria_libc_time_month(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)(g_tm.tm_mon + 1);
}

int32_t aria_libc_time_day(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)g_tm.tm_mday;
}

int32_t aria_libc_time_hour(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)g_tm.tm_hour;
}

int32_t aria_libc_time_minute(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)g_tm.tm_min;
}

int32_t aria_libc_time_second(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)g_tm.tm_sec;
}

int32_t aria_libc_time_weekday(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)g_tm.tm_wday;
}

int32_t aria_libc_time_yearday(int64_t epoch) {
    decompose_utc(epoch);
    return (int32_t)(g_tm.tm_yday + 1);
}

/* ── Construct ───────────────────────────────────────────────────── */

int64_t aria_libc_time_make_utc(int32_t year, int32_t month, int32_t day,
                                 int32_t hour, int32_t minute, int32_t second) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min  = minute;
    tm.tm_sec  = second;
    return (int64_t)timegm(&tm);
}

/* ── Formatting ──────────────────────────────────────────────────── */

static char g_fmt_buf[512];

const char *aria_libc_time_format(int64_t epoch, const char *fmt) {
    g_fmt_buf[0] = '\0';
    if (!fmt) return g_fmt_buf;
    decompose_utc(epoch);
    strftime(g_fmt_buf, sizeof(g_fmt_buf), fmt, &g_tm);
    return g_fmt_buf;
}

const char *aria_libc_time_iso8601(int64_t epoch) {
    return aria_libc_time_format(epoch, "%Y-%m-%dT%H:%M:%SZ");
}

/* ── Sleep ───────────────────────────────────────────────────────── */

void aria_libc_time_sleep_ms(int32_t ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void aria_libc_time_sleep_s(int32_t sec) {
    if (sec <= 0) return;
    struct timespec ts;
    ts.tv_sec  = sec;
    ts.tv_nsec = 0;
    nanosleep(&ts, NULL);
}

/* ── Wallclock milliseconds/microseconds ─────────────────────────── */

int64_t aria_libc_time_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

int64_t aria_libc_time_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

/* ── Local time decompose ────────────────────────────────────────── */

static void decompose_local(int64_t epoch) {
    time_t t = (time_t)epoch;
    localtime_r(&t, &g_tm);
}

int32_t aria_libc_time_local_year(int64_t epoch)    { decompose_local(epoch); return (int32_t)(g_tm.tm_year + 1900); }
int32_t aria_libc_time_local_month(int64_t epoch)   { decompose_local(epoch); return (int32_t)(g_tm.tm_mon + 1); }
int32_t aria_libc_time_local_day(int64_t epoch)     { decompose_local(epoch); return (int32_t)g_tm.tm_mday; }
int32_t aria_libc_time_local_hour(int64_t epoch)    { decompose_local(epoch); return (int32_t)g_tm.tm_hour; }
int32_t aria_libc_time_local_minute(int64_t epoch)  { decompose_local(epoch); return (int32_t)g_tm.tm_min; }
int32_t aria_libc_time_local_second(int64_t epoch)  { decompose_local(epoch); return (int32_t)g_tm.tm_sec; }
int32_t aria_libc_time_local_weekday(int64_t epoch) { decompose_local(epoch); return (int32_t)g_tm.tm_wday; }
int32_t aria_libc_time_local_yearday(int64_t epoch) { decompose_local(epoch); return (int32_t)(g_tm.tm_yday + 1); }

const char *aria_libc_time_format_local(int64_t epoch, const char *fmt) {
    g_fmt_buf[0] = '\0';
    if (!fmt) return g_fmt_buf;
    decompose_local(epoch);
    strftime(g_fmt_buf, sizeof(g_fmt_buf), fmt, &g_tm);
    return g_fmt_buf;
}

int64_t aria_libc_time_make_local(int32_t year, int32_t month, int32_t day,
                                  int32_t hour, int32_t minute, int32_t second) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
    t.tm_isdst = -1;
    return (int64_t)mktime(&t);
}
