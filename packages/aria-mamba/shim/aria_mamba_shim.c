/*
 * aria_mamba_shim.c — Mamba (Selective State Space Model) implementation
 *
 * Implements the Mamba architecture:
 *   - Linear projections (expand + contract)
 *   - 1D depthwise convolution
 *   - Selective SSM (S6) with input-dependent dynamics
 *   - SiLU gating
 *   - Layer normalization + residual connections
 *
 * Based on "Mamba: Linear-Time Sequence Modeling with Selective State Spaces"
 * (Gu & Dao, 2023)
 *
 * Compile:
 *   gcc -shared -fPIC -O2 -o libaria_mamba_shim.so aria_mamba_shim.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ── AriaString ABI (return values only) ── */
typedef struct { char *data; int64_t length; } AriaString;

static AriaString make_aria_string(const char *s) {
    AriaString r;
    int64_t len = (int64_t)strlen(s);
    r.data = (char *)malloc(len + 1);
    memcpy(r.data, s, len + 1);
    r.length = len;
    return r;
}

/* ── RNG ── */
static uint64_t g_rng = 0xCAFEBABEDEADULL;

static void set_seed(uint32_t seed) {
    g_rng = (uint64_t)seed * 6364136223846793005ULL + 1442695040888963407ULL;
    if (g_rng == 0) g_rng = 1;
}

static float randn(void) {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17;
    float u1 = (float)(g_rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17;
    float u2 = (float)(g_rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

static void xavier(float *w, int fi, int fo, int n) {
    float s = sqrtf(2.0f / (float)(fi + fo));
    for (int i = 0; i < n; i++) w[i] = randn() * s;
}

/* ── Mamba block weights ── */
typedef struct {
    /* Projections */
    float *in_proj;    /* [d_model × (2*d_inner)] — produces x_proj and z */
    float *out_proj;   /* [d_inner × d_model] */

    /* 1D conv (depthwise, kernel_size typically 4) */
    float *conv_w;     /* [d_inner × conv_k] */
    float *conv_b;     /* [d_inner] */

    /* SSM parameters projection */
    float *x_proj;     /* [d_inner × (dt_rank + 2*d_state)] — produces dt, B, C */
    float *dt_proj_w;  /* [dt_rank × d_inner] */
    float *dt_proj_b;  /* [d_inner] */

    /* SSM continuous params (A is fixed, D is learnable) */
    float *A_log;      /* [d_inner × d_state] */
    float *D;          /* [d_inner] */

    /* Layer norm */
    float *ln_g, *ln_b;  /* [d_model] */
} MambaLayer;

/* ── Mamba model ── */
#define MAX_MODELS 32

typedef struct {
    int alive;
    int d_model, d_inner, d_state, dt_rank;
    int n_layers, vocab_size, max_seq, conv_k;

    float *token_emb;          /* [vocab × d_model] */
    MambaLayer *layers;
    float *ln_final_g, *ln_final_b;  /* [d_model] */
} MambaModel;

static MambaModel g_models[MAX_MODELS];
static char g_err[512] = "";

static void set_error(const char *msg) {
    snprintf(g_err, sizeof(g_err), "%s", msg);
}

/* ── Scratch pool ── */
#define MAX_SCRATCH 64

typedef struct {
    float *data;
    int rows, cols, alive;
} Scratch;

static Scratch g_scratch[MAX_SCRATCH];

static int alloc_scratch(int rows, int cols) {
    for (int i = 0; i < MAX_SCRATCH; i++) {
        if (!g_scratch[i].alive) {
            g_scratch[i].data = (float *)calloc((size_t)rows * cols, sizeof(float));
            if (!g_scratch[i].data) return -1;
            g_scratch[i].rows = rows;
            g_scratch[i].cols = cols;
            g_scratch[i].alive = 1;
            return i;
        }
    }
    return -1;
}

/* ── Math helpers ── */

static void matmul(const float *A, const float *B, float *C, int M, int N, int K) {
    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++) {
            float s = 0.0f;
            for (int k = 0; k < K; k++) s += A[i*K+k] * B[k*N+j];
            C[i*N+j] = s;
        }
}

static float silu(float x) {
    return x / (1.0f + expf(-x));
}

static float softplus(float x) {
    return logf(1.0f + expf(x));
}

static void layer_norm(float *out, const float *x, const float *g,
                       const float *b, int rows, int cols, float eps) {
    for (int r = 0; r < rows; r++) {
        float mean = 0.0f, var = 0.0f;
        for (int c = 0; c < cols; c++) mean += x[r*cols+c];
        mean /= (float)cols;
        for (int c = 0; c < cols; c++) {
            float d = x[r*cols+c] - mean;
            var += d * d;
        }
        var /= (float)cols;
        float inv = 1.0f / sqrtf(var + eps);
        for (int c = 0; c < cols; c++)
            out[r*cols+c] = g[c] * (x[r*cols+c] - mean) * inv + b[c];
    }
}

/* ═══════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════ */

AriaString aria_mamba_last_error(void) { return make_aria_string(g_err); }

void aria_mamba_set_seed(int32_t seed) { set_seed((uint32_t)seed); }

int64_t aria_mamba_create(int32_t d_model, int32_t d_inner, int32_t d_state,
                          int32_t dt_rank, int32_t n_layers, int32_t conv_k,
                          int32_t vocab_size, int32_t max_seq) {
    int idx = -1;
    for (int i = 0; i < MAX_MODELS; i++)
        if (!g_models[i].alive) { idx = i; break; }
    if (idx < 0) { set_error("No free model slots"); return -1; }

    MambaModel *m = &g_models[idx];
    memset(m, 0, sizeof(*m));
    m->alive = 1;
    m->d_model = d_model;
    m->d_inner = d_inner;
    m->d_state = d_state;
    m->dt_rank = dt_rank;
    m->n_layers = n_layers;
    m->vocab_size = vocab_size;
    m->max_seq = max_seq;
    m->conv_k = conv_k;

    int dm = d_model, di = d_inner, ds = d_state, dr = dt_rank, ck = conv_k;

    m->token_emb = (float *)calloc((size_t)vocab_size * dm, sizeof(float));
    xavier(m->token_emb, vocab_size, dm, vocab_size * dm);

    m->layers = (MambaLayer *)calloc(n_layers, sizeof(MambaLayer));
    for (int l = 0; l < n_layers; l++) {
        MambaLayer *ly = &m->layers[l];

        ly->in_proj    = (float *)calloc((size_t)dm * (2*di), sizeof(float));
        ly->out_proj   = (float *)calloc((size_t)di * dm, sizeof(float));
        xavier(ly->in_proj, dm, 2*di, dm * 2*di);
        xavier(ly->out_proj, di, dm, di * dm);

        ly->conv_w     = (float *)calloc((size_t)di * ck, sizeof(float));
        ly->conv_b     = (float *)calloc(di, sizeof(float));
        xavier(ly->conv_w, ck, di, di * ck);

        int ssm_proj_cols = dr + 2 * ds;
        ly->x_proj     = (float *)calloc((size_t)di * ssm_proj_cols, sizeof(float));
        xavier(ly->x_proj, di, ssm_proj_cols, di * ssm_proj_cols);

        ly->dt_proj_w  = (float *)calloc((size_t)dr * di, sizeof(float));
        ly->dt_proj_b  = (float *)calloc(di, sizeof(float));
        xavier(ly->dt_proj_w, dr, di, dr * di);
        /* Initialize dt bias with uniform values for stability */
        for (int i = 0; i < di; i++)
            ly->dt_proj_b[i] = 0.01f * (float)(i % 7 + 1);

        ly->A_log      = (float *)calloc((size_t)di * ds, sizeof(float));
        ly->D          = (float *)calloc(di, sizeof(float));
        /* Initialize A_log as log of negative diagonal (S4 style) */
        for (int i = 0; i < di; i++) {
            for (int j = 0; j < ds; j++)
                ly->A_log[i * ds + j] = logf((float)(j + 1));
            ly->D[i] = 1.0f;
        }

        ly->ln_g = (float *)malloc(dm * sizeof(float));
        ly->ln_b = (float *)calloc(dm, sizeof(float));
        for (int i = 0; i < dm; i++) ly->ln_g[i] = 1.0f;
    }

    m->ln_final_g = (float *)malloc(dm * sizeof(float));
    m->ln_final_b = (float *)calloc(dm, sizeof(float));
    for (int i = 0; i < dm; i++) m->ln_final_g[i] = 1.0f;

    return (int64_t)idx;
}

void aria_mamba_destroy(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return;
    MambaModel *m = &g_models[id];
    free(m->token_emb);
    for (int l = 0; l < m->n_layers; l++) {
        MambaLayer *ly = &m->layers[l];
        free(ly->in_proj); free(ly->out_proj);
        free(ly->conv_w); free(ly->conv_b);
        free(ly->x_proj); free(ly->dt_proj_w); free(ly->dt_proj_b);
        free(ly->A_log); free(ly->D);
        free(ly->ln_g); free(ly->ln_b);
    }
    free(m->layers);
    free(m->ln_final_g); free(m->ln_final_b);
    m->alive = 0;
}

int32_t aria_mamba_d_model(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].d_model;
}
int32_t aria_mamba_d_inner(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].d_inner;
}
int32_t aria_mamba_d_state(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].d_state;
}
int32_t aria_mamba_n_layers(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].n_layers;
}
int32_t aria_mamba_vocab_size(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].vocab_size;
}

int64_t aria_mamba_param_count(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    MambaModel *m = &g_models[id];
    int64_t dm = m->d_model, di = m->d_inner, ds = m->d_state;
    int64_t dr = m->dt_rank, ck = m->conv_k, vs = m->vocab_size;
    int64_t emb = vs * dm;
    int64_t per_layer = dm * 2*di + di * dm +         /* in/out proj */
                        di * ck + di +                  /* conv */
                        di * (dr + 2*ds) +              /* x_proj */
                        dr * di + di +                  /* dt proj */
                        di * ds + di +                  /* A_log + D */
                        2 * dm;                         /* layer norm */
    return emb + (int64_t)m->n_layers * per_layer + 2 * dm;
}

/* Scratch management (shared pattern) */
int64_t aria_mamba_scratch_create(int32_t rows, int32_t cols) {
    int id = alloc_scratch(rows, cols);
    if (id < 0) { set_error("No free scratch"); return -1; }
    return (int64_t)id;
}
void aria_mamba_scratch_destroy(int64_t id) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return;
    free(g_scratch[id].data); g_scratch[id].data = NULL;
    g_scratch[id].alive = 0;
}
void aria_mamba_scratch_set(int64_t id, int32_t idx, double val) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return;
    int total = g_scratch[id].rows * g_scratch[id].cols;
    if (idx >= 0 && idx < total) g_scratch[id].data[idx] = (float)val;
}
double aria_mamba_scratch_get(int64_t id, int32_t idx) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return 0.0;
    int total = g_scratch[id].rows * g_scratch[id].cols;
    if (idx >= 0 && idx < total) return (double)g_scratch[id].data[idx];
    return 0.0;
}
int32_t aria_mamba_scratch_rows(int64_t id) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return 0;
    return g_scratch[id].rows;
}
int32_t aria_mamba_scratch_cols(int64_t id) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return 0;
    return g_scratch[id].cols;
}

/* ── Forward pass ──
 * Input: scratch of [seq_len × 1] token IDs
 * Output: scratch of [seq_len × vocab_size] logits
 */
int64_t aria_mamba_forward(int64_t model_id, int64_t input_id) {
    if (model_id < 0 || model_id >= MAX_MODELS || !g_models[model_id].alive) {
        set_error("Invalid model"); return -1;
    }
    if (input_id < 0 || input_id >= MAX_SCRATCH || !g_scratch[input_id].alive) {
        set_error("Invalid input"); return -1;
    }

    MambaModel *m = &g_models[model_id];
    int seq = g_scratch[input_id].rows;
    int dm = m->d_model, di = m->d_inner, ds = m->d_state;
    int dr = m->dt_rank, ck = m->conv_k;

    if (seq > m->max_seq) { set_error("Sequence too long"); return -1; }

    /* x = [seq × dm] */
    float *x = (float *)malloc((size_t)seq * dm * sizeof(float));
    float *resid = (float *)malloc((size_t)seq * dm * sizeof(float));
    float *normed = (float *)malloc((size_t)seq * dm * sizeof(float));

    /* Embedding */
    for (int t = 0; t < seq; t++) {
        int tok = (int)g_scratch[input_id].data[t];
        if (tok < 0) tok = 0;
        if (tok >= m->vocab_size) tok = m->vocab_size - 1;
        memcpy(x + t * dm, m->token_emb + tok * dm, dm * sizeof(float));
    }

    /* Allocate work buffers for mamba block */
    float *proj = (float *)malloc((size_t)seq * 2 * di * sizeof(float));  /* in_proj output */
    float *xz_x = (float *)malloc((size_t)seq * di * sizeof(float));     /* first half */
    float *xz_z = (float *)malloc((size_t)seq * di * sizeof(float));     /* second half (gate) */
    float *conv_out = (float *)malloc((size_t)seq * di * sizeof(float));
    float *ssm_proj = (float *)malloc((size_t)seq * (dr + 2*ds) * sizeof(float));
    float *dt_raw = (float *)malloc((size_t)seq * dr * sizeof(float));
    float *dt = (float *)malloc((size_t)seq * di * sizeof(float));
    float *B_mat = (float *)malloc((size_t)seq * ds * sizeof(float));
    float *C_mat = (float *)malloc((size_t)seq * ds * sizeof(float));
    float *ssm_out = (float *)malloc((size_t)seq * di * sizeof(float));
    float *state = (float *)calloc((size_t)di * ds, sizeof(float));  /* SSM hidden state */
    float *out_buf = (float *)malloc((size_t)seq * dm * sizeof(float));

    for (int l = 0; l < m->n_layers; l++) {
        MambaLayer *ly = &m->layers[l];

        /* Residual */
        memcpy(resid, x, (size_t)seq * dm * sizeof(float));

        /* Layer norm */
        layer_norm(normed, x, ly->ln_g, ly->ln_b, seq, dm, 1e-5f);

        /* In projection: [seq × dm] × [dm × 2di] → [seq × 2di] */
        matmul(normed, ly->in_proj, proj, seq, 2*di, dm);

        /* Split into x_branch and z (gate) */
        for (int t = 0; t < seq; t++) {
            memcpy(xz_x + t*di, proj + t*2*di,       di * sizeof(float));
            memcpy(xz_z + t*di, proj + t*2*di + di,   di * sizeof(float));
        }

        /* 1D causal convolution (depthwise) with padding */
        for (int t = 0; t < seq; t++) {
            for (int d = 0; d < di; d++) {
                float s = ly->conv_b[d];
                for (int k = 0; k < ck; k++) {
                    int src_t = t - k;
                    if (src_t >= 0)
                        s += xz_x[src_t * di + d] * ly->conv_w[d * ck + k];
                }
                conv_out[t * di + d] = silu(s);
            }
        }

        /* SSM parameter projection: [seq × di] × [di × (dr+2ds)] → [seq × (dr+2ds)] */
        int ssm_cols = dr + 2*ds;
        matmul(conv_out, ly->x_proj, ssm_proj, seq, ssm_cols, di);

        /* Split into dt_raw[seq×dr], B[seq×ds], C[seq×ds] */
        for (int t = 0; t < seq; t++) {
            memcpy(dt_raw + t*dr,  ssm_proj + t*ssm_cols,            dr * sizeof(float));
            memcpy(B_mat + t*ds,   ssm_proj + t*ssm_cols + dr,       ds * sizeof(float));
            memcpy(C_mat + t*ds,   ssm_proj + t*ssm_cols + dr + ds,  ds * sizeof(float));
        }

        /* dt projection: [seq×dr] × [dr×di] → [seq×di], then softplus */
        matmul(dt_raw, ly->dt_proj_w, dt, seq, di, dr);
        for (int t = 0; t < seq; t++)
            for (int d = 0; d < di; d++) {
                dt[t*di+d] = softplus(dt[t*di+d] + ly->dt_proj_b[d]);
            }

        /* Selective scan (SSM recurrence) */
        memset(state, 0, (size_t)di * ds * sizeof(float));
        for (int t = 0; t < seq; t++) {
            for (int d = 0; d < di; d++) {
                float dt_val = dt[t*di+d];
                float x_val = conv_out[t*di+d];
                float y = 0.0f;

                for (int s = 0; s < ds; s++) {
                    float A = -expf(ly->A_log[d*ds+s]);
                    float dA = expf(dt_val * A);
                    float dB = dt_val * B_mat[t*ds+s];
                    state[d*ds+s] = dA * state[d*ds+s] + dB * x_val;
                    y += state[d*ds+s] * C_mat[t*ds+s];
                }
                /* Add D*x (skip connection) */
                ssm_out[t*di+d] = y + ly->D[d] * x_val;
            }
        }

        /* Gate with z (SiLU) and multiply */
        for (int t = 0; t < seq; t++)
            for (int d = 0; d < di; d++)
                ssm_out[t*di+d] *= silu(xz_z[t*di+d]);

        /* Output projection: [seq×di] × [di×dm] → [seq×dm] */
        matmul(ssm_out, ly->out_proj, out_buf, seq, dm, di);

        /* Residual add */
        for (int i = 0; i < seq * dm; i++)
            x[i] = resid[i] + out_buf[i];
    }

    /* Final layer norm */
    layer_norm(normed, x, m->ln_final_g, m->ln_final_b, seq, dm, 1e-5f);

    /* Logits (weight-tied unembedding) */
    int out_id = alloc_scratch(seq, m->vocab_size);
    if (out_id < 0) {
        set_error("No scratch for logits");
        free(x); free(resid); free(normed); free(proj);
        free(xz_x); free(xz_z); free(conv_out); free(ssm_proj);
        free(dt_raw); free(dt); free(B_mat); free(C_mat);
        free(ssm_out); free(state); free(out_buf);
        return -1;
    }
    float *logits = g_scratch[out_id].data;
    for (int t = 0; t < seq; t++)
        for (int v = 0; v < m->vocab_size; v++) {
            float dot = 0.0f;
            for (int d = 0; d < dm; d++)
                dot += normed[t*dm+d] * m->token_emb[v*dm+d];
            logits[t * m->vocab_size + v] = dot;
        }

    free(x); free(resid); free(normed); free(proj);
    free(xz_x); free(xz_z); free(conv_out); free(ssm_proj);
    free(dt_raw); free(dt); free(B_mat); free(C_mat);
    free(ssm_out); free(state); free(out_buf);
    return (int64_t)out_id;
}

/* Argmax */
int32_t aria_mamba_argmax(int64_t sid, int32_t row) {
    if (sid < 0 || sid >= MAX_SCRATCH || !g_scratch[sid].alive) return -1;
    Scratch *s = &g_scratch[sid];
    if (row < 0 || row >= s->rows) return -1;
    float *r = s->data + row * s->cols;
    int best = 0;
    for (int i = 1; i < s->cols; i++) if (r[i] > r[best]) best = i;
    return (int32_t)best;
}

/* Softmax on row */
void aria_mamba_softmax_row(int64_t sid, int32_t row) {
    if (sid < 0 || sid >= MAX_SCRATCH || !g_scratch[sid].alive) return;
    Scratch *s = &g_scratch[sid];
    if (row < 0 || row >= s->rows) return;
    float *r = s->data + row * s->cols;
    float mx = r[0];
    for (int i = 1; i < s->cols; i++) if (r[i] > mx) mx = r[i];
    float sum = 0.0f;
    for (int i = 0; i < s->cols; i++) { r[i] = expf(r[i] - mx); sum += r[i]; }
    float inv = 1.0f / sum;
    for (int i = 0; i < s->cols; i++) r[i] *= inv;
}
