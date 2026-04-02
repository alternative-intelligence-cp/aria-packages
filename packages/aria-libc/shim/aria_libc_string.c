/*
 * aria_libc_string — C string operations for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_string.so aria_libc_string.c
 */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

/* ── Length & Compare ────────────────────────────────────────────── */

int64_t aria_libc_string_strlen(const char *s) {
    if (!s) return 0;
    return (int64_t)strlen(s);
}

int32_t aria_libc_string_strcmp(const char *a, const char *b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    return (int32_t)strcmp(a, b);
}

int32_t aria_libc_string_strncmp(const char *a, const char *b, int64_t n) {
    if (!a || !b || n <= 0) return 0;
    return (int32_t)strncmp(a, b, (size_t)n);
}

int32_t aria_libc_string_strcasecmp(const char *a, const char *b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    return (int32_t)strcasecmp(a, b);
}

/* ── Search ──────────────────────────────────────────────────────── */

int64_t aria_libc_string_strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return -1;
    const char *p = strstr(haystack, needle);
    if (!p) return -1;
    return (int64_t)(p - haystack);
}

int64_t aria_libc_string_strchr(const char *s, int32_t c) {
    if (!s) return -1;
    const char *p = strchr(s, c);
    if (!p) return -1;
    return (int64_t)(p - s);
}

int64_t aria_libc_string_strrchr(const char *s, int32_t c) {
    if (!s) return -1;
    const char *p = strrchr(s, c);
    if (!p) return -1;
    return (int64_t)(p - s);
}

/* ── Copy & Concat ───────────────────────────────────────────────── */

static char g_buf[65536];

const char *aria_libc_string_strcat(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    if (la + lb >= sizeof(g_buf)) { g_buf[0] = '\0'; return g_buf; }
    memcpy(g_buf, a, la);
    memcpy(g_buf + la, b, lb);
    g_buf[la + lb] = '\0';
    return g_buf;
}

const char *aria_libc_string_substr(const char *s, int64_t start, int64_t len) {
    if (!s) { g_buf[0] = '\0'; return g_buf; }
    size_t slen = strlen(s);
    if (start < 0) start = 0;
    if ((size_t)start >= slen) { g_buf[0] = '\0'; return g_buf; }
    if (len < 0 || (size_t)(start + len) > slen) len = (int64_t)(slen - start);
    if ((size_t)len >= sizeof(g_buf)) len = sizeof(g_buf) - 1;
    memcpy(g_buf, s + start, (size_t)len);
    g_buf[len] = '\0';
    return g_buf;
}

const char *aria_libc_string_strdup(const char *s) {
    if (!s) { g_buf[0] = '\0'; return g_buf; }
    size_t len = strlen(s);
    if (len >= sizeof(g_buf)) len = sizeof(g_buf) - 1;
    memcpy(g_buf, s, len);
    g_buf[len] = '\0';
    return g_buf;
}

/* ── Memory Operations ───────────────────────────────────────────── */

void aria_libc_string_memcpy(int64_t dst, int64_t src, int64_t n) {
    if (dst && src && n > 0)
        memcpy((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, (size_t)n);
}

void aria_libc_string_memset(int64_t dst, int32_t val, int64_t n) {
    if (dst && n > 0)
        memset((void *)(uintptr_t)dst, val, (size_t)n);
}

int32_t aria_libc_string_memcmp(int64_t a, int64_t b, int64_t n) {
    if (!a || !b || n <= 0) return 0;
    return (int32_t)memcmp((const void *)(uintptr_t)a, (const void *)(uintptr_t)b, (size_t)n);
}

/* ── Case ────────────────────────────────────────────────────────── */

const char *aria_libc_string_toupper(const char *s) {
    if (!s) { g_buf[0] = '\0'; return g_buf; }
    size_t i = 0;
    for (; s[i] && i < sizeof(g_buf) - 1; i++)
        g_buf[i] = (char)toupper((unsigned char)s[i]);
    g_buf[i] = '\0';
    return g_buf;
}

const char *aria_libc_string_tolower(const char *s) {
    if (!s) { g_buf[0] = '\0'; return g_buf; }
    size_t i = 0;
    for (; s[i] && i < sizeof(g_buf) - 1; i++)
        g_buf[i] = (char)tolower((unsigned char)s[i]);
    g_buf[i] = '\0';
    return g_buf;
}

/* ── Trim ────────────────────────────────────────────────────────── */

const char *aria_libc_string_trim(const char *s) {
    if (!s) { g_buf[0] = '\0'; return g_buf; }
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    if (len >= sizeof(g_buf)) len = sizeof(g_buf) - 1;
    memcpy(g_buf, s, len);
    g_buf[len] = '\0';
    return g_buf;
}

/* ── Char Classification ─────────────────────────────────────────── */

int32_t aria_libc_string_isalpha(int32_t c)  { return isalpha(c)  ? 1 : 0; }
int32_t aria_libc_string_isdigit(int32_t c)  { return isdigit(c)  ? 1 : 0; }
int32_t aria_libc_string_isalnum(int32_t c)  { return isalnum(c)  ? 1 : 0; }
int32_t aria_libc_string_isspace(int32_t c)  { return isspace(c)  ? 1 : 0; }
int32_t aria_libc_string_ispunct(int32_t c)  { return ispunct(c)  ? 1 : 0; }

/* ── Conversion ──────────────────────────────────────────────────── */

int64_t aria_libc_string_atoi(const char *s) {
    if (!s) return 0;
    return (int64_t)atoll(s);
}

double aria_libc_string_atof(const char *s) {
    if (!s) return 0.0;
    return atof(s);
}

/* ── Byte-level String Access (for stdlib/string.aria port) ──────── */

int32_t aria_libc_string_byte_at(const char *s, int64_t idx) {
    if (!s || idx < 0) return -1;
    int64_t len = (int64_t)strlen(s);
    if (idx >= len) return -1;
    return (int32_t)(unsigned char)s[idx];
}

void aria_libc_string_copy_to_buf(int64_t dst_ptr, int64_t dst_off,
                                   const char *src, int64_t src_off,
                                   int64_t len) {
    if (!dst_ptr || !src || len <= 0) return;
    memcpy((char *)(uintptr_t)(dst_ptr + dst_off), src + src_off, (size_t)len);
}

const char *aria_libc_string_from_buf(int64_t buf_ptr, int64_t offset,
                                       int64_t len) {
    if (!buf_ptr || len <= 0) { g_buf[0] = '\0'; return g_buf; }
    if ((size_t)len >= sizeof(g_buf)) len = sizeof(g_buf) - 1;
    memcpy(g_buf, (const char *)(uintptr_t)(buf_ptr + offset), (size_t)len);
    g_buf[len] = '\0';
    return g_buf;
}
