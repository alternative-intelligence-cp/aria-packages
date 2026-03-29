/*  aria_ftp_shim.c — FTP URL builder and request composer for Aria FFI
 *
 *  Handle-based FTP session builder. Constructs FTP URLs and manages
 *  session state (host, port, credentials, paths) without requiring
 *  a live FTP server for testing.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_ftp_shim.so aria_ftp_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SESSIONS      32
#define MAX_FIELD_LEN     512
#define MAX_PATH_LEN      2048
#define MAX_DIR_ENTRIES   128
#define MAX_ENTRY_LEN     256

/* ── session storage ─────────────────────────────────────────────── */

typedef struct {
    char host[MAX_FIELD_LEN];
    int32_t port;
    char username[MAX_FIELD_LEN];
    char password[MAX_FIELD_LEN];
    char cwd[MAX_PATH_LEN];
    int32_t passive_mode;   /* 1=passive (default), 0=active */
    int32_t binary_mode;    /* 1=binary (default), 0=ascii */
    int32_t active;
} FtpSession;

static FtpSession g_sessions[MAX_SESSIONS];
static int32_t    g_init = 0;

/* simulated directory listing */
static char g_dir_entries[MAX_DIR_ENTRIES][MAX_ENTRY_LEN];
static int32_t g_dir_count = 0;

/* result buffer */
static char g_result_buf[MAX_PATH_LEN * 2];

static void ensure_init(void) {
    if (!g_init) {
        memset(g_sessions, 0, sizeof(g_sessions));
        g_init = 1;
    }
}

/* ── session lifecycle ───────────────────────────────────────────── */

int64_t aria_ftp_session_create(const char *host, int32_t port) {
    ensure_init();
    if (!host || !*host) return -1;
    if (port <= 0 || port > 65535) return -1;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!g_sessions[i].active) {
            memset(&g_sessions[i], 0, sizeof(FtpSession));
            strncpy(g_sessions[i].host, host, MAX_FIELD_LEN - 1);
            g_sessions[i].port = port;
            g_sessions[i].passive_mode = 1;
            g_sessions[i].binary_mode = 1;
            strcpy(g_sessions[i].cwd, "/");
            g_sessions[i].active = 1;
            return (int64_t)i;
        }
    }
    return -1;
}

void aria_ftp_session_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS) return;
    g_sessions[handle].active = 0;
}

/* ── authentication ──────────────────────────────────────────────── */

int32_t aria_ftp_set_credentials(int64_t handle, const char *user, const char *pass) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    if (!user) return -1;
    strncpy(g_sessions[handle].username, user, MAX_FIELD_LEN - 1);
    if (pass) strncpy(g_sessions[handle].password, pass, MAX_FIELD_LEN - 1);
    return 0;
}

/* ── mode settings ───────────────────────────────────────────────── */

int32_t aria_ftp_set_passive(int64_t handle, int32_t passive) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    g_sessions[handle].passive_mode = passive ? 1 : 0;
    return 0;
}

int32_t aria_ftp_set_binary(int64_t handle, int32_t binary) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    g_sessions[handle].binary_mode = binary ? 1 : 0;
    return 0;
}

int32_t aria_ftp_is_passive(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    return g_sessions[handle].passive_mode;
}

int32_t aria_ftp_is_binary(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    return g_sessions[handle].binary_mode;
}

/* ── directory navigation ────────────────────────────────────────── */

int32_t aria_ftp_cwd(int64_t handle, const char *path) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    if (!path || !*path) return -1;

    if (path[0] == '/') {
        strncpy(g_sessions[handle].cwd, path, MAX_PATH_LEN - 1);
    } else {
        size_t cwdlen = strlen(g_sessions[handle].cwd);
        if (cwdlen > 1) {
            snprintf(g_sessions[handle].cwd + cwdlen,
                     MAX_PATH_LEN - cwdlen, "/%s", path);
        } else {
            snprintf(g_sessions[handle].cwd, MAX_PATH_LEN, "/%s", path);
        }
    }
    return 0;
}

const char *aria_ftp_pwd(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return "";
    return g_sessions[handle].cwd;
}

int32_t aria_ftp_cdup(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    char *slash = strrchr(g_sessions[handle].cwd, '/');
    if (slash && slash != g_sessions[handle].cwd) {
        *slash = '\0';
    } else {
        strcpy(g_sessions[handle].cwd, "/");
    }
    return 0;
}

/* ── URL builder ─────────────────────────────────────────────────── */

/* Build FTP URL for a file path. */
const char *aria_ftp_build_url(int64_t handle, const char *filepath) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return "";

    FtpSession *s = &g_sessions[handle];
    int pos = 0;

    pos += snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos, "ftp://");

    if (s->username[0]) {
        pos += snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos,
                        "%s", s->username);
        if (s->password[0]) {
            pos += snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos,
                            ":%s", s->password);
        }
        pos += snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos, "@");
    }

    pos += snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos,
                    "%s", s->host);

    if (s->port != 21) {
        pos += snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos,
                        ":%d", s->port);
    }

    if (filepath && *filepath) {
        if (filepath[0] != '/') {
            pos += snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos, "/");
        }
        snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos, "%s", filepath);
    } else {
        snprintf(g_result_buf + pos, sizeof(g_result_buf) - pos, "/");
    }

    return g_result_buf;
}

/* ── getters ─────────────────────────────────────────────────────── */

const char *aria_ftp_get_host(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return "";
    return g_sessions[handle].host;
}

int32_t aria_ftp_get_port(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return -1;
    return g_sessions[handle].port;
}

const char *aria_ftp_get_username(int64_t handle) {
    if (handle < 0 || handle >= MAX_SESSIONS || !g_sessions[handle].active) return "";
    return g_sessions[handle].username;
}

/* ── test helpers ────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_ftp_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_ftp_assert_str_eq(const char *actual, const char *expected, const char *msg) {
    if (actual && expected && strcmp(actual, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected '%s', got '%s')\n", msg,
               expected ? expected : "(null)", actual ? actual : "(null)");
    }
}

void aria_ftp_assert_str_contains(const char *haystack, const char *needle, const char *msg) {
    if (haystack && needle && strstr(haystack, needle)) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (needle '%s' not in '%s')\n", msg,
               needle ? needle : "(null)", haystack ? haystack : "(null)");
    }
}

void aria_ftp_test_summary(void) {
    printf("\n=== aria-ftp test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
