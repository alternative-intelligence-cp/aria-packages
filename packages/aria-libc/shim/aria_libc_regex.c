/*
 * aria_libc_regex — POSIX regex for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_regex.so aria_libc_regex.c
 */
#include <regex.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Up to 16 compiled regex patterns */
#define MAX_REGEX 16
#define MAX_MATCHES 32

static regex_t g_regex[MAX_REGEX];
static int g_regex_used[MAX_REGEX] = {0};
static regmatch_t g_matches[MAX_MATCHES];
static char g_match_buf[4096];
static char g_error_buf[256];

/* ── Compile / Free ──────────────────────────────────────────────── */

int32_t aria_libc_regex_compile(const char *pattern, int32_t flags) {
    if (!pattern) return -1;
    int slot = -1;
    for (int i = 0; i < MAX_REGEX; i++) {
        if (!g_regex_used[i]) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int cflags = REG_EXTENDED;
    if (flags & 1) cflags |= REG_ICASE;
    if (flags & 2) cflags |= REG_NEWLINE;

    int rc = regcomp(&g_regex[slot], pattern, cflags);
    if (rc != 0) {
        regerror(rc, &g_regex[slot], g_error_buf, sizeof(g_error_buf));
        return -1;
    }
    g_regex_used[slot] = 1;
    return (int32_t)slot;
}

void aria_libc_regex_free(int32_t handle) {
    if (handle < 0 || handle >= MAX_REGEX || !g_regex_used[handle]) return;
    regfree(&g_regex[handle]);
    g_regex_used[handle] = 0;
}

/* ── Match ───────────────────────────────────────────────────────── */

int32_t aria_libc_regex_match(int32_t handle, const char *text) {
    if (handle < 0 || handle >= MAX_REGEX || !g_regex_used[handle] || !text)
        return 0;
    return regexec(&g_regex[handle], text, 0, NULL, 0) == 0 ? 1 : 0;
}

int32_t aria_libc_regex_search(int32_t handle, const char *text) {
    if (handle < 0 || handle >= MAX_REGEX || !g_regex_used[handle] || !text)
        return -1;
    int rc = regexec(&g_regex[handle], text, MAX_MATCHES, g_matches, 0);
    if (rc != 0) return -1;
    return (int32_t)g_matches[0].rm_so;
}

/* ── Capture Groups ──────────────────────────────────────────────── */

int32_t aria_libc_regex_captures(int32_t handle, const char *text) {
    if (handle < 0 || handle >= MAX_REGEX || !g_regex_used[handle] || !text)
        return 0;
    int rc = regexec(&g_regex[handle], text, MAX_MATCHES, g_matches, 0);
    if (rc != 0) return 0;
    int count = 0;
    for (int i = 0; i < MAX_MATCHES; i++) {
        if (g_matches[i].rm_so < 0) break;
        count++;
    }
    return (int32_t)count;
}

const char *aria_libc_regex_capture(int32_t idx, const char *text) {
    g_match_buf[0] = '\0';
    if (idx < 0 || idx >= MAX_MATCHES || !text) return g_match_buf;
    if (g_matches[idx].rm_so < 0) return g_match_buf;
    int start = g_matches[idx].rm_so;
    int len = g_matches[idx].rm_eo - start;
    if ((size_t)len >= sizeof(g_match_buf)) len = sizeof(g_match_buf) - 1;
    memcpy(g_match_buf, text + start, (size_t)len);
    g_match_buf[len] = '\0';
    return g_match_buf;
}

int32_t aria_libc_regex_capture_start(int32_t idx) {
    if (idx < 0 || idx >= MAX_MATCHES) return -1;
    return (int32_t)g_matches[idx].rm_so;
}

int32_t aria_libc_regex_capture_end(int32_t idx) {
    if (idx < 0 || idx >= MAX_MATCHES) return -1;
    return (int32_t)g_matches[idx].rm_eo;
}

/* ── Replace (first match) ───────────────────────────────────────── */

static char g_replace_buf[65536];

const char *aria_libc_regex_replace(int32_t handle, const char *text,
                                     const char *replacement) {
    g_replace_buf[0] = '\0';
    if (handle < 0 || handle >= MAX_REGEX || !g_regex_used[handle] || !text || !replacement)
        return g_replace_buf;
    regmatch_t m[1];
    int rc = regexec(&g_regex[handle], text, 1, m, 0);
    if (rc != 0) {
        /* No match — return original */
        size_t len = strlen(text);
        if (len >= sizeof(g_replace_buf)) len = sizeof(g_replace_buf) - 1;
        memcpy(g_replace_buf, text, len);
        g_replace_buf[len] = '\0';
        return g_replace_buf;
    }
    size_t pre = (size_t)m[0].rm_so;
    size_t rep = strlen(replacement);
    size_t post = strlen(text + m[0].rm_eo);
    if (pre + rep + post >= sizeof(g_replace_buf)) return g_replace_buf;
    memcpy(g_replace_buf, text, pre);
    memcpy(g_replace_buf + pre, replacement, rep);
    memcpy(g_replace_buf + pre + rep, text + m[0].rm_eo, post);
    g_replace_buf[pre + rep + post] = '\0';
    return g_replace_buf;
}

/* ── Error Info ──────────────────────────────────────────────────── */

const char *aria_libc_regex_last_error(void) {
    return g_error_buf;
}
