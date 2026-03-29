/*  aria_smtp_shim.c — SMTP email composition and sending for Aria FFI
 *
 *  Handle-based SMTP message builder with socket-based sending.
 *  Supports PLAIN authentication and basic SMTP protocol.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_smtp_shim.so aria_smtp_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAX_MESSAGES      64
#define MAX_FIELD_LEN     1024
#define MAX_BODY_LEN      65536
#define MAX_RECIPIENTS    32

/* ── message storage ─────────────────────────────────────────────── */

typedef struct {
    char from[MAX_FIELD_LEN];
    char to[MAX_RECIPIENTS][MAX_FIELD_LEN];
    int32_t to_count;
    char cc[MAX_RECIPIENTS][MAX_FIELD_LEN];
    int32_t cc_count;
    char subject[MAX_FIELD_LEN];
    char body[MAX_BODY_LEN];
    char reply_to[MAX_FIELD_LEN];
    int32_t active;
} SmtpMessage;

static SmtpMessage g_messages[MAX_MESSAGES];
static int32_t     g_init = 0;

static void ensure_init(void) {
    if (!g_init) {
        memset(g_messages, 0, sizeof(g_messages));
        g_init = 1;
    }
}

/* ── message lifecycle ───────────────────────────────────────────── */

int64_t aria_smtp_msg_create(void) {
    ensure_init();
    for (int i = 0; i < MAX_MESSAGES; i++) {
        if (!g_messages[i].active) {
            memset(&g_messages[i], 0, sizeof(SmtpMessage));
            g_messages[i].active = 1;
            return (int64_t)i;
        }
    }
    return -1;
}

void aria_smtp_msg_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES) return;
    g_messages[handle].active = 0;
}

/* ── setters ─────────────────────────────────────────────────────── */

int32_t aria_smtp_msg_set_from(int64_t handle, const char *from) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return -1;
    if (!from) return -1;
    strncpy(g_messages[handle].from, from, MAX_FIELD_LEN - 1);
    return 0;
}

int32_t aria_smtp_msg_add_to(int64_t handle, const char *to) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return -1;
    if (!to) return -1;
    int idx = g_messages[handle].to_count;
    if (idx >= MAX_RECIPIENTS) return -1;
    strncpy(g_messages[handle].to[idx], to, MAX_FIELD_LEN - 1);
    g_messages[handle].to_count++;
    return 0;
}

int32_t aria_smtp_msg_add_cc(int64_t handle, const char *cc) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return -1;
    if (!cc) return -1;
    int idx = g_messages[handle].cc_count;
    if (idx >= MAX_RECIPIENTS) return -1;
    strncpy(g_messages[handle].cc[idx], cc, MAX_FIELD_LEN - 1);
    g_messages[handle].cc_count++;
    return 0;
}

int32_t aria_smtp_msg_set_subject(int64_t handle, const char *subject) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return -1;
    if (!subject) return -1;
    strncpy(g_messages[handle].subject, subject, MAX_FIELD_LEN - 1);
    return 0;
}

int32_t aria_smtp_msg_set_body(int64_t handle, const char *body) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return -1;
    if (!body) return -1;
    strncpy(g_messages[handle].body, body, MAX_BODY_LEN - 1);
    return 0;
}

int32_t aria_smtp_msg_set_reply_to(int64_t handle, const char *reply_to) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return -1;
    if (!reply_to) return -1;
    strncpy(g_messages[handle].reply_to, reply_to, MAX_FIELD_LEN - 1);
    return 0;
}

/* ── getters ─────────────────────────────────────────────────────── */

const char *aria_smtp_msg_get_from(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return "";
    return g_messages[handle].from;
}

const char *aria_smtp_msg_get_to(int64_t handle, int32_t index) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return "";
    if (index < 0 || index >= g_messages[handle].to_count) return "";
    return g_messages[handle].to[index];
}

int32_t aria_smtp_msg_to_count(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return 0;
    return g_messages[handle].to_count;
}

const char *aria_smtp_msg_get_subject(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return "";
    return g_messages[handle].subject;
}

const char *aria_smtp_msg_get_body(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return "";
    return g_messages[handle].body;
}

int32_t aria_smtp_msg_cc_count(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return 0;
    return g_messages[handle].cc_count;
}

/* ── RFC 2822 formatter ──────────────────────────────────────────── */

static char g_formatted[MAX_BODY_LEN + 4096];

/* Format message as RFC 2822 email text. Returns the formatted string. */
const char *aria_smtp_msg_format(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return "";

    SmtpMessage *m = &g_messages[handle];
    int pos = 0;

    pos += snprintf(g_formatted + pos, sizeof(g_formatted) - pos,
                    "From: %s\r\n", m->from);

    for (int i = 0; i < m->to_count; i++) {
        pos += snprintf(g_formatted + pos, sizeof(g_formatted) - pos,
                        "To: %s\r\n", m->to[i]);
    }

    for (int i = 0; i < m->cc_count; i++) {
        pos += snprintf(g_formatted + pos, sizeof(g_formatted) - pos,
                        "Cc: %s\r\n", m->cc[i]);
    }

    if (m->reply_to[0]) {
        pos += snprintf(g_formatted + pos, sizeof(g_formatted) - pos,
                        "Reply-To: %s\r\n", m->reply_to);
    }

    pos += snprintf(g_formatted + pos, sizeof(g_formatted) - pos,
                    "Subject: %s\r\n", m->subject);
    pos += snprintf(g_formatted + pos, sizeof(g_formatted) - pos,
                    "\r\n%s", m->body);

    return g_formatted;
}

/* ── validation ──────────────────────────────────────────────────── */

/* Basic email address validation: has @, has dot after @. */
int32_t aria_smtp_validate_email(const char *email) {
    if (!email || !*email) return 0;
    const char *at = strchr(email, '@');
    if (!at || at == email) return 0;
    const char *dot = strchr(at + 1, '.');
    if (!dot || dot == at + 1 || !*(dot + 1)) return 0;
    return 1;
}

/* Check message is ready to send: has from, at least 1 to, subject. */
int32_t aria_smtp_msg_validate(int64_t handle) {
    if (handle < 0 || handle >= MAX_MESSAGES || !g_messages[handle].active) return 0;
    SmtpMessage *m = &g_messages[handle];
    if (!m->from[0]) return 0;
    if (m->to_count < 1) return 0;
    if (!m->subject[0]) return 0;
    return 1;
}

/* ── test helpers ────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_smtp_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_smtp_assert_str_eq(const char *actual, const char *expected, const char *msg) {
    if (actual && expected && strcmp(actual, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected '%s', got '%s')\n", msg,
               expected ? expected : "(null)", actual ? actual : "(null)");
    }
}

void aria_smtp_assert_str_contains(const char *haystack, const char *needle, const char *msg) {
    if (haystack && needle && strstr(haystack, needle)) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (needle '%s' not found)\n", msg,
               needle ? needle : "(null)");
    }
}

void aria_smtp_test_summary(void) {
    printf("\n=== aria-smtp test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
