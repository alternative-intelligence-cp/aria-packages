/*
 * aria_rate_limit_shim.c — Token bucket rate limiting for Aria HTTP servers
 *
 * Supports:
 *   - Per-client rate limiting by key (IP address, user ID, etc.)
 *   - Token bucket algorithm (configurable max tokens + refill rate)
 *   - Rate limit headers (X-RateLimit-Limit, X-RateLimit-Remaining, Retry-After)
 *   - Global configuration
 *
 * Thread-safety: single-threaded (global state).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ── limits ──────────────────────────────────────────────────────── */
#define MAX_CLIENTS        1024
#define MAX_KEY_LEN        256
#define MAX_OUTPUT         512

/* ── client bucket ───────────────────────────────────────────────── */
typedef struct {
    char     key[MAX_KEY_LEN];
    double   tokens;
    time_t   last_refill;
    int      active;
} Bucket;

static Bucket  g_buckets[MAX_CLIENTS];
static int     g_bucket_count = 0;

/* configuration */
static int     g_max_tokens    = 100;    /* max tokens per client */
static double  g_refill_rate   = 10.0;   /* tokens per second */
static int     g_window        = 60;     /* window in seconds (for headers) */

/* output buffer */
static char    g_output[MAX_OUTPUT] = "";

/* test tracking */
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ── helpers ─────────────────────────────────────────────────────── */

/* Find or create a bucket for the given key */
static Bucket *get_bucket(const char *key) {
    if (!key || !*key) return NULL;

    /* find existing */
    for (int i = 0; i < g_bucket_count; i++) {
        if (g_buckets[i].active && strcmp(g_buckets[i].key, key) == 0) {
            return &g_buckets[i];
        }
    }

    /* find inactive slot */
    for (int i = 0; i < g_bucket_count; i++) {
        if (!g_buckets[i].active) {
            snprintf(g_buckets[i].key, MAX_KEY_LEN, "%s", key);
            g_buckets[i].tokens = (double)g_max_tokens;
            g_buckets[i].last_refill = time(NULL);
            g_buckets[i].active = 1;
            return &g_buckets[i];
        }
    }

    /* allocate new */
    if (g_bucket_count < MAX_CLIENTS) {
        Bucket *b = &g_buckets[g_bucket_count++];
        snprintf(b->key, MAX_KEY_LEN, "%s", key);
        b->tokens = (double)g_max_tokens;
        b->last_refill = time(NULL);
        b->active = 1;
        return b;
    }

    /* evict oldest */
    int oldest = 0;
    for (int i = 1; i < g_bucket_count; i++) {
        if (g_buckets[i].last_refill < g_buckets[oldest].last_refill) oldest = i;
    }
    Bucket *b = &g_buckets[oldest];
    snprintf(b->key, MAX_KEY_LEN, "%s", key);
    b->tokens = (double)g_max_tokens;
    b->last_refill = time(NULL);
    b->active = 1;
    return b;
}

/* Refill tokens based on elapsed time */
static void refill(Bucket *b) {
    time_t now = time(NULL);
    double elapsed = difftime(now, b->last_refill);
    if (elapsed > 0) {
        b->tokens += elapsed * g_refill_rate;
        if (b->tokens > (double)g_max_tokens) {
            b->tokens = (double)g_max_tokens;
        }
        b->last_refill = now;
    }
}

/* ── configuration API ───────────────────────────────────────────── */

void aria_rate_limit_configure(int32_t max_tokens, int32_t refill_per_sec, int32_t window_sec) {
    g_max_tokens  = max_tokens > 0 ? max_tokens : 100;
    g_refill_rate = refill_per_sec > 0 ? (double)refill_per_sec : 10.0;
    g_window      = window_sec > 0 ? window_sec : 60;
}

void aria_rate_limit_set_max(int32_t max_tokens) {
    g_max_tokens = max_tokens > 0 ? max_tokens : 100;
}

void aria_rate_limit_set_rate(int32_t tokens_per_sec) {
    g_refill_rate = tokens_per_sec > 0 ? (double)tokens_per_sec : 10.0;
}

/* ── rate checking API ───────────────────────────────────────────── */

/*
 * Check if a request from the given key is allowed.
 * Consumes 1 token. Returns 1 if allowed, 0 if rate limited.
 */
int32_t aria_rate_limit_check(const char *key) {
    Bucket *b = get_bucket(key);
    if (!b) return 1; /* fail open on error */

    refill(b);

    if (b->tokens >= 1.0) {
        b->tokens -= 1.0;
        return 1;
    }
    return 0;
}

/*
 * Check with a custom cost (e.g., expensive operations use more tokens).
 */
int32_t aria_rate_limit_check_cost(const char *key, int32_t cost) {
    Bucket *b = get_bucket(key);
    if (!b) return 1;

    refill(b);

    double c = (double)(cost > 0 ? cost : 1);
    if (b->tokens >= c) {
        b->tokens -= c;
        return 1;
    }
    return 0;
}

/* Get remaining tokens for a key (after refill). */
int32_t aria_rate_limit_remaining(const char *key) {
    Bucket *b = get_bucket(key);
    if (!b) return g_max_tokens;
    refill(b);
    return (int32_t)b->tokens;
}

/* Get seconds until next token is available (for Retry-After). */
int32_t aria_rate_limit_retry_after(const char *key) {
    Bucket *b = get_bucket(key);
    if (!b) return 0;
    refill(b);
    if (b->tokens >= 1.0) return 0;
    double needed = 1.0 - b->tokens;
    int secs = (int)(needed / g_refill_rate) + 1;
    return (int32_t)secs;
}

/*
 * Generate rate limit headers for the response.
 * Returns a string like:
 *   "X-RateLimit-Limit: 100\r\nX-RateLimit-Remaining: 42\r\n"
 * If rate limited, adds Retry-After.
 */
const char *aria_rate_limit_headers(const char *key) {
    Bucket *b = get_bucket(key);
    if (!b) {
        snprintf(g_output, MAX_OUTPUT,
                 "X-RateLimit-Limit: %d\r\nX-RateLimit-Remaining: %d\r\n",
                 g_max_tokens, g_max_tokens);
        return g_output;
    }

    refill(b);
    int remaining = (int)b->tokens;
    int pos = 0;
    pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                    "X-RateLimit-Limit: %d\r\n", g_max_tokens);
    pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                    "X-RateLimit-Remaining: %d\r\n", remaining);

    if (remaining <= 0) {
        double needed = 1.0 - b->tokens;
        int retry = (int)(needed / g_refill_rate) + 1;
        pos += snprintf(g_output + pos, MAX_OUTPUT - pos,
                        "Retry-After: %d\r\n", retry);
    }

    return g_output;
}

/* Reset all buckets (for testing). */
void aria_rate_limit_reset(void) {
    for (int i = 0; i < g_bucket_count; i++) {
        g_buckets[i].active = 0;
    }
    g_bucket_count = 0;
    g_max_tokens   = 100;
    g_refill_rate  = 10.0;
    g_window       = 60;
}

/* Manually set tokens for testing (bypass refill). */
void aria_rate_limit_test_set_tokens(const char *key, int32_t tokens) {
    Bucket *b = get_bucket(key);
    if (b) {
        b->tokens = (double)tokens;
        b->last_refill = time(NULL);
    }
}

/* ── test helpers ────────────────────────────────────────────────── */

void aria_rate_limit_assert_str_eq(const char *label, const char *expected, const char *actual) {
    if (strcmp(expected, actual) == 0) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected: \"%s\"\n    got:      \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_rate_limit_assert_int_eq(const char *label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected %d, got %d\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_rate_limit_assert_contains(const char *label, const char *expected, const char *actual) {
    if (strstr(actual, expected) != NULL) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected to contain: \"%s\"\n    in: \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_rate_limit_test_summary(void) {
    printf("\n--- aria-rate-limit: %d passed, %d failed ---\n",
           g_tests_passed, g_tests_failed);
    g_tests_passed = 0;
    g_tests_failed = 0;
}
