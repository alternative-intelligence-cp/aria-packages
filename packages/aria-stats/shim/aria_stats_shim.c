/*  aria_stats_shim.c — Statistics functions for Aria FFI
 *
 *  Operates on a static double array (load data, then compute stats).
 *  Supports: mean, median, mode, variance, std deviation,
 *  min, max, range, percentile, correlation, covariance.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_stats_shim.so aria_stats_shim.c -lm
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_DATA    4096
#define MAX_SETS    8

/* ── data storage ────────────────────────────────────────────────── */

typedef struct {
    double data[MAX_DATA];
    int32_t count;
} DataSet;

static DataSet g_sets[MAX_SETS];

/* ── helpers ─────────────────────────────────────────────────────── */

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/* ── lifecycle ───────────────────────────────────────────────────── */

int32_t aria_stats_create(void) {
    for (int32_t i = 0; i < MAX_SETS; i++) {
        if (g_sets[i].count == -1) {
            g_sets[i].count = 0;
            return i;
        }
    }
    /* find first empty (count==0 and not in use) — use sentinel */
    for (int32_t i = 0; i < MAX_SETS; i++) {
        if (g_sets[i].count == 0) {
            return i;
        }
    }
    return -1;
}

void aria_stats_destroy(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return;
    g_sets[handle].count = 0;
}

int32_t aria_stats_clear(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return -1;
    g_sets[handle].count = 0;
    return 0;
}

/* ── data input ──────────────────────────────────────────────────── */

int32_t aria_stats_push(int32_t handle, double value) {
    if (handle < 0 || handle >= MAX_SETS) return -1;
    DataSet *ds = &g_sets[handle];
    if (ds->count >= MAX_DATA) return -1;
    ds->data[ds->count++] = value;
    return 0;
}

int32_t aria_stats_count(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0;
    return g_sets[handle].count;
}

double aria_stats_get(int32_t handle, int32_t idx) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (idx < 0 || idx >= ds->count) return 0.0;
    return ds->data[idx];
}

/* ── descriptive statistics ──────────────────────────────────────── */

double aria_stats_mean(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (ds->count == 0) return 0.0;
    double sum = 0.0;
    for (int32_t i = 0; i < ds->count; i++) sum += ds->data[i];
    return sum / (double)ds->count;
}

double aria_stats_median(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (ds->count == 0) return 0.0;

    /* sort a copy */
    double tmp[MAX_DATA];
    memcpy(tmp, ds->data, sizeof(double) * (size_t)ds->count);
    qsort(tmp, (size_t)ds->count, sizeof(double), cmp_double);

    int32_t n = ds->count;
    if (n % 2 == 1) return tmp[n / 2];
    return (tmp[n / 2 - 1] + tmp[n / 2]) / 2.0;
}

double aria_stats_mode(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (ds->count == 0) return 0.0;

    /* sort a copy, then count runs */
    double tmp[MAX_DATA];
    memcpy(tmp, ds->data, sizeof(double) * (size_t)ds->count);
    qsort(tmp, (size_t)ds->count, sizeof(double), cmp_double);

    double best_val = tmp[0];
    int32_t best_cnt = 1, cur_cnt = 1;
    for (int32_t i = 1; i < ds->count; i++) {
        if (tmp[i] == tmp[i - 1]) {
            cur_cnt++;
        } else {
            if (cur_cnt > best_cnt) { best_cnt = cur_cnt; best_val = tmp[i - 1]; }
            cur_cnt = 1;
        }
    }
    if (cur_cnt > best_cnt) best_val = tmp[ds->count - 1];
    return best_val;
}

double aria_stats_variance(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (ds->count < 2) return 0.0;

    double m = aria_stats_mean(handle);
    double sum_sq = 0.0;
    for (int32_t i = 0; i < ds->count; i++) {
        double d = ds->data[i] - m;
        sum_sq += d * d;
    }
    return sum_sq / (double)(ds->count - 1);  /* sample variance */
}

double aria_stats_stddev(int32_t handle) {
    return sqrt(aria_stats_variance(handle));
}

double aria_stats_min(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (ds->count == 0) return 0.0;
    double mn = ds->data[0];
    for (int32_t i = 1; i < ds->count; i++)
        if (ds->data[i] < mn) mn = ds->data[i];
    return mn;
}

double aria_stats_max(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (ds->count == 0) return 0.0;
    double mx = ds->data[0];
    for (int32_t i = 1; i < ds->count; i++)
        if (ds->data[i] > mx) mx = ds->data[i];
    return mx;
}

double aria_stats_range(int32_t handle) {
    return aria_stats_max(handle) - aria_stats_min(handle);
}

double aria_stats_percentile(int32_t handle, double p) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    if (ds->count == 0) return 0.0;
    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;

    double tmp[MAX_DATA];
    memcpy(tmp, ds->data, sizeof(double) * (size_t)ds->count);
    qsort(tmp, (size_t)ds->count, sizeof(double), cmp_double);

    double rank = (p / 100.0) * (double)(ds->count - 1);
    int32_t lo = (int32_t)floor(rank);
    int32_t hi = (int32_t)ceil(rank);
    if (lo == hi) return tmp[lo];
    double frac = rank - (double)lo;
    return tmp[lo] * (1.0 - frac) + tmp[hi] * frac;
}

double aria_stats_sum(int32_t handle) {
    if (handle < 0 || handle >= MAX_SETS) return 0.0;
    DataSet *ds = &g_sets[handle];
    double sum = 0.0;
    for (int32_t i = 0; i < ds->count; i++) sum += ds->data[i];
    return sum;
}

/* ── two-dataset operations ──────────────────────────────────────── */

double aria_stats_correlation(int32_t h1, int32_t h2) {
    if (h1 < 0 || h1 >= MAX_SETS || h2 < 0 || h2 >= MAX_SETS) return 0.0;
    DataSet *a = &g_sets[h1], *b = &g_sets[h2];
    int32_t n = a->count < b->count ? a->count : b->count;
    if (n < 2) return 0.0;

    double ma = 0.0, mb = 0.0;
    for (int32_t i = 0; i < n; i++) { ma += a->data[i]; mb += b->data[i]; }
    ma /= n; mb /= n;

    double cov = 0.0, va = 0.0, vb = 0.0;
    for (int32_t i = 0; i < n; i++) {
        double da = a->data[i] - ma;
        double db = b->data[i] - mb;
        cov += da * db;
        va  += da * da;
        vb  += db * db;
    }

    double denom = sqrt(va * vb);
    if (denom == 0.0) return 0.0;
    return cov / denom;
}

double aria_stats_covariance(int32_t h1, int32_t h2) {
    if (h1 < 0 || h1 >= MAX_SETS || h2 < 0 || h2 >= MAX_SETS) return 0.0;
    DataSet *a = &g_sets[h1], *b = &g_sets[h2];
    int32_t n = a->count < b->count ? a->count : b->count;
    if (n < 2) return 0.0;

    double ma = 0.0, mb = 0.0;
    for (int32_t i = 0; i < n; i++) { ma += a->data[i]; mb += b->data[i]; }
    ma /= n; mb /= n;

    double cov = 0.0;
    for (int32_t i = 0; i < n; i++) {
        cov += (a->data[i] - ma) * (b->data[i] - mb);
    }
    return cov / (double)(n - 1);  /* sample covariance */
}

/* ================================================================= */
/* ── test harness (always exported for Aria test binary) ─────────── */
/* ================================================================= */

#include <stdio.h>

static int g_pass = 0, g_fail = 0;

static void assert_int_eq(int32_t a, int32_t b, const char *msg) {
    if (a == b) { g_pass++; printf("  PASS: %s\n", msg); }
    else        { g_fail++; printf("  FAIL: %s — got %d, expected %d\n", msg, a, b); }
}

static void assert_float_near(double a, double b, double eps, const char *msg) {
    if (fabs(a - b) < eps) { g_pass++; printf("  PASS: %s\n", msg); }
    else                   { g_fail++; printf("  FAIL: %s — got %f, expected %f\n", msg, a, b); }
}

int32_t aria_test_stats_run(void) {
    printf("=== aria-stats tests ===\n");

    int32_t h = aria_stats_create();
    assert_int_eq(h >= 0 ? 1 : 0, 1, "create returns valid handle");
    assert_int_eq(aria_stats_count(h), 0, "empty dataset count = 0");

    /* push data: [2, 4, 4, 4, 5, 5, 7, 9] */
    aria_stats_push(h, 2.0);
    aria_stats_push(h, 4.0);
    aria_stats_push(h, 4.0);
    aria_stats_push(h, 4.0);
    aria_stats_push(h, 5.0);
    aria_stats_push(h, 5.0);
    aria_stats_push(h, 7.0);
    aria_stats_push(h, 9.0);

    assert_int_eq(aria_stats_count(h), 8, "count = 8 after pushes");
    assert_float_near(aria_stats_get(h, 0), 2.0, 0.001, "get(0) = 2.0");

    assert_float_near(aria_stats_mean(h), 5.0, 0.001, "mean = 5.0");
    assert_float_near(aria_stats_median(h), 4.5, 0.001, "median = 4.5");
    assert_float_near(aria_stats_mode(h), 4.0, 0.001, "mode = 4.0");
    assert_float_near(aria_stats_variance(h), 4.571, 0.01, "variance ~ 4.571");
    assert_float_near(aria_stats_stddev(h), 2.138, 0.01, "stddev ~ 2.138");
    assert_float_near(aria_stats_min(h), 2.0, 0.001, "min = 2.0");
    assert_float_near(aria_stats_max(h), 9.0, 0.001, "max = 9.0");
    assert_float_near(aria_stats_range(h), 7.0, 0.001, "range = 7.0");
    assert_float_near(aria_stats_sum(h), 40.0, 0.001, "sum = 40.0");

    /* percentiles */
    assert_float_near(aria_stats_percentile(h, 0.0), 2.0, 0.001, "p0 = 2.0");
    assert_float_near(aria_stats_percentile(h, 50.0), 4.5, 0.001, "p50 = median");
    assert_float_near(aria_stats_percentile(h, 100.0), 9.0, 0.001, "p100 = 9.0");

    /* two-dataset correlation: x vs 2x should be 1.0 */
    int32_t h2 = aria_stats_create();
    aria_stats_push(h2, 4.0);
    aria_stats_push(h2, 8.0);
    aria_stats_push(h2, 8.0);
    aria_stats_push(h2, 8.0);
    aria_stats_push(h2, 10.0);
    aria_stats_push(h2, 10.0);
    aria_stats_push(h2, 14.0);
    aria_stats_push(h2, 18.0);

    assert_float_near(aria_stats_correlation(h, h2), 1.0, 0.001, "correlation(x, 2x) = 1.0");
    assert_float_near(aria_stats_covariance(h, h2), 9.143, 0.01, "covariance(x, 2x) ~ 9.143");

    /* clear */
    aria_stats_clear(h);
    assert_int_eq(aria_stats_count(h), 0, "count = 0 after clear");
    assert_float_near(aria_stats_mean(h), 0.0, 0.001, "mean = 0.0 after clear");

    aria_stats_destroy(h);
    aria_stats_destroy(h2);

    printf("\nResults: %d passed, %d failed (of %d)\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail;
}

#ifdef ARIA_STATS_TEST
int main(void) { return aria_test_stats_run(); }
#endif
