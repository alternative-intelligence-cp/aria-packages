/*
 * aria_tensor_shim.c — CPU float32 tensor library for Aria
 *
 * Manages N-dimensional float32 tensors as opaque int64 handles.
 * Data is row-major. Supports up to 8 dimensions.
 *
 * Compile: gcc -shared -fPIC -O2 -o libaria_tensor_shim.so aria_tensor_shim.c -lm
 *
 * ABI: all float params/returns are double (Aria flt32 ABI mismatch).
 *       "free" avoided in all names (Aria compiler bug).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define MAX_DIMS 8
#define MAX_TENSORS 4096

typedef struct {
    float *data;
    int    shape[MAX_DIMS];
    int    ndim;
    int    size;       /* total element count */
    int    alive;      /* slot in use */
} Tensor;

static Tensor g_tensors[MAX_TENSORS];
static int    g_next_id = 0;
static char   g_error[256] = "";

static void set_err(const char *msg) {
    strncpy(g_error, msg, sizeof(g_error) - 1);
    g_error[sizeof(g_error) - 1] = '\0';
}

const char* aria_tensor_last_error(void) { return g_error; }

static Tensor* get(long long id) {
    if (id < 0 || id >= MAX_TENSORS || !g_tensors[id].alive) {
        set_err("invalid tensor handle");
        return NULL;
    }
    return &g_tensors[id];
}

static int alloc_slot(void) {
    for (int i = g_next_id; i < MAX_TENSORS; i++) {
        if (!g_tensors[i].alive) { g_next_id = i + 1; return i; }
    }
    for (int i = 0; i < g_next_id; i++) {
        if (!g_tensors[i].alive) { g_next_id = i + 1; return i; }
    }
    return -1;
}

/* ============================================================
 * Creation / destruction
 * ============================================================ */

/* Create tensor from up to 8 dimensions. Unused dims = 0. */
long long aria_tensor_create(int d0, int d1, int d2, int d3,
                             int d4, int d5, int d6, int d7) {
    int dims[MAX_DIMS] = {d0, d1, d2, d3, d4, d5, d6, d7};
    int ndim = 0;
    int size = 1;
    for (int i = 0; i < MAX_DIMS; i++) {
        if (dims[i] <= 0) break;
        size *= dims[i];
        ndim++;
    }
    if (ndim == 0) { set_err("tensor needs at least 1 dimension"); return -1; }

    int slot = alloc_slot();
    if (slot < 0) { set_err("tensor pool exhausted"); return -1; }

    float *data = (float*)calloc((size_t)size, sizeof(float));
    if (!data) { set_err("allocation failed"); return -1; }

    Tensor *t = &g_tensors[slot];
    t->data = data;
    memcpy(t->shape, dims, sizeof(int) * MAX_DIMS);
    t->ndim = ndim;
    t->size = size;
    t->alive = 1;
    return (long long)slot;
}

/* Create 1D tensor */
long long aria_tensor_create_1d(int n) {
    return aria_tensor_create(n, 0, 0, 0, 0, 0, 0, 0);
}

/* Create 2D tensor (matrix) */
long long aria_tensor_create_2d(int rows, int cols) {
    return aria_tensor_create(rows, cols, 0, 0, 0, 0, 0, 0);
}

/* Create 3D tensor */
long long aria_tensor_create_3d(int d0, int d1, int d2) {
    return aria_tensor_create(d0, d1, d2, 0, 0, 0, 0, 0);
}

void aria_tensor_destroy(long long id) {
    Tensor *t = get(id);
    if (!t) return;
    if (t->data) free(t->data);
    t->data = NULL;
    t->alive = 0;
}

/* Clone tensor with all data */
long long aria_tensor_clone(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    long long nid = aria_tensor_create(src->shape[0], src->shape[1],
        src->shape[2], src->shape[3], src->shape[4], src->shape[5],
        src->shape[6], src->shape[7]);
    if (nid < 0) return -1;
    memcpy(g_tensors[nid].data, src->data, (size_t)src->size * sizeof(float));
    return nid;
}

/* ============================================================
 * Metadata
 * ============================================================ */

int aria_tensor_ndim(long long id) {
    Tensor *t = get(id);
    return t ? t->ndim : -1;
}

int aria_tensor_size(long long id) {
    Tensor *t = get(id);
    return t ? t->size : -1;
}

int aria_tensor_shape_at(long long id, int dim) {
    Tensor *t = get(id);
    if (!t || dim < 0 || dim >= t->ndim) return -1;
    return t->shape[dim];
}

/* Return raw data pointer as int64 (for GPU transfer via aria-cuda) */
long long aria_tensor_data_ptr(long long id) {
    Tensor *t = get(id);
    if (!t) return 0;
    return (long long)(uintptr_t)t->data;
}

/* ============================================================
 * Element access (double for Aria ABI)
 * ============================================================ */

void aria_tensor_set_1d(long long id, int i, double val) {
    Tensor *t = get(id);
    if (!t || i < 0 || i >= t->size) return;
    t->data[i] = (float)val;
}

double aria_tensor_get_1d(long long id, int i) {
    Tensor *t = get(id);
    if (!t || i < 0 || i >= t->size) return 0.0;
    return (double)t->data[i];
}

void aria_tensor_set_2d(long long id, int r, int c, double val) {
    Tensor *t = get(id);
    if (!t || t->ndim < 2) return;
    int cols = t->shape[1];
    int idx = r * cols + c;
    if (idx < 0 || idx >= t->size) return;
    t->data[idx] = (float)val;
}

double aria_tensor_get_2d(long long id, int r, int c) {
    Tensor *t = get(id);
    if (!t || t->ndim < 2) return 0.0;
    int cols = t->shape[1];
    int idx = r * cols + c;
    if (idx < 0 || idx >= t->size) return 0.0;
    return (double)t->data[idx];
}

/* ============================================================
 * Fill operations
 * ============================================================ */

void aria_tensor_fill(long long id, double val) {
    Tensor *t = get(id);
    if (!t) return;
    float v = (float)val;
    for (int i = 0; i < t->size; i++) t->data[i] = v;
}

void aria_tensor_fill_range(long long id, double start, double step) {
    Tensor *t = get(id);
    if (!t) return;
    float s = (float)start, st = (float)step;
    for (int i = 0; i < t->size; i++) t->data[i] = s + st * i;
}

/* Simple xorshift random in [0,1) */
static unsigned int g_rng_state = 42;
void aria_tensor_fill_random(long long id) {
    Tensor *t = get(id);
    if (!t) return;
    for (int i = 0; i < t->size; i++) {
        g_rng_state ^= g_rng_state << 13;
        g_rng_state ^= g_rng_state >> 17;
        g_rng_state ^= g_rng_state << 5;
        t->data[i] = (float)(g_rng_state & 0x7FFFFF) / (float)0x7FFFFF;
    }
}

/* Xavier/Glorot initialization for weight matrices */
void aria_tensor_fill_xavier(long long id, int fan_in, int fan_out) {
    Tensor *t = get(id);
    if (!t) return;
    float limit = sqrtf(6.0f / (float)(fan_in + fan_out));
    for (int i = 0; i < t->size; i++) {
        g_rng_state ^= g_rng_state << 13;
        g_rng_state ^= g_rng_state >> 17;
        g_rng_state ^= g_rng_state << 5;
        float r = (float)(g_rng_state & 0x7FFFFF) / (float)0x7FFFFF;
        t->data[i] = (2.0f * r - 1.0f) * limit;
    }
}

void aria_tensor_set_rng_seed(int seed) {
    g_rng_state = (unsigned int)seed;
}

/* ============================================================
 * Shape operations
 * ============================================================ */

/* Reshape tensor (must have same total size) — returns new tensor */
long long aria_tensor_reshape(long long id, int d0, int d1, int d2, int d3) {
    Tensor *src = get(id);
    if (!src) return -1;

    int dims[8] = {d0, d1, d2, d3, 0, 0, 0, 0};
    int new_size = 1, ndim = 0;
    for (int i = 0; i < 4; i++) {
        if (dims[i] <= 0) break;
        new_size *= dims[i];
        ndim++;
    }
    if (new_size != src->size) {
        set_err("reshape size mismatch");
        return -1;
    }

    long long nid = aria_tensor_create(d0, d1, d2, d3, 0, 0, 0, 0);
    if (nid < 0) return -1;
    memcpy(g_tensors[nid].data, src->data, (size_t)src->size * sizeof(float));
    return nid;
}

/* Transpose 2D matrix — returns new tensor */
long long aria_tensor_transpose(long long id) {
    Tensor *src = get(id);
    if (!src || src->ndim != 2) { set_err("transpose needs 2D tensor"); return -1; }

    int rows = src->shape[0], cols = src->shape[1];
    long long nid = aria_tensor_create_2d(cols, rows);
    if (nid < 0) return -1;

    Tensor *dst = &g_tensors[nid];
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            dst->data[c * rows + r] = src->data[r * cols + c];
        }
    }
    return nid;
}

/* Flatten to 1D — returns new tensor */
long long aria_tensor_flatten(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    return aria_tensor_reshape(id, src->size, 0, 0, 0);
}

/* ============================================================
 * Arithmetic (element-wise, return new tensor)
 * ============================================================ */

long long aria_tensor_add(long long a_id, long long b_id) {
    Tensor *a = get(a_id), *b = get(b_id);
    if (!a || !b || a->size != b->size) {
        set_err("add: size mismatch");
        return -1;
    }
    long long nid = aria_tensor_clone(a_id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) r->data[i] += b->data[i];
    return nid;
}

long long aria_tensor_sub(long long a_id, long long b_id) {
    Tensor *a = get(a_id), *b = get(b_id);
    if (!a || !b || a->size != b->size) {
        set_err("sub: size mismatch");
        return -1;
    }
    long long nid = aria_tensor_clone(a_id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) r->data[i] -= b->data[i];
    return nid;
}

long long aria_tensor_mul(long long a_id, long long b_id) {
    Tensor *a = get(a_id), *b = get(b_id);
    if (!a || !b || a->size != b->size) {
        set_err("mul: size mismatch");
        return -1;
    }
    long long nid = aria_tensor_clone(a_id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) r->data[i] *= b->data[i];
    return nid;
}

long long aria_tensor_scale(long long id, double alpha_d) {
    Tensor *src = get(id);
    if (!src) return -1;
    float alpha = (float)alpha_d;
    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) r->data[i] *= alpha;
    return nid;
}

/* Add bias to each row of a 2D tensor (broadcasting) */
long long aria_tensor_add_bias(long long id, long long bias_id) {
    Tensor *t = get(id), *b = get(bias_id);
    if (!t || !b || t->ndim < 2) { set_err("add_bias: need 2D tensor"); return -1; }
    int cols = t->shape[t->ndim - 1];
    if (b->size != cols) { set_err("add_bias: bias size != cols"); return -1; }

    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    int rows = r->size / cols;
    for (int row = 0; row < rows; row++) {
        for (int c = 0; c < cols; c++) {
            r->data[row * cols + c] += b->data[c];
        }
    }
    return nid;
}

/* ============================================================
 * In-place arithmetic (modifies tensor directly)
 * ============================================================ */

void aria_tensor_add_inplace(long long a_id, long long b_id) {
    Tensor *a = get(a_id), *b = get(b_id);
    if (!a || !b || a->size != b->size) return;
    for (int i = 0; i < a->size; i++) a->data[i] += b->data[i];
}

void aria_tensor_scale_inplace(long long id, double alpha_d) {
    Tensor *t = get(id);
    if (!t) return;
    float alpha = (float)alpha_d;
    for (int i = 0; i < t->size; i++) t->data[i] *= alpha;
}

/* ============================================================
 * Matrix multiply (CPU, row-major)
 * A[M,K] * B[K,N] = C[M,N]
 * ============================================================ */

long long aria_tensor_matmul(long long a_id, long long b_id) {
    Tensor *a = get(a_id), *b = get(b_id);
    if (!a || !b || a->ndim != 2 || b->ndim != 2) {
        set_err("matmul: need 2D tensors");
        return -1;
    }
    int M = a->shape[0], K = a->shape[1];
    int K2 = b->shape[0], N = b->shape[1];
    if (K != K2) { set_err("matmul: inner dims mismatch"); return -1; }

    long long nid = aria_tensor_create_2d(M, N);
    if (nid < 0) return -1;
    Tensor *c = &g_tensors[nid];

    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++) {
                sum += a->data[m * K + k] * b->data[k * N + n];
            }
            c->data[m * N + n] = sum;
        }
    }
    return nid;
}

/* ============================================================
 * Activations (return new tensor)
 * ============================================================ */

long long aria_tensor_relu(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) {
        if (r->data[i] < 0.0f) r->data[i] = 0.0f;
    }
    return nid;
}

long long aria_tensor_sigmoid(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) {
        r->data[i] = 1.0f / (1.0f + expf(-r->data[i]));
    }
    return nid;
}

long long aria_tensor_tanh_act(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) {
        r->data[i] = tanhf(r->data[i]);
    }
    return nid;
}

/* SiLU (Swish): x * sigmoid(x) */
long long aria_tensor_silu(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) {
        float sig = 1.0f / (1.0f + expf(-r->data[i]));
        r->data[i] *= sig;
    }
    return nid;
}

/* GELU approximation */
long long aria_tensor_gelu(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    for (int i = 0; i < r->size; i++) {
        float x = r->data[i];
        r->data[i] = 0.5f * x * (1.0f + tanhf(0.7978845608f *
            (x + 0.044715f * x * x * x)));
    }
    return nid;
}

/* Softmax along last dimension of 2D tensor */
long long aria_tensor_softmax(long long id) {
    Tensor *src = get(id);
    if (!src) return -1;
    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];

    if (r->ndim == 1) {
        /* 1D softmax */
        float mx = r->data[0];
        for (int i = 1; i < r->size; i++) if (r->data[i] > mx) mx = r->data[i];
        float sum = 0.0f;
        for (int i = 0; i < r->size; i++) {
            r->data[i] = expf(r->data[i] - mx);
            sum += r->data[i];
        }
        for (int i = 0; i < r->size; i++) r->data[i] /= sum;
    } else {
        /* 2D: softmax per row */
        int rows = r->shape[0], cols = r->shape[1];
        for (int row = 0; row < rows; row++) {
            float *p = r->data + row * cols;
            float mx = p[0];
            for (int c = 1; c < cols; c++) if (p[c] > mx) mx = p[c];
            float sum = 0.0f;
            for (int c = 0; c < cols; c++) {
                p[c] = expf(p[c] - mx);
                sum += p[c];
            }
            for (int c = 0; c < cols; c++) p[c] /= sum;
        }
    }
    return nid;
}

/* ============================================================
 * Reductions
 * ============================================================ */

double aria_tensor_sum(long long id) {
    Tensor *t = get(id);
    if (!t) return 0.0;
    double s = 0.0;
    for (int i = 0; i < t->size; i++) s += (double)t->data[i];
    return s;
}

double aria_tensor_mean(long long id) {
    Tensor *t = get(id);
    if (!t || t->size == 0) return 0.0;
    return aria_tensor_sum(id) / (double)t->size;
}

double aria_tensor_max_val(long long id) {
    Tensor *t = get(id);
    if (!t) return 0.0;
    float mx = t->data[0];
    for (int i = 1; i < t->size; i++) if (t->data[i] > mx) mx = t->data[i];
    return (double)mx;
}

double aria_tensor_min_val(long long id) {
    Tensor *t = get(id);
    if (!t) return 0.0;
    float mn = t->data[0];
    for (int i = 1; i < t->size; i++) if (t->data[i] < mn) mn = t->data[i];
    return (double)mn;
}

/* ============================================================
 * Layer normalization
 * ============================================================ */

/* Layer norm along last dimension of 2D tensor */
long long aria_tensor_layer_norm(long long id, double eps_d) {
    Tensor *src = get(id);
    if (!src || src->ndim != 2) { set_err("layer_norm: need 2D"); return -1; }
    float eps = (float)eps_d;
    int rows = src->shape[0], cols = src->shape[1];

    long long nid = aria_tensor_clone(id);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];

    for (int row = 0; row < rows; row++) {
        float *p = r->data + row * cols;
        float mean = 0.0f;
        for (int c = 0; c < cols; c++) mean += p[c];
        mean /= cols;
        float var = 0.0f;
        for (int c = 0; c < cols; c++) {
            float d = p[c] - mean;
            var += d * d;
        }
        var /= cols;
        float inv_std = 1.0f / sqrtf(var + eps);
        for (int c = 0; c < cols; c++) {
            p[c] = (p[c] - mean) * inv_std;
        }
    }
    return nid;
}

/* ============================================================
 * Debug / print
 * ============================================================ */

void aria_tensor_print(long long id) {
    Tensor *t = get(id);
    if (!t) { printf("[invalid tensor]\n"); return; }

    printf("Tensor(shape=[");
    for (int i = 0; i < t->ndim; i++) {
        if (i > 0) printf(", ");
        printf("%d", t->shape[i]);
    }
    printf("], size=%d)\n", t->size);

    if (t->ndim == 1) {
        printf("[");
        int lim = t->size < 10 ? t->size : 10;
        for (int i = 0; i < lim; i++) {
            if (i > 0) printf(", ");
            printf("%.4f", t->data[i]);
        }
        if (t->size > 10) printf(", ...");
        printf("]\n");
    } else if (t->ndim == 2) {
        int rows = t->shape[0], cols = t->shape[1];
        int rlim = rows < 6 ? rows : 6;
        for (int r = 0; r < rlim; r++) {
            printf("  [");
            int clim = cols < 8 ? cols : 8;
            for (int c = 0; c < clim; c++) {
                if (c > 0) printf(", ");
                printf("%.4f", t->data[r * cols + c]);
            }
            if (cols > 8) printf(", ...");
            printf("]\n");
        }
        if (rows > 6) printf("  ...\n");
    }
}

/* ============================================================
 * Concatenation
 * ============================================================ */

/* Concatenate two 2D tensors along rows (axis 0) */
long long aria_tensor_concat_rows(long long a_id, long long b_id) {
    Tensor *a = get(a_id), *b = get(b_id);
    if (!a || !b || a->ndim != 2 || b->ndim != 2) {
        set_err("concat: need 2D tensors");
        return -1;
    }
    if (a->shape[1] != b->shape[1]) {
        set_err("concat: column count mismatch");
        return -1;
    }
    int new_rows = a->shape[0] + b->shape[0];
    int cols = a->shape[1];
    long long nid = aria_tensor_create_2d(new_rows, cols);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    memcpy(r->data, a->data, (size_t)a->size * sizeof(float));
    memcpy(r->data + a->size, b->data, (size_t)b->size * sizeof(float));
    return nid;
}

/* Slice rows from 2D tensor [start, end) */
long long aria_tensor_slice_rows(long long id, int start, int count) {
    Tensor *t = get(id);
    if (!t || t->ndim != 2) { set_err("slice: need 2D tensor"); return -1; }
    int cols = t->shape[1];
    if (start < 0 || start + count > t->shape[0]) {
        set_err("slice: out of bounds");
        return -1;
    }
    long long nid = aria_tensor_create_2d(count, cols);
    if (nid < 0) return -1;
    Tensor *r = &g_tensors[nid];
    memcpy(r->data, t->data + start * cols, (size_t)(count * cols) * sizeof(float));
    return nid;
}

/* ============================================================
 * GPU interop helpers
 * ============================================================ */

/* Copy raw float data FROM an external buffer (e.g., downloaded from GPU) */
void aria_tensor_copy_from_ptr(long long id, long long src_ptr, int count) {
    Tensor *t = get(id);
    if (!t || count > t->size) return;
    memcpy(t->data, (const void*)(uintptr_t)src_ptr, (size_t)count * sizeof(float));
}

/* Copy raw float data TO an external buffer (e.g., for GPU upload) */
void aria_tensor_copy_to_ptr(long long id, long long dst_ptr, int count) {
    Tensor *t = get(id);
    if (!t || count > t->size) return;
    memcpy((void*)(uintptr_t)dst_ptr, t->data, (size_t)count * sizeof(float));
}
