/*
 * aria_router_shim.c — Express-style HTTP router for Aria
 *
 * Provides: route registration, pattern matching with path params,
 *           middleware chain, and query string parsing.
 *
 * Design: Routes are registered with a handler ID (int32).
 * When a request arrives, router_match() finds the best route
 * and returns the handler ID. Path parameters are extracted
 * and available via router_param().
 *
 * Build:
 *   gcc -shared -fPIC -O2 -o libaria_router_shim.so aria_router_shim.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define MAX_ROUTES       256
#define MAX_PARAMS       16
#define MAX_MIDDLEWARE    64
#define MAX_PATTERN_LEN  512
#define MAX_PARAM_NAME   64
#define MAX_PARAM_VALUE  512

/* ------------------------------------------------------------------ */
/* Data structures                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char method[16];             /* GET, POST, PUT, DELETE, PATCH, * */
    char pattern[MAX_PATTERN_LEN]; /* e.g. "/users/:id/posts/:pid" */
    int32_t handler_id;          /* Aria-side handler identifier */
    int  segment_count;          /* cached: number of segments */
    /* Parsed segments for matching */
    char segments[32][MAX_PARAM_NAME];
    int  is_param[32];           /* 1 if segment is :param */
    int  is_wildcard;            /* 1 if pattern ends with * */
} Route;

typedef struct {
    int32_t middleware_id;       /* Aria-side middleware identifier */
    char path_prefix[MAX_PATTERN_LEN]; /* "" = global, "/api" = scoped */
} Middleware;

typedef struct {
    char name[MAX_PARAM_NAME];
    char value[MAX_PARAM_VALUE];
} Param;

/* ------------------------------------------------------------------ */
/* Global state                                                        */
/* ------------------------------------------------------------------ */

static Route      g_routes[MAX_ROUTES];
static int        g_route_count = 0;

static Middleware  g_middleware[MAX_MIDDLEWARE];
static int        g_mw_count = 0;

/* Matched param storage — valid after router_match() */
static Param      g_params[MAX_PARAMS];
static int        g_param_count = 0;
static int32_t    g_matched_handler = -1;

/* Middleware matches — valid after router_match() */
static int32_t    g_matched_mw[MAX_MIDDLEWARE];
static int        g_matched_mw_count = 0;

/* Error */
static char       g_err[256] = "";

/* Test helpers */
static int g_test_pass = 0;
static int g_test_fail = 0;

/* ------------------------------------------------------------------ */
/* Internal: parse a URL path into segments                            */
/* ------------------------------------------------------------------ */

static int split_path(const char* path, char segments[][MAX_PARAM_NAME],
                      int max_segments) {
    int count = 0;
    const char* p = path;
    if (*p == '/') p++; /* skip leading slash */

    while (*p && count < max_segments) {
        const char* slash = strchr(p, '/');
        int len;
        if (slash) {
            len = (int)(slash - p);
        } else {
            len = (int)strlen(p);
        }
        if (len > 0) {
            if (len >= MAX_PARAM_NAME) len = MAX_PARAM_NAME - 1;
            memcpy(segments[count], p, len);
            segments[count][len] = 0;
            count++;
        }
        if (!slash) break;
        p = slash + 1;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/* Add a route. Method can be "GET","POST","PUT","DELETE","PATCH","*".
 * Pattern supports :param segments and trailing * wildcard.
 * handler_id is returned by router_match when this route matches.    */
void aria_router_add(const char* method, const char* pattern,
                     int32_t handler_id) {
    if (g_route_count >= MAX_ROUTES) {
        snprintf(g_err, sizeof(g_err), "too many routes (max %d)", MAX_ROUTES);
        return;
    }
    Route* r = &g_routes[g_route_count];
    snprintf(r->method, sizeof(r->method), "%s", method);
    snprintf(r->pattern, sizeof(r->pattern), "%s", pattern);
    r->handler_id = handler_id;

    /* Check for trailing wildcard */
    int plen = (int)strlen(pattern);
    r->is_wildcard = (plen > 0 && pattern[plen - 1] == '*');

    /* Parse segments for matching */
    char temp_pattern[MAX_PATTERN_LEN];
    snprintf(temp_pattern, sizeof(temp_pattern), "%s", pattern);
    /* Remove trailing * for segment parsing */
    if (r->is_wildcard && plen > 1) {
        temp_pattern[plen - 1] = 0;
        if (temp_pattern[plen - 2] == '/') temp_pattern[plen - 2] = 0;
    }

    r->segment_count = split_path(temp_pattern, r->segments, 32);
    for (int i = 0; i < r->segment_count; i++) {
        r->is_param[i] = (r->segments[i][0] == ':');
    }

    g_route_count++;
}

/* Register middleware. path_prefix="" applies globally.
 * middleware_id is included in the match result for matching paths.   */
void aria_router_use(const char* path_prefix, int32_t middleware_id) {
    if (g_mw_count >= MAX_MIDDLEWARE) return;
    Middleware* mw = &g_middleware[g_mw_count];
    mw->middleware_id = middleware_id;
    snprintf(mw->path_prefix, sizeof(mw->path_prefix), "%s", path_prefix);
    g_mw_count++;
}

/* Match a request method+path against registered routes.
 * Returns handler_id of the matched route, or -1 if no match.
 * After a match, use router_param() to get path parameter values,
 * and router_middleware_*() to get applicable middleware.             */
int32_t aria_router_match(const char* method, const char* path) {
    g_param_count = 0;
    g_matched_handler = -1;
    g_matched_mw_count = 0;
    g_err[0] = 0;

    /* Parse the request path into segments */
    char req_segments[32][MAX_PARAM_NAME];
    int req_seg_count = split_path(path, req_segments, 32);

    /* Find matching route */
    for (int i = 0; i < g_route_count; i++) {
        Route* r = &g_routes[i];

        /* Check method (exact match or wildcard) */
        if (r->method[0] != '*' && strcasecmp(r->method, method) != 0)
            continue;

        /* Check segment count */
        if (!r->is_wildcard && req_seg_count != r->segment_count)
            continue;
        if (r->is_wildcard && req_seg_count < r->segment_count)
            continue;

        /* Match each segment */
        int matched = 1;
        Param temp_params[MAX_PARAMS];
        int temp_param_count = 0;

        for (int s = 0; s < r->segment_count && matched; s++) {
            if (r->is_param[s]) {
                /* Parameter segment — always matches, extract value */
                if (temp_param_count < MAX_PARAMS) {
                    /* Skip the ':' prefix for param name */
                    snprintf(temp_params[temp_param_count].name,
                             MAX_PARAM_NAME, "%s", r->segments[s] + 1);
                    snprintf(temp_params[temp_param_count].value,
                             MAX_PARAM_VALUE, "%s", req_segments[s]);
                    temp_param_count++;
                }
            } else {
                /* Literal segment — must match exactly */
                if (strcmp(r->segments[s], req_segments[s]) != 0)
                    matched = 0;
            }
        }

        if (matched) {
            /* Copy params */
            g_param_count = temp_param_count;
            memcpy(g_params, temp_params, sizeof(Param) * temp_param_count);
            g_matched_handler = r->handler_id;

            /* Find applicable middleware */
            int pathlen = (int)strlen(path);
            for (int m = 0; m < g_mw_count; m++) {
                int pfxlen = (int)strlen(g_middleware[m].path_prefix);
                if (pfxlen == 0 ||
                    (pathlen >= pfxlen &&
                     strncmp(path, g_middleware[m].path_prefix, pfxlen) == 0)) {
                    if (g_matched_mw_count < MAX_MIDDLEWARE) {
                        g_matched_mw[g_matched_mw_count++] =
                            g_middleware[m].middleware_id;
                    }
                }
            }
            return r->handler_id;
        }
    }

    return -1; /* no match */
}

/* Get a named path parameter from the last match.
 * Returns "" if not found.                                            */
const char* aria_router_param(const char* name) {
    for (int i = 0; i < g_param_count; i++) {
        if (strcmp(g_params[i].name, name) == 0)
            return g_params[i].value;
    }
    return "";
}

/* Number of path parameters from last match */
int32_t aria_router_param_count(void) {
    return g_param_count;
}

/* Get parameter name by index */
const char* aria_router_param_name(int32_t index) {
    if (index < 0 || index >= g_param_count) return "";
    return g_params[index].name;
}

/* Get parameter value by index */
const char* aria_router_param_value(int32_t index) {
    if (index < 0 || index >= g_param_count) return "";
    return g_params[index].value;
}

/* Number of middleware that matched the last request */
int32_t aria_router_middleware_count(void) {
    return g_matched_mw_count;
}

/* Get middleware ID by index (in registration order) */
int32_t aria_router_middleware_id(int32_t index) {
    if (index < 0 || index >= g_matched_mw_count) return -1;
    return g_matched_mw[index];
}

/* Number of registered routes */
int32_t aria_router_route_count(void) {
    return g_route_count;
}

/* Reset all routes and middleware */
void aria_router_reset(void) {
    g_route_count = 0;
    g_mw_count = 0;
    g_param_count = 0;
    g_matched_handler = -1;
    g_matched_mw_count = 0;
}

/* Last error message */
const char* aria_router_error(void) {
    return g_err;
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

void aria_router_assert_int_eq(const char* label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        g_test_pass++;
        fprintf(stderr, "  PASS: %s\n", label);
    } else {
        g_test_fail++;
        fprintf(stderr, "  FAIL: %s — expected %d, got %d\n",
                label, expected, actual);
    }
}

void aria_router_assert_str_eq(const char* label, const char* expected,
                                const char* actual) {
    if (strcmp(expected, actual) == 0) {
        g_test_pass++;
        fprintf(stderr, "  PASS: %s\n", label);
    } else {
        g_test_fail++;
        fprintf(stderr, "  FAIL: %s — expected \"%s\", got \"%s\"\n",
                label, expected, actual);
    }
}

void aria_router_test_summary(void) {
    fprintf(stderr, "\n--- aria-router: %d passed, %d failed ---\n",
            g_test_pass, g_test_fail);
}
