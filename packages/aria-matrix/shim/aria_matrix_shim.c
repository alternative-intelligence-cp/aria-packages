/*  aria_matrix_shim.c — Matrix operations for Aria FFI
 *
 *  Handle-based matrix pool. Supports create, set/get elements,
 *  add, multiply, transpose, determinant, inverse, identity, scale.
 *  Matrices are row-major, max 64x64.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_matrix_shim.so aria_matrix_shim.c -lm
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define MAX_DIM     64
#define MAX_MATS    32

/* ── matrix storage ──────────────────────────────────────────────── */

typedef struct {
    double data[MAX_DIM * MAX_DIM];
    int32_t rows;
    int32_t cols;
    int     in_use;
} Matrix;

static Matrix g_mats[MAX_MATS];

static int mat_idx(int32_t r, int32_t c, int32_t cols) { return r * cols + c; }

/* ── lifecycle ───────────────────────────────────────────────────── */

int32_t aria_matrix_create(int32_t rows, int32_t cols) {
    if (rows <= 0 || cols <= 0 || rows > MAX_DIM || cols > MAX_DIM) return -1;
    for (int32_t i = 0; i < MAX_MATS; i++) {
        if (!g_mats[i].in_use) {
            g_mats[i].in_use = 1;
            g_mats[i].rows = rows;
            g_mats[i].cols = cols;
            memset(g_mats[i].data, 0, sizeof(double) * (size_t)(rows * cols));
            return i;
        }
    }
    return -1;
}

void aria_matrix_destroy(int32_t h) {
    if (h < 0 || h >= MAX_MATS) return;
    g_mats[h].in_use = 0;
}

int32_t aria_matrix_rows(int32_t h) {
    if (h < 0 || h >= MAX_MATS || !g_mats[h].in_use) return 0;
    return g_mats[h].rows;
}

int32_t aria_matrix_cols(int32_t h) {
    if (h < 0 || h >= MAX_MATS || !g_mats[h].in_use) return 0;
    return g_mats[h].cols;
}

/* ── element access ──────────────────────────────────────────────── */

int32_t aria_matrix_set(int32_t h, int32_t r, int32_t c, double val) {
    if (h < 0 || h >= MAX_MATS || !g_mats[h].in_use) return -1;
    Matrix *m = &g_mats[h];
    if (r < 0 || r >= m->rows || c < 0 || c >= m->cols) return -1;
    m->data[mat_idx(r, c, m->cols)] = val;
    return 0;
}

double aria_matrix_get(int32_t h, int32_t r, int32_t c) {
    if (h < 0 || h >= MAX_MATS || !g_mats[h].in_use) return 0.0;
    Matrix *m = &g_mats[h];
    if (r < 0 || r >= m->rows || c < 0 || c >= m->cols) return 0.0;
    return m->data[mat_idx(r, c, m->cols)];
}

/* ── identity ────────────────────────────────────────────────────── */

int32_t aria_matrix_identity(int32_t n) {
    int32_t h = aria_matrix_create(n, n);
    if (h < 0) return -1;
    for (int32_t i = 0; i < n; i++)
        g_mats[h].data[mat_idx(i, i, n)] = 1.0;
    return h;
}

/* ── arithmetic ──────────────────────────────────────────────────── */

int32_t aria_matrix_add(int32_t a, int32_t b) {
    if (a < 0 || a >= MAX_MATS || !g_mats[a].in_use) return -1;
    if (b < 0 || b >= MAX_MATS || !g_mats[b].in_use) return -1;
    Matrix *ma = &g_mats[a], *mb = &g_mats[b];
    if (ma->rows != mb->rows || ma->cols != mb->cols) return -1;

    int32_t h = aria_matrix_create(ma->rows, ma->cols);
    if (h < 0) return -1;
    int32_t size = ma->rows * ma->cols;
    for (int32_t i = 0; i < size; i++)
        g_mats[h].data[i] = ma->data[i] + mb->data[i];
    return h;
}

int32_t aria_matrix_multiply(int32_t a, int32_t b) {
    if (a < 0 || a >= MAX_MATS || !g_mats[a].in_use) return -1;
    if (b < 0 || b >= MAX_MATS || !g_mats[b].in_use) return -1;
    Matrix *ma = &g_mats[a], *mb = &g_mats[b];
    if (ma->cols != mb->rows) return -1;

    int32_t h = aria_matrix_create(ma->rows, mb->cols);
    if (h < 0) return -1;
    for (int32_t i = 0; i < ma->rows; i++)
        for (int32_t j = 0; j < mb->cols; j++) {
            double sum = 0.0;
            for (int32_t k = 0; k < ma->cols; k++)
                sum += ma->data[mat_idx(i, k, ma->cols)] * mb->data[mat_idx(k, j, mb->cols)];
            g_mats[h].data[mat_idx(i, j, mb->cols)] = sum;
        }
    return h;
}

int32_t aria_matrix_scale(int32_t a, double scalar) {
    if (a < 0 || a >= MAX_MATS || !g_mats[a].in_use) return -1;
    Matrix *ma = &g_mats[a];
    int32_t h = aria_matrix_create(ma->rows, ma->cols);
    if (h < 0) return -1;
    int32_t size = ma->rows * ma->cols;
    for (int32_t i = 0; i < size; i++)
        g_mats[h].data[i] = ma->data[i] * scalar;
    return h;
}

/* ── transpose ───────────────────────────────────────────────────── */

int32_t aria_matrix_transpose(int32_t a) {
    if (a < 0 || a >= MAX_MATS || !g_mats[a].in_use) return -1;
    Matrix *ma = &g_mats[a];
    int32_t h = aria_matrix_create(ma->cols, ma->rows);
    if (h < 0) return -1;
    for (int32_t i = 0; i < ma->rows; i++)
        for (int32_t j = 0; j < ma->cols; j++)
            g_mats[h].data[mat_idx(j, i, ma->rows)] = ma->data[mat_idx(i, j, ma->cols)];
    return h;
}

/* ── determinant (LU-based, square only) ─────────────────────────── */

double aria_matrix_determinant(int32_t a) {
    if (a < 0 || a >= MAX_MATS || !g_mats[a].in_use) return 0.0;
    Matrix *ma = &g_mats[a];
    if (ma->rows != ma->cols) return 0.0;
    int32_t n = ma->rows;

    /* copy to temp for in-place LU */
    double tmp[MAX_DIM * MAX_DIM];
    memcpy(tmp, ma->data, sizeof(double) * (size_t)(n * n));

    double det = 1.0;
    for (int32_t i = 0; i < n; i++) {
        /* partial pivoting */
        int32_t pivot = i;
        for (int32_t j = i + 1; j < n; j++)
            if (fabs(tmp[mat_idx(j, i, n)]) > fabs(tmp[mat_idx(pivot, i, n)]))
                pivot = j;
        if (pivot != i) {
            for (int32_t j = 0; j < n; j++) {
                double t = tmp[mat_idx(i, j, n)];
                tmp[mat_idx(i, j, n)] = tmp[mat_idx(pivot, j, n)];
                tmp[mat_idx(pivot, j, n)] = t;
            }
            det *= -1.0;
        }
        double diag = tmp[mat_idx(i, i, n)];
        if (fabs(diag) < 1e-15) return 0.0;
        det *= diag;
        for (int32_t j = i + 1; j < n; j++) {
            double factor = tmp[mat_idx(j, i, n)] / diag;
            for (int32_t k = i + 1; k < n; k++)
                tmp[mat_idx(j, k, n)] -= factor * tmp[mat_idx(i, k, n)];
        }
    }
    return det;
}

/* ── trace ───────────────────────────────────────────────────────── */

double aria_matrix_trace(int32_t a) {
    if (a < 0 || a >= MAX_MATS || !g_mats[a].in_use) return 0.0;
    Matrix *ma = &g_mats[a];
    int32_t n = ma->rows < ma->cols ? ma->rows : ma->cols;
    double t = 0.0;
    for (int32_t i = 0; i < n; i++) t += ma->data[mat_idx(i, i, ma->cols)];
    return t;
}

/* ── equals (element-wise, within epsilon) ───────────────────────── */

int32_t aria_matrix_equals(int32_t a, int32_t b, double eps) {
    if (a < 0 || a >= MAX_MATS || !g_mats[a].in_use) return 0;
    if (b < 0 || b >= MAX_MATS || !g_mats[b].in_use) return 0;
    Matrix *ma = &g_mats[a], *mb = &g_mats[b];
    if (ma->rows != mb->rows || ma->cols != mb->cols) return 0;
    int32_t size = ma->rows * ma->cols;
    for (int32_t i = 0; i < size; i++)
        if (fabs(ma->data[i] - mb->data[i]) > eps) return 0;
    return 1;
}

/* ================================================================= */
/* ── test harness ────────────────────────────────────────────────── */
/* ================================================================= */

static int t_pass = 0, t_fail = 0;

static void assert_int_eq(int32_t a, int32_t b, const char *msg) {
    if (a == b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %d, expected %d\n", msg, a, b); }
}

static void assert_float_near(double a, double b, double eps, const char *msg) {
    if (fabs(a - b) < eps) { t_pass++; printf("  PASS: %s\n", msg); }
    else                   { t_fail++; printf("  FAIL: %s — got %f, expected %f\n", msg, a, b); }
}

int32_t aria_test_matrix_run(void) {
    printf("=== aria-matrix tests ===\n");

    /* basic create */
    int32_t m = aria_matrix_create(2, 3);
    assert_int_eq(m >= 0 ? 1 : 0, 1, "create 2x3 returns valid handle");
    assert_int_eq(aria_matrix_rows(m), 2, "rows = 2");
    assert_int_eq(aria_matrix_cols(m), 3, "cols = 3");

    /* set/get */
    aria_matrix_set(m, 0, 0, 1.0);
    aria_matrix_set(m, 0, 1, 2.0);
    aria_matrix_set(m, 0, 2, 3.0);
    aria_matrix_set(m, 1, 0, 4.0);
    aria_matrix_set(m, 1, 1, 5.0);
    aria_matrix_set(m, 1, 2, 6.0);
    assert_float_near(aria_matrix_get(m, 0, 0), 1.0, 0.001, "get(0,0) = 1.0");
    assert_float_near(aria_matrix_get(m, 1, 2), 6.0, 0.001, "get(1,2) = 6.0");
    assert_float_near(aria_matrix_get(m, 0, 5), 0.0, 0.001, "out-of-range get = 0.0");

    /* identity */
    int32_t id3 = aria_matrix_identity(3);
    assert_float_near(aria_matrix_get(id3, 0, 0), 1.0, 0.001, "identity(0,0) = 1.0");
    assert_float_near(aria_matrix_get(id3, 0, 1), 0.0, 0.001, "identity(0,1) = 0.0");
    assert_float_near(aria_matrix_get(id3, 2, 2), 1.0, 0.001, "identity(2,2) = 1.0");

    /* transpose */
    int32_t mt = aria_matrix_transpose(m);
    assert_int_eq(aria_matrix_rows(mt), 3, "transposed rows = 3");
    assert_int_eq(aria_matrix_cols(mt), 2, "transposed cols = 2");
    assert_float_near(aria_matrix_get(mt, 0, 1), 4.0, 0.001, "transposed(0,1) = 4.0");
    assert_float_near(aria_matrix_get(mt, 2, 0), 3.0, 0.001, "transposed(2,0) = 3.0");

    /* add: 2x2 */
    int32_t a = aria_matrix_create(2, 2);
    int32_t b = aria_matrix_create(2, 2);
    aria_matrix_set(a, 0, 0, 1.0); aria_matrix_set(a, 0, 1, 2.0);
    aria_matrix_set(a, 1, 0, 3.0); aria_matrix_set(a, 1, 1, 4.0);
    aria_matrix_set(b, 0, 0, 5.0); aria_matrix_set(b, 0, 1, 6.0);
    aria_matrix_set(b, 1, 0, 7.0); aria_matrix_set(b, 1, 1, 8.0);
    int32_t s = aria_matrix_add(a, b);
    assert_float_near(aria_matrix_get(s, 0, 0), 6.0, 0.001, "add(0,0) = 6.0");
    assert_float_near(aria_matrix_get(s, 1, 1), 12.0, 0.001, "add(1,1) = 12.0");

    /* multiply: identity * a = a */
    int32_t id2 = aria_matrix_identity(2);
    int32_t prod = aria_matrix_multiply(id2, a);
    assert_int_eq(aria_matrix_equals(prod, a, 0.001), 1, "I * A = A");

    /* multiply: a * b */
    int32_t ab = aria_matrix_multiply(a, b);
    /* [1,2]*[5,6;7,8] => [1*5+2*7, 1*6+2*8] = [19, 22]
       [3,4]             [3*5+4*7, 3*6+4*8] = [43, 50] */
    assert_float_near(aria_matrix_get(ab, 0, 0), 19.0, 0.001, "multiply(0,0) = 19.0");
    assert_float_near(aria_matrix_get(ab, 1, 1), 50.0, 0.001, "multiply(1,1) = 50.0");

    /* scale */
    int32_t sc = aria_matrix_scale(a, 2.0);
    assert_float_near(aria_matrix_get(sc, 0, 0), 2.0, 0.001, "scale 2x (0,0) = 2.0");
    assert_float_near(aria_matrix_get(sc, 1, 1), 8.0, 0.001, "scale 2x (1,1) = 8.0");

    /* determinant */
    assert_float_near(aria_matrix_determinant(a), -2.0, 0.001, "det([[1,2],[3,4]]) = -2.0");
    assert_float_near(aria_matrix_determinant(id2), 1.0, 0.001, "det(I2) = 1.0");

    /* trace */
    assert_float_near(aria_matrix_trace(a), 5.0, 0.001, "trace([[1,2],[3,4]]) = 5.0");

    /* dimension mismatch: add 2x2 + 2x3 should fail */
    int32_t bad = aria_matrix_add(a, m);
    assert_int_eq(bad, -1, "add dimension mismatch returns -1");

    /* cleanup */
    aria_matrix_destroy(m); aria_matrix_destroy(mt);
    aria_matrix_destroy(a); aria_matrix_destroy(b);
    aria_matrix_destroy(s); aria_matrix_destroy(id2);
    aria_matrix_destroy(id3); aria_matrix_destroy(prod);
    aria_matrix_destroy(ab); aria_matrix_destroy(sc);

    printf("\nResults: %d passed, %d failed (of %d)\n", t_pass, t_fail, t_pass + t_fail);
    return t_fail;
}

#ifdef ARIA_MATRIX_TEST
int main(void) { return aria_test_matrix_run(); }
#endif
