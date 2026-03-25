/*
 * aria_session_shim.c — In-memory session management for Aria HTTP servers
 *
 * Supports:
 *   - Create/destroy sessions with unique IDs
 *   - Store key/value pairs per session
 *   - Session expiry (max-age in seconds)
 *   - Lookup session by ID
 *   - Generate session cookie header via aria-cookie integration
 *
 * Session IDs are generated using /dev/urandom for cryptographic randomness.
 * Thread-safety: single-threaded (global state).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

/* ── limits ──────────────────────────────────────────────────────── */
#define MAX_SESSIONS       256
#define MAX_SESSION_VARS   32
#define MAX_KEY_LEN        128
#define MAX_VAL_LEN        4096
#define SESSION_ID_LEN     32       /* 32 hex chars = 128 bits */
#define MAX_OUTPUT         8192

/* ── session data ────────────────────────────────────────────────── */
typedef struct {
    char   id[SESSION_ID_LEN + 1];
    char   keys[MAX_SESSION_VARS][MAX_KEY_LEN];
    char   vals[MAX_SESSION_VARS][MAX_VAL_LEN];
    int    var_count;
    time_t created;
    time_t last_access;
    int    active;
} Session;

static Session  g_sessions[MAX_SESSIONS];
static int      g_session_count = 0;
static int      g_max_age       = 3600;   /* 1 hour default */
static char     g_cookie_name[MAX_KEY_LEN] = "aria_sid";

/* current session pointer index (-1 = none) */
static int      g_current = -1;

/* output buffer */
static char     g_output[MAX_OUTPUT] = "";

/* test tracking */
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ── helpers ─────────────────────────────────────────────────────── */

/* Generate a random session ID using /dev/urandom */
static void generate_session_id(char *dst) {
    unsigned char buf[SESSION_ID_LEN / 2]; /* 16 bytes = 32 hex chars */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, sizeof(buf));
        close(fd);
        if (n == (ssize_t)sizeof(buf)) {
            for (int i = 0; i < (int)sizeof(buf); i++) {
                snprintf(dst + i * 2, 3, "%02x", buf[i]);
            }
            dst[SESSION_ID_LEN] = '\0';
            return;
        }
    }
    /* fallback: time + counter based (less secure, but functional) */
    static int counter = 0;
    snprintf(dst, SESSION_ID_LEN + 1, "%08lx%08x%08lx%08x",
             (unsigned long)time(NULL), counter++,
             (unsigned long)time(NULL) ^ 0xDEADBEEF, rand());
}

/* Find session by ID. Returns index or -1. */
static int find_session(const char *id) {
    if (!id || !*id) return -1;
    time_t now = time(NULL);
    for (int i = 0; i < g_session_count; i++) {
        if (g_sessions[i].active && strcmp(g_sessions[i].id, id) == 0) {
            /* check expiry */
            if (g_max_age > 0 && (now - g_sessions[i].last_access) > g_max_age) {
                g_sessions[i].active = 0; /* expired */
                return -1;
            }
            return i;
        }
    }
    return -1;
}

/* ── configuration ───────────────────────────────────────────────── */

void aria_session_set_max_age(int32_t seconds) {
    g_max_age = seconds;
}

void aria_session_set_cookie_name(const char *name) {
    if (name && *name) {
        snprintf(g_cookie_name, MAX_KEY_LEN, "%s", name);
    }
}

const char *aria_session_get_cookie_name(void) {
    return g_cookie_name;
}

/* ── session lifecycle ───────────────────────────────────────────── */

/*
 * Create a new session. Returns the session ID string.
 * Sets it as the current session.
 */
const char *aria_session_create(void) {
    /* find a slot */
    int idx = -1;
    for (int i = 0; i < g_session_count; i++) {
        if (!g_sessions[i].active) { idx = i; break; }
    }
    if (idx < 0) {
        if (g_session_count >= MAX_SESSIONS) {
            /* evict oldest */
            int oldest = 0;
            for (int i = 1; i < g_session_count; i++) {
                if (g_sessions[i].last_access < g_sessions[oldest].last_access) oldest = i;
            }
            idx = oldest;
        } else {
            idx = g_session_count++;
        }
    }

    Session *s = &g_sessions[idx];
    generate_session_id(s->id);
    s->var_count   = 0;
    s->created     = time(NULL);
    s->last_access = s->created;
    s->active      = 1;
    memset(s->keys, 0, sizeof(s->keys));
    memset(s->vals, 0, sizeof(s->vals));

    g_current = idx;
    return s->id;
}

/*
 * Load (resume) a session by ID.
 * Returns 1 if found and active, 0 if expired/not found.
 */
int32_t aria_session_load(const char *id) {
    int idx = find_session(id);
    if (idx < 0) {
        g_current = -1;
        return 0;
    }
    g_sessions[idx].last_access = time(NULL);
    g_current = idx;
    return 1;
}

/* Destroy the current session. */
void aria_session_destroy(void) {
    if (g_current >= 0 && g_current < g_session_count) {
        g_sessions[g_current].active = 0;
    }
    g_current = -1;
}

/* Destroy a session by ID. */
void aria_session_destroy_id(const char *id) {
    int idx = find_session(id);
    if (idx >= 0) {
        g_sessions[idx].active = 0;
        if (g_current == idx) g_current = -1;
    }
}

/* Get current session ID. Returns "" if none. */
const char *aria_session_id(void) {
    if (g_current < 0 || !g_sessions[g_current].active) return "";
    return g_sessions[g_current].id;
}

/* ── session data access ─────────────────────────────────────────── */

/* Set a session variable (creates or updates). */
void aria_session_set(const char *key, const char *value) {
    if (g_current < 0 || !key || !*key) return;
    Session *s = &g_sessions[g_current];

    /* update existing */
    for (int i = 0; i < s->var_count; i++) {
        if (strcmp(s->keys[i], key) == 0) {
            snprintf(s->vals[i], MAX_VAL_LEN, "%s", value ? value : "");
            return;
        }
    }
    /* add new */
    if (s->var_count < MAX_SESSION_VARS) {
        snprintf(s->keys[s->var_count], MAX_KEY_LEN, "%s", key);
        snprintf(s->vals[s->var_count], MAX_VAL_LEN, "%s", value ? value : "");
        s->var_count++;
    }
}

/* Get a session variable. Returns "" if not found. */
const char *aria_session_get(const char *key) {
    if (g_current < 0 || !key) return "";
    Session *s = &g_sessions[g_current];
    for (int i = 0; i < s->var_count; i++) {
        if (strcmp(s->keys[i], key) == 0) return s->vals[i];
    }
    return "";
}

/* Check if a session variable exists. */
int32_t aria_session_has(const char *key) {
    if (g_current < 0 || !key) return 0;
    Session *s = &g_sessions[g_current];
    for (int i = 0; i < s->var_count; i++) {
        if (strcmp(s->keys[i], key) == 0) return 1;
    }
    return 0;
}

/* Remove a session variable. */
void aria_session_remove(const char *key) {
    if (g_current < 0 || !key) return;
    Session *s = &g_sessions[g_current];
    for (int i = 0; i < s->var_count; i++) {
        if (strcmp(s->keys[i], key) == 0) {
            /* shift remaining */
            for (int j = i; j < s->var_count - 1; j++) {
                memcpy(s->keys[j], s->keys[j+1], MAX_KEY_LEN);
                memcpy(s->vals[j], s->vals[j+1], MAX_VAL_LEN);
            }
            s->var_count--;
            return;
        }
    }
}

/* Get count of session variables. */
int32_t aria_session_var_count(void) {
    if (g_current < 0) return 0;
    return (int32_t)g_sessions[g_current].var_count;
}

/* Get total active session count. */
int32_t aria_session_count(void) {
    int count = 0;
    time_t now = time(NULL);
    for (int i = 0; i < g_session_count; i++) {
        if (g_sessions[i].active) {
            if (g_max_age > 0 && (now - g_sessions[i].last_access) > g_max_age) {
                g_sessions[i].active = 0;
            } else {
                count++;
            }
        }
    }
    return (int32_t)count;
}

/* ── cookie header helpers ───────────────────────────────────────── */

/*
 * Generate a Set-Cookie header for the current session.
 * Returns e.g. "Set-Cookie: aria_sid=abc123...; Path=/; HttpOnly; SameSite=Lax\r\n"
 */
const char *aria_session_cookie_header(void) {
    g_output[0] = '\0';
    if (g_current < 0 || !g_sessions[g_current].active) return g_output;

    snprintf(g_output, MAX_OUTPUT,
             "Set-Cookie: %s=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=%d\r\n",
             g_cookie_name, g_sessions[g_current].id, g_max_age);
    return g_output;
}

/*
 * Generate a Set-Cookie header to clear/delete the session cookie.
 */
const char *aria_session_clear_cookie(void) {
    snprintf(g_output, MAX_OUTPUT,
             "Set-Cookie: %s=; Path=/; Max-Age=0\r\n", g_cookie_name);
    return g_output;
}

/* Reset all sessions (for testing). */
void aria_session_reset(void) {
    for (int i = 0; i < g_session_count; i++) {
        g_sessions[i].active = 0;
    }
    g_session_count = 0;
    g_current = -1;
    g_max_age = 3600;
    snprintf(g_cookie_name, MAX_KEY_LEN, "aria_sid");
}

/* ── test helpers ────────────────────────────────────────────────── */

void aria_session_assert_str_eq(const char *label, const char *expected, const char *actual) {
    if (strcmp(expected, actual) == 0) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected: \"%s\"\n    got:      \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_session_assert_int_eq(const char *label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected %d, got %d\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_session_assert_contains(const char *label, const char *expected, const char *actual) {
    if (strstr(actual, expected) != NULL) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected to contain: \"%s\"\n    in: \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_session_assert_not_empty(const char *label, const char *actual) {
    if (actual && actual[0] != '\0') {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected non-empty string\n", label);
        g_tests_failed++;
    }
}

void aria_session_test_summary(void) {
    printf("\n--- aria-session: %d passed, %d failed ---\n",
           g_tests_passed, g_tests_failed);
    g_tests_passed = 0;
    g_tests_failed = 0;
}
