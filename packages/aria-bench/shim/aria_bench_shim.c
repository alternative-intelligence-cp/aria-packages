/* aria_bench_shim.c — High-resolution monotonic timer for benchmarking
 *
 * Build:
 *   cc -O2 -shared -fPIC -Wall -o libaria_bench_shim.so aria_bench_shim.c -lrt
 */

#include <time.h>
#include <stdint.h>
#include <stdio.h>

/* ── Timer API ─────────────────────────────────────────────────────── */

int64_t aria_bench_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int64_t aria_bench_elapsed_ns(int64_t start_ns) {
    return aria_bench_now_ns() - start_ns;
}

/* ── Test Assertion Helpers ────────────────────────────────────────── */

static int _bench_pass = 0;
static int _bench_fail = 0;

void aria_bench_assert_true(int32_t cond, const char *msg) {
    if (cond) { printf("[PASS] %s\n", msg); _bench_pass++; }
    else      { printf("[FAIL] %s\n", msg); _bench_fail++; }
}

void aria_bench_assert_gt(int64_t a, int64_t b, const char *msg) {
    if (a > b) { printf("[PASS] %s\n", msg); _bench_pass++; }
    else       { printf("[FAIL] %s (got %ld <= %ld)\n", msg, (long)a, (long)b); _bench_fail++; }
}

void aria_bench_assert_eq(int64_t a, int64_t b, const char *msg) {
    if (a == b) { printf("[PASS] %s\n", msg); _bench_pass++; }
    else        { printf("[FAIL] %s (got %ld, expected %ld)\n", msg, (long)a, (long)b); _bench_fail++; }
}

void aria_bench_test_summary(void) {
    printf("\n--- %d passed, %d failed ---\n", _bench_pass, _bench_fail);
}
