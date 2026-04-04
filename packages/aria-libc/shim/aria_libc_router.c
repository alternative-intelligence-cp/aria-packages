/*
 * aria_libc_router — HTTP route matching for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_router.so aria_libc_router.c
 *
 * Provides route registration, pattern matching with path params, middleware tracking.
 * Patterns: exact "/about", params "/users/:id", wildcard "/static/*"
 *
 * v0.12.0: New — replaces old aria_router_shim.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    const char *data;
    int64_t     length;
} AriaString;

/* ════════════════════════════════════════════════════════════════════════════
 * Route storage
 * ════════════════════════════════════════════════════════════════════════════ */
#define MAX_ROUTES       256
#define MAX_MIDDLEWARE    64
#define MAX_PARAMS       16
#define METHOD_LEN       16
#define PATTERN_LEN      1024
#define PARAM_NAME_LEN   128
#define PARAM_VAL_LEN    512

typedef struct {
    char method[METHOD_LEN];
    char pattern[PATTERN_LEN];
    int64_t handler_id;
} Route;

static Route g_routes[MAX_ROUTES];
static int   g_route_count = 0;

static int64_t g_middleware[MAX_MIDDLEWARE];
static int     g_mw_count = 0;

/* last match results */
static int64_t g_matched_handler = -1;

static char g_param_names[MAX_PARAMS][PARAM_NAME_LEN];
static char g_param_values[MAX_PARAMS][PARAM_VAL_LEN];
static int  g_param_count = 0;

static char g_err_buf[256] = "";
static char g_ret_buf[PARAM_VAL_LEN] = "";

/* ════════════════════════════════════════════════════════════════════════════
 * Route registration
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_router_add(const char *method, const char *pattern,
                              int64_t handler_id) {
    if (g_route_count >= MAX_ROUTES) {
        snprintf(g_err_buf, sizeof(g_err_buf), "max routes exceeded");
        return 0;
    }
    Route *r = &g_routes[g_route_count];
    strncpy(r->method, method, METHOD_LEN - 1);
    r->method[METHOD_LEN - 1] = '\0';
    strncpy(r->pattern, pattern, PATTERN_LEN - 1);
    r->pattern[PATTERN_LEN - 1] = '\0';
    r->handler_id = handler_id;
    g_route_count++;
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Internal: match a pattern against a path, extracting params
 * Returns 1 on match, 0 no match
 * ════════════════════════════════════════════════════════════════════════════ */
static int match_pattern(const char *pattern, const char *path) {
    g_param_count = 0;
    const char *pp = pattern;
    const char *pa = path;

    while (*pp && *pa) {
        if (*pp == ':') {
            /* parameter segment: extract name until / or end */
            pp++; /* skip ':' */
            const char *name_start = pp;
            while (*pp && *pp != '/') pp++;
            int nlen = (int)(pp - name_start);
            if (nlen >= PARAM_NAME_LEN) nlen = PARAM_NAME_LEN - 1;

            /* extract value from path until / or end */
            const char *val_start = pa;
            while (*pa && *pa != '/') pa++;
            int vlen = (int)(pa - val_start);
            if (vlen >= PARAM_VAL_LEN) vlen = PARAM_VAL_LEN - 1;

            if (g_param_count < MAX_PARAMS) {
                memcpy(g_param_names[g_param_count], name_start, (size_t)nlen);
                g_param_names[g_param_count][nlen] = '\0';
                memcpy(g_param_values[g_param_count], val_start, (size_t)vlen);
                g_param_values[g_param_count][vlen] = '\0';
                g_param_count++;
            }
            continue;
        }
        if (*pp == '*') {
            /* wildcard: matches everything remaining */
            return 1;
        }
        if (*pp != *pa) return 0;
        pp++;
        pa++;
    }

    /* both must be exhausted for exact match */
    if (*pp == '\0' && *pa == '\0') return 1;
    /* trailing wildcard */
    if (*pp == '*') return 1;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Route matching
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_router_match(const char *method, const char *path) {
    g_matched_handler = -1;
    g_param_count = 0;

    for (int i = 0; i < g_route_count; i++) {
        /* method check (empty method = match all) */
        if (g_routes[i].method[0] != '\0' &&
            strcasecmp(g_routes[i].method, method) != 0) {
            continue;
        }
        if (match_pattern(g_routes[i].pattern, path)) {
            g_matched_handler = g_routes[i].handler_id;
            return g_matched_handler;
        }
    }
    return -1;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Parameter accessors (after match)
 * ════════════════════════════════════════════════════════════════════════════ */

AriaString aria_libc_router_param(const char *name) {
    for (int i = 0; i < g_param_count; i++) {
        if (strcmp(g_param_names[i], name) == 0) {
            return (AriaString){ g_param_values[i],
                                 (int64_t)strlen(g_param_values[i]) };
        }
    }
    return (AriaString){ "", 0 };
}

int64_t aria_libc_router_param_count(void) {
    return (int64_t)g_param_count;
}

AriaString aria_libc_router_param_name(int64_t idx) {
    if (idx < 0 || idx >= g_param_count) return (AriaString){ "", 0 };
    return (AriaString){ g_param_names[idx],
                         (int64_t)strlen(g_param_names[idx]) };
}

AriaString aria_libc_router_param_value(int64_t idx) {
    if (idx < 0 || idx >= g_param_count) return (AriaString){ "", 0 };
    return (AriaString){ g_param_values[idx],
                         (int64_t)strlen(g_param_values[idx]) };
}

/* ════════════════════════════════════════════════════════════════════════════
 * Middleware
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_router_use_mw(int64_t middleware_id) {
    if (g_mw_count >= MAX_MIDDLEWARE) return 0;
    g_middleware[g_mw_count++] = middleware_id;
    return 1;
}

int64_t aria_libc_router_mw_count(void) {
    return (int64_t)g_mw_count;
}

int64_t aria_libc_router_mw_id(int64_t idx) {
    if (idx < 0 || idx >= g_mw_count) return -1;
    return g_middleware[idx];
}

/* ════════════════════════════════════════════════════════════════════════════
 * State
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_router_route_count(void) {
    return (int64_t)g_route_count;
}

int64_t aria_libc_router_matched(void) {
    return g_matched_handler;
}

int64_t aria_libc_router_reset_all(void) {
    g_route_count = 0;
    g_mw_count = 0;
    g_matched_handler = -1;
    g_param_count = 0;
    g_err_buf[0] = '\0';
    return 1;
}

AriaString aria_libc_router_error(void) {
    return (AriaString){ g_err_buf, (int64_t)strlen(g_err_buf) };
}
