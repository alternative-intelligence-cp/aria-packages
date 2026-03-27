/* aria_path_shim.c — Path manipulation helpers for Aria
 * All string params are const char* (Aria ABI).
 * String returns are const char* (Aria runtime wraps to AriaString).
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>

static char result_buf[4096];

const char *aria_path_shim_join(const char *a, const char *b) {
    if (!a || !*a) return b ? b : "";
    if (!b || !*b) return a;
    size_t alen = strlen(a);
    if (a[alen - 1] == '/') {
        snprintf(result_buf, sizeof(result_buf), "%s%s", a, b);
    } else {
        snprintf(result_buf, sizeof(result_buf), "%s/%s", a, b);
    }
    return result_buf;
}

const char *aria_path_shim_basename(const char *p) {
    if (!p || !*p) return "";
    size_t len = strlen(p);
    /* skip trailing slash */
    if (len > 1 && p[len - 1] == '/') len--;
    /* find last slash */
    const char *last = NULL;
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '/') last = p + i;
    }
    if (!last) {
        /* no slash — copy relevant portion */
        memcpy(result_buf, p, len);
        result_buf[len] = '\0';
        return result_buf;
    }
    size_t start = (last - p) + 1;
    size_t blen = len - start;
    memcpy(result_buf, p + start, blen);
    result_buf[blen] = '\0';
    return result_buf;
}

const char *aria_path_shim_dirname(const char *p) {
    if (!p || !*p) return ".";
    size_t len = strlen(p);
    if (len > 1 && p[len - 1] == '/') len--;
    /* find last slash */
    int64_t last = -1;
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '/') last = (int64_t)i;
    }
    if (last < 0) return ".";
    if (last == 0) return "/";
    memcpy(result_buf, p, last);
    result_buf[last] = '\0';
    return result_buf;
}

const char *aria_path_shim_extension(const char *p) {
    if (!p) return "";
    /* get basename first */
    const char *base = aria_path_shim_basename(p);
    /* find last dot */
    const char *dot = NULL;
    for (const char *c = base; *c; c++) {
        if (*c == '.') dot = c;
    }
    if (!dot || dot == base) return "";
    return dot;  /* returns ".ext" */
}

const char *aria_path_shim_stem(const char *p) {
    if (!p) return "";
    const char *base = aria_path_shim_basename(p);
    /* find last dot */
    const char *dot = NULL;
    for (const char *c = base; *c; c++) {
        if (*c == '.') dot = c;
    }
    if (!dot || dot == base) return base;
    size_t stem_len = dot - base;
    /* can't reuse result_buf since basename already used it — use static secondary */
    static char stem_buf[4096];
    memcpy(stem_buf, base, stem_len);
    stem_buf[stem_len] = '\0';
    return stem_buf;
}

int64_t aria_path_shim_is_absolute(const char *p) {
    return (p && p[0] == '/') ? 1 : 0;
}

int64_t aria_path_shim_is_relative(const char *p) {
    return (p && p[0] != '/') ? 1 : 0;
}

int64_t aria_path_shim_has_extension(const char *p) {
    const char *ext = aria_path_shim_extension(p);
    return (ext && *ext) ? 1 : 0;
}
