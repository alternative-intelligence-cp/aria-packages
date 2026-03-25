/*
 * aria_cors_shim.c — CORS header generation for Aria HTTP servers
 *
 * Configurable CORS policy: allowed origins, methods, headers, credentials,
 * max-age. Generates the correct header string for server_respond_headers().
 *
 * Thread-safety: single-threaded (global state like aria-server).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* ── limits ──────────────────────────────────────────────────────── */
#define MAX_ORIGINS       32
#define MAX_ORIGIN_LEN    256
#define MAX_METHODS_LEN   256
#define MAX_HDRS_LEN      512
#define MAX_OUTPUT         4096

/* ── configuration state ─────────────────────────────────────────── */
static char   g_origins[MAX_ORIGINS][MAX_ORIGIN_LEN];
static int    g_origin_count    = 0;
static int    g_allow_all       = 0;  /* "*" origin */

static char   g_methods[MAX_METHODS_LEN]  = "GET, POST, PUT, DELETE, OPTIONS";
static char   g_headers[MAX_HDRS_LEN]     = "Content-Type, Authorization";
static int    g_credentials     = 0;
static int    g_max_age         = 86400;  /* 24 hours default */

static char   g_expose_headers[MAX_HDRS_LEN] = "";

/* output buffer */
static char   g_output[MAX_OUTPUT] = "";

/* test tracking */
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ── helpers ─────────────────────────────────────────────────────── */

static int ci_strcmp(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 1;
        a++; b++;
    }
    return *a != *b;
}

/* ── configuration API ───────────────────────────────────────────── */

void aria_cors_reset(void) {
    g_origin_count = 0;
    g_allow_all    = 0;
    snprintf(g_methods, sizeof(g_methods), "GET, POST, PUT, DELETE, OPTIONS");
    snprintf(g_headers, sizeof(g_headers), "Content-Type, Authorization");
    g_credentials  = 0;
    g_max_age      = 86400;
    g_expose_headers[0] = '\0';
    g_output[0]    = '\0';
}

void aria_cors_allow_origin(const char *origin) {
    if (!origin) return;
    if (strcmp(origin, "*") == 0) {
        g_allow_all = 1;
        return;
    }
    if (g_origin_count < MAX_ORIGINS) {
        snprintf(g_origins[g_origin_count], MAX_ORIGIN_LEN, "%s", origin);
        g_origin_count++;
    }
}

void aria_cors_allow_methods(const char *methods) {
    if (!methods) return;
    snprintf(g_methods, sizeof(g_methods), "%s", methods);
}

void aria_cors_allow_headers(const char *headers) {
    if (!headers) return;
    snprintf(g_headers, sizeof(g_headers), "%s", headers);
}

void aria_cors_expose_headers(const char *headers) {
    if (!headers) return;
    snprintf(g_expose_headers, sizeof(g_expose_headers), "%s", headers);
}

void aria_cors_allow_credentials(int32_t allow) {
    g_credentials = allow ? 1 : 0;
}

void aria_cors_max_age(int32_t seconds) {
    g_max_age = seconds;
}

/* ── origin checking ─────────────────────────────────────────────── */

/* Check if a request origin is allowed. Returns 1 if allowed. */
int32_t aria_cors_is_allowed(const char *origin) {
    if (!origin || !*origin) return 0;
    if (g_allow_all) return 1;
    for (int i = 0; i < g_origin_count; i++) {
        if (ci_strcmp(g_origins[i], origin) == 0) return 1;
    }
    return 0;
}

/* ── header generation ───────────────────────────────────────────── */

/* Build CORS headers for a normal response.
 * origin: the Origin header from the request (for validation).
 * Returns a header string like "Access-Control-Allow-Origin: ...\r\nAccess-Control-...\r\n"
 * suitable for passing as extra_headers to server_respond_headers(). */
const char *aria_cors_headers(const char *origin) {
    g_output[0] = '\0';
    int pos = 0;

    /* If no origins configured and not allow-all, return empty */
    if (!g_allow_all && g_origin_count == 0) return g_output;

    /* Check if origin is allowed */
    if (!g_allow_all && !aria_cors_is_allowed(origin)) return g_output;

    /* Access-Control-Allow-Origin */
    if (g_allow_all && !g_credentials) {
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Access-Control-Allow-Origin: *\r\n");
    } else if (origin && *origin) {
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Access-Control-Allow-Origin: %s\r\n", origin);
        /* Vary header when reflecting specific origin */
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Vary: Origin\r\n");
    }

    /* Credentials */
    if (g_credentials) {
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Access-Control-Allow-Credentials: true\r\n");
    }

    /* Expose headers */
    if (g_expose_headers[0]) {
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Access-Control-Expose-Headers: %s\r\n", g_expose_headers);
    }

    return g_output;
}

/* Build CORS headers for a preflight (OPTIONS) response.
 * origin: the Origin header from the request.
 * Returns header string with preflight-specific headers. */
const char *aria_cors_preflight(const char *origin) {
    g_output[0] = '\0';
    int pos = 0;

    if (!g_allow_all && g_origin_count == 0) return g_output;
    if (!g_allow_all && !aria_cors_is_allowed(origin)) return g_output;

    /* Access-Control-Allow-Origin */
    if (g_allow_all && !g_credentials) {
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Access-Control-Allow-Origin: *\r\n");
    } else if (origin && *origin) {
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Access-Control-Allow-Origin: %s\r\n", origin);
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Vary: Origin\r\n");
    }

    /* Methods */
    pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                    "Access-Control-Allow-Methods: %s\r\n", g_methods);

    /* Headers */
    pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                    "Access-Control-Allow-Headers: %s\r\n", g_headers);

    /* Max-Age */
    pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                    "Access-Control-Max-Age: %d\r\n", g_max_age);

    /* Credentials */
    if (g_credentials) {
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Access-Control-Allow-Credentials: true\r\n");
    }

    return g_output;
}

/* Check if a request method is OPTIONS (preflight check helper).
 * Returns 1 if the method is OPTIONS. */
int32_t aria_cors_is_preflight(const char *method) {
    if (!method) return 0;
    return (ci_strcmp(method, "OPTIONS") == 0) ? 1 : 0;
}

/* ── test helpers ────────────────────────────────────────────────── */

void aria_cors_assert_str_eq(const char *label, const char *expected, const char *actual) {
    if (strcmp(expected, actual) == 0) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected: \"%s\"\n    got:      \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_cors_assert_int_eq(const char *label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected %d, got %d\n", label, expected, actual);
        g_tests_failed++;
    }
}

/* Check if actual string contains expected substring */
void aria_cors_assert_contains(const char *label, const char *expected, const char *actual) {
    if (strstr(actual, expected) != NULL) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected to contain: \"%s\"\n    in: \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_cors_test_summary(void) {
    printf("\n--- aria-cors: %d passed, %d failed ---\n",
           g_tests_passed, g_tests_failed);
    g_tests_passed = 0;
    g_tests_failed = 0;
}
