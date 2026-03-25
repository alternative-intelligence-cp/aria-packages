/*
 * aria_cookie_shim.c — HTTP cookie parsing and Set-Cookie generation
 *
 * Supports:
 *   - Parse Cookie header into name/value pairs
 *   - Build Set-Cookie headers with attributes (Path, Domain, Max-Age,
 *     Expires, Secure, HttpOnly, SameSite)
 *
 * Thread-safety: single-threaded (global state).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* ── limits ──────────────────────────────────────────────────────── */
#define MAX_COOKIES       64
#define MAX_NAME_LEN      128
#define MAX_VALUE_LEN     4096
#define MAX_SET_COOKIE    8192

/* ── parsed cookie storage ───────────────────────────────────────── */
static char   g_names[MAX_COOKIES][MAX_NAME_LEN];
static char   g_values[MAX_COOKIES][MAX_VALUE_LEN];
static int    g_cookie_count = 0;

/* Set-Cookie builder state */
static char   g_set_name[MAX_NAME_LEN]     = "";
static char   g_set_value[MAX_VALUE_LEN]   = "";
static char   g_set_path[512]              = "";
static char   g_set_domain[256]            = "";
static int    g_set_max_age                = -1;  /* -1 = not set */
static int    g_set_secure                 = 0;
static int    g_set_httponly               = 0;
static char   g_set_samesite[16]           = "";   /* Strict, Lax, None */

/* output buffer */
static char   g_output[MAX_SET_COOKIE] = "";

/* test tracking */
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ── helpers ─────────────────────────────────────────────────────── */

static void trim(char *s) {
    /* trim leading */
    char *p = s;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    /* trim trailing */
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) {
        s[--len] = '\0';
    }
}

/* ── Cookie header parsing ───────────────────────────────────────── */

/*
 * Parse a Cookie header value: "name1=value1; name2=value2; ..."
 * Returns number of parsed cookies.
 */
int32_t aria_cookie_parse(const char *cookie_header) {
    g_cookie_count = 0;
    if (!cookie_header || !*cookie_header) return 0;

    /* work on a copy */
    char buf[65536];
    snprintf(buf, sizeof(buf), "%s", cookie_header);

    char *saveptr;
    char *pair = strtok_r(buf, ";", &saveptr);
    while (pair && g_cookie_count < MAX_COOKIES) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            snprintf(g_names[g_cookie_count], MAX_NAME_LEN, "%s", pair);
            trim(g_names[g_cookie_count]);
            snprintf(g_values[g_cookie_count], MAX_VALUE_LEN, "%s", eq + 1);
            trim(g_values[g_cookie_count]);
            g_cookie_count++;
        }
        pair = strtok_r(NULL, ";", &saveptr);
    }
    return (int32_t)g_cookie_count;
}

/* Get cookie value by name. Returns "" if not found. */
const char *aria_cookie_get(const char *name) {
    if (!name) return "";
    for (int i = 0; i < g_cookie_count; i++) {
        if (strcmp(g_names[i], name) == 0) return g_values[i];
    }
    return "";
}

/* Check if cookie exists. */
int32_t aria_cookie_has(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < g_cookie_count; i++) {
        if (strcmp(g_names[i], name) == 0) return 1;
    }
    return 0;
}

/* Get parsed cookie count. */
int32_t aria_cookie_count(void) {
    return (int32_t)g_cookie_count;
}

/* Get cookie name by index. */
const char *aria_cookie_name(int32_t index) {
    if (index < 0 || index >= g_cookie_count) return "";
    return g_names[index];
}

/* Get cookie value by index. */
const char *aria_cookie_value(int32_t index) {
    if (index < 0 || index >= g_cookie_count) return "";
    return g_values[index];
}

/* ── Set-Cookie builder ──────────────────────────────────────────── */

/* Reset builder state */
void aria_cookie_builder_reset(void) {
    g_set_name[0]     = '\0';
    g_set_value[0]    = '\0';
    g_set_path[0]     = '\0';
    g_set_domain[0]   = '\0';
    g_set_max_age     = -1;
    g_set_secure      = 0;
    g_set_httponly     = 0;
    g_set_samesite[0] = '\0';
    g_output[0]       = '\0';
}

void aria_cookie_builder_name(const char *name) {
    if (name) snprintf(g_set_name, MAX_NAME_LEN, "%s", name);
}

void aria_cookie_builder_value(const char *value) {
    if (value) snprintf(g_set_value, MAX_VALUE_LEN, "%s", value);
}

void aria_cookie_builder_path(const char *path) {
    if (path) snprintf(g_set_path, sizeof(g_set_path), "%s", path);
}

void aria_cookie_builder_domain(const char *domain) {
    if (domain) snprintf(g_set_domain, sizeof(g_set_domain), "%s", domain);
}

void aria_cookie_builder_max_age(int32_t seconds) {
    g_set_max_age = seconds;
}

void aria_cookie_builder_secure(int32_t flag) {
    g_set_secure = flag ? 1 : 0;
}

void aria_cookie_builder_httponly(int32_t flag) {
    g_set_httponly = flag ? 1 : 0;
}

void aria_cookie_builder_samesite(const char *policy) {
    if (policy) snprintf(g_set_samesite, sizeof(g_set_samesite), "%s", policy);
}

/*
 * Build the Set-Cookie header value from the builder state.
 * Returns: "Set-Cookie: name=value; Path=/; HttpOnly; Secure; SameSite=Strict\r\n"
 */
const char *aria_cookie_builder_build(void) {
    g_output[0] = '\0';
    if (!g_set_name[0]) return g_output;

    int pos = 0;
    pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos,
                    "Set-Cookie: %s=%s", g_set_name, g_set_value);

    if (g_set_path[0]) {
        pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos,
                        "; Path=%s", g_set_path);
    }
    if (g_set_domain[0]) {
        pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos,
                        "; Domain=%s", g_set_domain);
    }
    if (g_set_max_age >= 0) {
        pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos,
                        "; Max-Age=%d", g_set_max_age);
    }
    if (g_set_secure) {
        pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos, "; Secure");
    }
    if (g_set_httponly) {
        pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos, "; HttpOnly");
    }
    if (g_set_samesite[0]) {
        pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos,
                        "; SameSite=%s", g_set_samesite);
    }

    pos += snprintf(g_output + pos, MAX_SET_COOKIE - pos, "\r\n");
    return g_output;
}

/*
 * Convenience: build a delete cookie header (Max-Age=0).
 * Returns "Set-Cookie: name=; Path=/; Max-Age=0\r\n"
 */
const char *aria_cookie_delete(const char *name, const char *path) {
    g_output[0] = '\0';
    if (!name || !*name) return g_output;
    const char *p = (path && *path) ? path : "/";
    snprintf(g_output, MAX_SET_COOKIE,
             "Set-Cookie: %s=; Path=%s; Max-Age=0\r\n", name, p);
    return g_output;
}

/* Full reset (parse state + builder) */
void aria_cookie_reset(void) {
    g_cookie_count = 0;
    aria_cookie_builder_reset();
}

/* ── test helpers ────────────────────────────────────────────────── */

void aria_cookie_assert_str_eq(const char *label, const char *expected, const char *actual) {
    if (strcmp(expected, actual) == 0) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected: \"%s\"\n    got:      \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_cookie_assert_int_eq(const char *label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected %d, got %d\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_cookie_assert_contains(const char *label, const char *expected, const char *actual) {
    if (strstr(actual, expected) != NULL) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected to contain: \"%s\"\n    in: \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_cookie_test_summary(void) {
    printf("\n--- aria-cookie: %d passed, %d failed ---\n",
           g_tests_passed, g_tests_failed);
    g_tests_passed = 0;
    g_tests_failed = 0;
}
