/*  aria_mock_shim.c — Test mocking framework for Aria FFI
 *
 *  Handle-based mock functions: create mock, set return values,
 *  record calls with arguments, verify call count and arguments.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_mock_shim.so aria_mock_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_MOCKS       32
#define MAX_CALLS       256
#define MAX_ARGS        8
#define MAX_NAME_LEN    128
#define MAX_STR_LEN     256

/* ── types ───────────────────────────────────────────────────────── */

typedef struct {
    int64_t  int_args[MAX_ARGS];
    char     str_args[MAX_ARGS][MAX_STR_LEN];
    int32_t  arg_count;
} CallRecord;

typedef struct {
    char        name[MAX_NAME_LEN];
    int         in_use;

    /* call recording */
    CallRecord  calls[MAX_CALLS];
    int32_t     call_count;

    /* return value stubbing */
    int64_t     return_int;
    char        return_str[MAX_STR_LEN];
    int32_t     return_type;   /* 0=none, 1=int, 2=string */

    /* expectations */
    int32_t     expect_calls;       /* -1 = no expectation */
    int32_t     expect_min_calls;   /* -1 = no min */
    int32_t     expect_max_calls;   /* -1 = no max */
} MockFunc;

static MockFunc g_mocks[MAX_MOCKS];

/* ── AriaString return ABI ───────────────────────────────────────── */

typedef struct { const char *data; int64_t length; } AriaString;

static char g_str_buf[MAX_STR_LEN];
static AriaString g_str_ret;

static AriaString make_str(const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (len >= MAX_STR_LEN) len = MAX_STR_LEN - 1;
    memcpy(g_str_buf, s ? s : "", len);
    g_str_buf[len] = '\0';
    g_str_ret.data = g_str_buf;
    g_str_ret.length = (int64_t)len;
    return g_str_ret;
}

/* ── lifecycle ───────────────────────────────────────────────────── */

int32_t aria_mock_create(const char *name) {
    for (int32_t i = 0; i < MAX_MOCKS; i++) {
        if (!g_mocks[i].in_use) {
            memset(&g_mocks[i], 0, sizeof(MockFunc));
            g_mocks[i].in_use = 1;
            g_mocks[i].expect_calls = -1;
            g_mocks[i].expect_min_calls = -1;
            g_mocks[i].expect_max_calls = -1;
            if (name) {
                strncpy(g_mocks[i].name, name, MAX_NAME_LEN - 1);
                g_mocks[i].name[MAX_NAME_LEN - 1] = '\0';
            }
            return i;
        }
    }
    return -1;
}

void aria_mock_destroy(int32_t h) {
    if (h < 0 || h >= MAX_MOCKS) return;
    g_mocks[h].in_use = 0;
}

void aria_mock_reset(int32_t h) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return;
    g_mocks[h].call_count = 0;
    g_mocks[h].return_type = 0;
    g_mocks[h].return_int = 0;
    g_mocks[h].return_str[0] = '\0';
}

AriaString aria_mock_get_name(int32_t h) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return make_str("");
    return make_str(g_mocks[h].name);
}

/* ── return value stubbing ───────────────────────────────────────── */

void aria_mock_returns_int(int32_t h, int64_t val) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return;
    g_mocks[h].return_type = 1;
    g_mocks[h].return_int = val;
}

void aria_mock_returns_str(int32_t h, const char *val) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return;
    g_mocks[h].return_type = 2;
    if (val) {
        strncpy(g_mocks[h].return_str, val, MAX_STR_LEN - 1);
        g_mocks[h].return_str[MAX_STR_LEN - 1] = '\0';
    }
}

/* ── call recording ──────────────────────────────────────────────── */

int64_t aria_mock_call(int32_t h) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return 0;
    MockFunc *m = &g_mocks[h];
    if (m->call_count < MAX_CALLS) {
        m->calls[m->call_count].arg_count = 0;
    }
    m->call_count++;
    return m->return_type == 1 ? m->return_int : 0;
}

int64_t aria_mock_call_with_int(int32_t h, int64_t arg) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return 0;
    MockFunc *m = &g_mocks[h];
    if (m->call_count < MAX_CALLS) {
        m->calls[m->call_count].int_args[0] = arg;
        m->calls[m->call_count].arg_count = 1;
    }
    m->call_count++;
    return m->return_type == 1 ? m->return_int : 0;
}

int64_t aria_mock_call_with_str(int32_t h, const char *arg) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return 0;
    MockFunc *m = &g_mocks[h];
    if (m->call_count < MAX_CALLS) {
        if (arg) {
            strncpy(m->calls[m->call_count].str_args[0], arg, MAX_STR_LEN - 1);
            m->calls[m->call_count].str_args[0][MAX_STR_LEN - 1] = '\0';
        }
        m->calls[m->call_count].arg_count = 1;
    }
    m->call_count++;
    return m->return_type == 1 ? m->return_int : 0;
}

/* ── call inspection ─────────────────────────────────────────────── */

int32_t aria_mock_call_count(int32_t h) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return 0;
    return g_mocks[h].call_count;
}

int64_t aria_mock_call_arg_int(int32_t h, int32_t call_idx, int32_t arg_idx) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return 0;
    if (call_idx < 0 || call_idx >= g_mocks[h].call_count) return 0;
    if (call_idx >= MAX_CALLS) return 0;
    if (arg_idx < 0 || arg_idx >= MAX_ARGS) return 0;
    return g_mocks[h].calls[call_idx].int_args[arg_idx];
}

AriaString aria_mock_call_arg_str(int32_t h, int32_t call_idx, int32_t arg_idx) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return make_str("");
    if (call_idx < 0 || call_idx >= g_mocks[h].call_count) return make_str("");
    if (call_idx >= MAX_CALLS) return make_str("");
    if (arg_idx < 0 || arg_idx >= MAX_ARGS) return make_str("");
    return make_str(g_mocks[h].calls[call_idx].str_args[arg_idx]);
}

/* ── expectations ────────────────────────────────────────────────── */

void aria_mock_expect_calls(int32_t h, int32_t n) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return;
    g_mocks[h].expect_calls = n;
}

void aria_mock_expect_min(int32_t h, int32_t n) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return;
    g_mocks[h].expect_min_calls = n;
}

void aria_mock_expect_max(int32_t h, int32_t n) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return;
    g_mocks[h].expect_max_calls = n;
}

int32_t aria_mock_verify(int32_t h) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return 0;
    MockFunc *m = &g_mocks[h];
    if (m->expect_calls >= 0 && m->call_count != m->expect_calls) return 0;
    if (m->expect_min_calls >= 0 && m->call_count < m->expect_min_calls) return 0;
    if (m->expect_max_calls >= 0 && m->call_count > m->expect_max_calls) return 0;
    return 1;
}

int32_t aria_mock_was_called(int32_t h) {
    if (h < 0 || h >= MAX_MOCKS || !g_mocks[h].in_use) return 0;
    return g_mocks[h].call_count > 0 ? 1 : 0;
}

/* ================================================================= */
/* ── test harness ────────────────────────────────────────────────── */
/* ================================================================= */

static int t_pass = 0, t_fail = 0;

static void assert_int_eq(int32_t a, int32_t b, const char *msg) {
    if (a == b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %d, expected %d\n", msg, a, b); }
}

static void assert_i64_eq(int64_t a, int64_t b, const char *msg) {
    if (a == b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %ld, expected %ld\n", msg, a, b); }
}

static void assert_str_eq(const char *a, const char *b, const char *msg) {
    if (a && b && strcmp(a, b) == 0) { t_pass++; printf("  PASS: %s\n", msg); }
    else { t_fail++; printf("  FAIL: %s — got '%s', expected '%s'\n", msg, a ? a : "(null)", b ? b : "(null)"); }
}

int32_t aria_test_mock_run(void) {
    printf("=== aria-mock tests ===\n");

    /* basic create */
    int32_t m = aria_mock_create("my_func");
    assert_int_eq(m >= 0 ? 1 : 0, 1, "create returns valid handle");
    AriaString nm = aria_mock_get_name(m);
    assert_str_eq(nm.data, "my_func", "name = my_func");

    /* initial state */
    assert_int_eq(aria_mock_call_count(m), 0, "initial call count = 0");
    assert_int_eq(aria_mock_was_called(m), 0, "was_called = 0 initially");

    /* call recording */
    aria_mock_call(m);
    assert_int_eq(aria_mock_call_count(m), 1, "call count = 1 after one call");
    assert_int_eq(aria_mock_was_called(m), 1, "was_called = 1");

    aria_mock_call(m);
    aria_mock_call(m);
    assert_int_eq(aria_mock_call_count(m), 3, "call count = 3 after three calls");

    /* return value stubbing */
    int32_t m2 = aria_mock_create("returns_42");
    aria_mock_returns_int(m2, 42);
    int64_t ret = aria_mock_call(m2);
    assert_i64_eq(ret, 42, "stubbed return = 42");

    /* call with int arg */
    int32_t m3 = aria_mock_create("int_func");
    aria_mock_call_with_int(m3, 100);
    aria_mock_call_with_int(m3, 200);
    assert_int_eq(aria_mock_call_count(m3), 2, "int_func call count = 2");
    assert_i64_eq(aria_mock_call_arg_int(m3, 0, 0), 100, "call 0 arg 0 = 100");
    assert_i64_eq(aria_mock_call_arg_int(m3, 1, 0), 200, "call 1 arg 0 = 200");

    /* call with string arg */
    int32_t m4 = aria_mock_create("str_func");
    aria_mock_call_with_str(m4, "hello");
    aria_mock_call_with_str(m4, "world");
    AriaString a0 = aria_mock_call_arg_str(m4, 0, 0);
    assert_str_eq(a0.data, "hello", "str call 0 arg 0 = hello");
    AriaString a1 = aria_mock_call_arg_str(m4, 1, 0);
    assert_str_eq(a1.data, "world", "str call 1 arg 0 = world");

    /* expectations: exact count */
    int32_t m5 = aria_mock_create("expect_exact");
    aria_mock_expect_calls(m5, 2);
    aria_mock_call(m5);
    aria_mock_call(m5);
    assert_int_eq(aria_mock_verify(m5), 1, "verify passes: called 2, expected 2");

    aria_mock_call(m5);
    assert_int_eq(aria_mock_verify(m5), 0, "verify fails: called 3, expected 2");

    /* expectations: min/max */
    int32_t m6 = aria_mock_create("expect_range");
    aria_mock_expect_min(m6, 1);
    aria_mock_expect_max(m6, 3);
    assert_int_eq(aria_mock_verify(m6), 0, "verify fails: called 0, min 1");
    aria_mock_call(m6);
    assert_int_eq(aria_mock_verify(m6), 1, "verify passes: called 1, min 1, max 3");
    aria_mock_call(m6); aria_mock_call(m6); aria_mock_call(m6);
    assert_int_eq(aria_mock_verify(m6), 0, "verify fails: called 4, max 3");

    /* reset */
    aria_mock_reset(m);
    assert_int_eq(aria_mock_call_count(m), 0, "call count = 0 after reset");
    assert_int_eq(aria_mock_was_called(m), 0, "was_called = 0 after reset");

    /* cleanup */
    aria_mock_destroy(m);
    aria_mock_destroy(m2);
    aria_mock_destroy(m3);
    aria_mock_destroy(m4);
    aria_mock_destroy(m5);
    aria_mock_destroy(m6);

    printf("\nResults: %d passed, %d failed (of %d)\n", t_pass, t_fail, t_pass + t_fail);
    return t_fail;
}

#ifdef ARIA_MOCK_TEST
int main(void) { return aria_test_mock_run(); }
#endif
