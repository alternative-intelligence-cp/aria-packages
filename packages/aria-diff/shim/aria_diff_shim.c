/*  aria_diff_shim.c — Text diffing for Aria FFI
 *
 *  Line-by-line diff using the LCS (Longest Common Subsequence) algorithm.
 *  Produces diff hunks: added, removed, and context lines.
 *  Can output unified diff format.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_diff_shim.so aria_diff_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_LINES       1024
#define MAX_LINE_LEN    512
#define MAX_HUNKS       2048
#define MAX_OUTPUT      65536

/* ── line splitting ──────────────────────────────────────────────── */

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LEN];
    int32_t count;
} LineArray;

static LineArray g_lines_a;
static LineArray g_lines_b;

static void split_lines(const char *text, LineArray *out) {
    out->count = 0;
    if (!text) return;

    const char *p = text;
    while (*p && out->count < MAX_LINES) {
        const char *nl = strchr(p, '\n');
        size_t len;
        if (nl) {
            len = nl - p;
        } else {
            len = strlen(p);
        }
        if (len >= MAX_LINE_LEN) len = MAX_LINE_LEN - 1;
        memcpy(out->lines[out->count], p, len);
        out->lines[out->count][len] = '\0';
        out->count++;
        if (nl) p = nl + 1;
        else break;
    }
}

/* ── LCS-based diff ──────────────────────────────────────────────── */

/* Diff entry types */
#define DIFF_CONTEXT  0
#define DIFF_ADDED    1
#define DIFF_REMOVED  2

typedef struct {
    int32_t tag;    /* 0=context, 1=added, 2=removed */
    int32_t index;  /* line index in original (a for removed/context, b for added) */
} DiffEntry;

static DiffEntry g_diff[MAX_HUNKS];
static int32_t   g_diff_count = 0;

/* LCS table (dynamic programming) */
static int32_t g_lcs[MAX_LINES + 1][MAX_LINES + 1];

/* Compute diff between text_a and text_b. Returns number of diff entries. */
int32_t aria_diff_compute(const char *text_a, const char *text_b) {
    split_lines(text_a, &g_lines_a);
    split_lines(text_b, &g_lines_b);

    int32_t m = g_lines_a.count;
    int32_t n = g_lines_b.count;

    /* Build LCS table */
    for (int32_t i = 0; i <= m; i++) g_lcs[i][0] = 0;
    for (int32_t j = 0; j <= n; j++) g_lcs[0][j] = 0;

    for (int32_t i = 1; i <= m; i++) {
        for (int32_t j = 1; j <= n; j++) {
            if (strcmp(g_lines_a.lines[i-1], g_lines_b.lines[j-1]) == 0) {
                g_lcs[i][j] = g_lcs[i-1][j-1] + 1;
            } else {
                g_lcs[i][j] = g_lcs[i-1][j] > g_lcs[i][j-1]
                             ? g_lcs[i-1][j] : g_lcs[i][j-1];
            }
        }
    }

    /* Backtrack to produce diff */
    g_diff_count = 0;

    /* Temporary reverse buffer */
    DiffEntry rev[MAX_HUNKS];
    int32_t rcount = 0;

    int32_t i = m, j = n;
    while (i > 0 || j > 0) {
        if (rcount >= MAX_HUNKS) break;
        if (i > 0 && j > 0 &&
            strcmp(g_lines_a.lines[i-1], g_lines_b.lines[j-1]) == 0) {
            rev[rcount].tag = DIFF_CONTEXT;
            rev[rcount].index = i - 1;
            rcount++;
            i--; j--;
        } else if (j > 0 && (i == 0 || g_lcs[i][j-1] >= g_lcs[i-1][j])) {
            rev[rcount].tag = DIFF_ADDED;
            rev[rcount].index = j - 1;
            rcount++;
            j--;
        } else {
            rev[rcount].tag = DIFF_REMOVED;
            rev[rcount].index = i - 1;
            rcount++;
            i--;
        }
    }

    /* Reverse into g_diff */
    g_diff_count = rcount;
    for (int32_t k = 0; k < rcount; k++) {
        g_diff[k] = rev[rcount - 1 - k];
    }

    return g_diff_count;
}

/* ── accessors ───────────────────────────────────────────────────── */

int32_t aria_diff_count(void) {
    return g_diff_count;
}

/* Get tag at index: 0=context, 1=added, 2=removed, -1=invalid */
int32_t aria_diff_get_tag(int32_t idx) {
    if (idx < 0 || idx >= g_diff_count) return -1;
    return g_diff[idx].tag;
}

/* Get line text at diff index. */
const char *aria_diff_get_line(int32_t idx) {
    if (idx < 0 || idx >= g_diff_count) return "";
    DiffEntry *e = &g_diff[idx];
    if (e->tag == DIFF_ADDED) {
        return g_lines_b.lines[e->index];
    } else {
        return g_lines_a.lines[e->index];
    }
}

/* ── statistics ──────────────────────────────────────────────────── */

int32_t aria_diff_additions(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_diff_count; i++) {
        if (g_diff[i].tag == DIFF_ADDED) count++;
    }
    return count;
}

int32_t aria_diff_deletions(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_diff_count; i++) {
        if (g_diff[i].tag == DIFF_REMOVED) count++;
    }
    return count;
}

int32_t aria_diff_unchanged(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_diff_count; i++) {
        if (g_diff[i].tag == DIFF_CONTEXT) count++;
    }
    return count;
}

/* ── unified format output ───────────────────────────────────────── */

static char g_unified_buf[MAX_OUTPUT];

const char *aria_diff_unified(void) {
    int pos = 0;
    g_unified_buf[0] = '\0';

    for (int32_t i = 0; i < g_diff_count && pos < MAX_OUTPUT - MAX_LINE_LEN - 4; i++) {
        const char *line = aria_diff_get_line(i);
        switch (g_diff[i].tag) {
            case DIFF_CONTEXT:
                pos += snprintf(g_unified_buf + pos, MAX_OUTPUT - pos, " %s\n", line);
                break;
            case DIFF_ADDED:
                pos += snprintf(g_unified_buf + pos, MAX_OUTPUT - pos, "+%s\n", line);
                break;
            case DIFF_REMOVED:
                pos += snprintf(g_unified_buf + pos, MAX_OUTPUT - pos, "-%s\n", line);
                break;
        }
    }
    return g_unified_buf;
}

/* ── test helpers ────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_diff_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_diff_assert_str_eq(const char *actual, const char *expected, const char *msg) {
    if (actual && expected && strcmp(actual, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected '%s', got '%s')\n", msg,
               expected ? expected : "(null)", actual ? actual : "(null)");
    }
}

void aria_diff_assert_str_contains(const char *haystack, const char *needle, const char *msg) {
    if (haystack && needle && strstr(haystack, needle)) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (needle '%s' not found)\n", msg,
               needle ? needle : "(null)");
    }
}

void aria_diff_assert_int_gt(int32_t actual, int32_t threshold, const char *msg) {
    if (actual > threshold) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected >%d, got %d)\n", msg, threshold, actual);
    }
}

void aria_diff_test_summary(void) {
    printf("\n=== aria-diff test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
