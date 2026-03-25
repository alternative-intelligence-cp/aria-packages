/*
 * aria_transformer_shim.c — Transformer model implementation
 *
 * Self-contained transformer encoder with:
 *   - Token + positional embeddings
 *   - Multi-head scaled dot-product attention
 *   - Feed-forward network (GELU activation)
 *   - Layer normalization + residual connections
 *   - Output logits via unembedding
 *
 * All weights managed internally. Handle-based API for Aria.
 *
 * Compile:
 *   gcc -shared -fPIC -O2 -o libaria_transformer_shim.so aria_transformer_shim.c -lm
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

/* ── RNG (xorshift64) ── */
static uint64_t g_rng_state = 0xDEADBEEFCAFEBABEULL;

static void set_rng_seed(uint32_t seed) {
    g_rng_state = (uint64_t)seed * 6364136223846793005ULL + 1442695040888963407ULL;
    if (g_rng_state == 0) g_rng_state = 1;
}

static float rand_normal(void) {
    /* Box-Muller from xorshift64 */
    g_rng_state ^= g_rng_state << 13;
    g_rng_state ^= g_rng_state >> 7;
    g_rng_state ^= g_rng_state << 17;
    float u1 = (float)(g_rng_state & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    g_rng_state ^= g_rng_state << 13;
    g_rng_state ^= g_rng_state >> 7;
    g_rng_state ^= g_rng_state << 17;
    float u2 = (float)(g_rng_state & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

static void xavier_init(float *w, int fan_in, int fan_out, int n) {
    float scale = sqrtf(2.0f / (float)(fan_in + fan_out));
    for (int i = 0; i < n; i++) w[i] = rand_normal() * scale;
}

/* ── Transformer layer weights ── */
typedef struct {
    float *wq, *wk, *wv, *wo;  /* [d_model × d_model] */
    float *ff1, *ff2;           /* [d_model × d_ff], [d_ff × d_model] */
    float *ff_b1, *ff_b2;      /* [d_ff], [d_model] */
    float *ln1_g, *ln1_b;      /* pre-attention layer norm */
    float *ln2_g, *ln2_b;      /* pre-FFN layer norm */
} TransformerLayer;

/* ── Transformer model ── */
#define MAX_MODELS 64

typedef struct {
    int    alive;
    int    d_model, n_heads, n_layers, d_ff;
    int    vocab_size, max_seq;
    int    head_dim;
    float  scale;         /* 1/sqrt(head_dim) */

    /* Embeddings */
    float *token_emb;     /* [vocab_size × d_model] */
    float *pos_emb;       /* [max_seq × d_model] */

    /* Layers */
    TransformerLayer *layers;

    /* Final layer norm */
    float *ln_final_g, *ln_final_b;  /* [d_model] */
} TransformerModel;

static TransformerModel g_models[MAX_MODELS];
static char g_last_error[512] = "";

static void set_error(const char *msg) {
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
}

/* ── Tensor scratch pool (for intermediate results) ── */
#define MAX_SCRATCH 64

typedef struct {
    float *data;
    int    rows, cols;
    int    alive;
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

/* ── Core math ops ── */

static void matmul(const float *A, const float *B, float *C,
                   int M, int N, int K) {
    /* C[M×N] = A[M×K] × B[K×N] */
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++)
                sum += A[i * K + k] * B[k * N + j];
            C[i * N + j] = sum;
        }
    }
}

static void add_vec(float *dst, const float *src, int n) {
    for (int i = 0; i < n; i++) dst[i] += src[i];
}

static void add_bias_rows(float *mat, const float *bias, int rows, int cols) {
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            mat[r * cols + c] += bias[c];
}

static void layer_norm(float *out, const float *x, const float *g,
                       const float *b, int rows, int cols, float eps) {
    for (int r = 0; r < rows; r++) {
        const float *row = x + r * cols;
        float *orow = out + r * cols;
        float mean = 0.0f;
        for (int c = 0; c < cols; c++) mean += row[c];
        mean /= (float)cols;
        float var = 0.0f;
        for (int c = 0; c < cols; c++) {
            float d = row[c] - mean;
            var += d * d;
        }
        var /= (float)cols;
        float inv = 1.0f / sqrtf(var + eps);
        for (int c = 0; c < cols; c++)
            orow[c] = g[c] * (row[c] - mean) * inv + b[c];
    }
}

static void gelu(float *x, int n) {
    for (int i = 0; i < n; i++) {
        float v = x[i];
        x[i] = 0.5f * v * (1.0f + tanhf(0.7978845608f * (v + 0.044715f * v * v * v)));
    }
}

static void softmax_rows(float *mat, int rows, int cols) {
    for (int r = 0; r < rows; r++) {
        float *row = mat + r * cols;
        float mx = row[0];
        for (int c = 1; c < cols; c++) if (row[c] > mx) mx = row[c];
        float sum = 0.0f;
        for (int c = 0; c < cols; c++) {
            row[c] = expf(row[c] - mx);
            sum += row[c];
        }
        float inv = 1.0f / sum;
        for (int c = 0; c < cols; c++) row[c] *= inv;
    }
}

static void apply_causal_mask(float *attn, int seq_len) {
    /* attn is [seq_len × seq_len], mask future positions with -inf */
    for (int i = 0; i < seq_len; i++)
        for (int j = i + 1; j < seq_len; j++)
            attn[i * seq_len + j] = -1e9f;
}

/* ═══════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════ */

AriaString aria_transformer_last_error(void) {
    return make_aria_string(g_last_error);
}

void aria_transformer_set_seed(int32_t seed) {
    set_rng_seed((uint32_t)seed);
}

/* Create a transformer model */
int64_t aria_transformer_create(int32_t d_model, int32_t n_heads,
                                int32_t n_layers, int32_t d_ff,
                                int32_t vocab_size, int32_t max_seq) {
    if (d_model <= 0 || n_heads <= 0 || d_model % n_heads != 0) {
        set_error("d_model must be positive and divisible by n_heads");
        return -1;
    }

    int idx = -1;
    for (int i = 0; i < MAX_MODELS; i++) {
        if (!g_models[i].alive) { idx = i; break; }
    }
    if (idx < 0) { set_error("No free model slots"); return -1; }

    TransformerModel *m = &g_models[idx];
    memset(m, 0, sizeof(*m));
    m->alive = 1;
    m->d_model = d_model;
    m->n_heads = n_heads;
    m->n_layers = n_layers;
    m->d_ff = d_ff;
    m->vocab_size = vocab_size;
    m->max_seq = max_seq;
    m->head_dim = d_model / n_heads;
    m->scale = 1.0f / sqrtf((float)m->head_dim);

    int dm = d_model, vs = vocab_size, ms = max_seq, df = d_ff;

    /* Token embedding */
    m->token_emb = (float *)calloc((size_t)vs * dm, sizeof(float));
    xavier_init(m->token_emb, vs, dm, vs * dm);

    /* Positional embedding (learnable) */
    m->pos_emb = (float *)calloc((size_t)ms * dm, sizeof(float));
    xavier_init(m->pos_emb, ms, dm, ms * dm);

    /* Layers */
    m->layers = (TransformerLayer *)calloc(n_layers, sizeof(TransformerLayer));
    for (int l = 0; l < n_layers; l++) {
        TransformerLayer *ly = &m->layers[l];
        ly->wq  = (float *)calloc((size_t)dm * dm, sizeof(float));
        ly->wk  = (float *)calloc((size_t)dm * dm, sizeof(float));
        ly->wv  = (float *)calloc((size_t)dm * dm, sizeof(float));
        ly->wo  = (float *)calloc((size_t)dm * dm, sizeof(float));
        xavier_init(ly->wq, dm, dm, dm * dm);
        xavier_init(ly->wk, dm, dm, dm * dm);
        xavier_init(ly->wv, dm, dm, dm * dm);
        xavier_init(ly->wo, dm, dm, dm * dm);

        ly->ff1   = (float *)calloc((size_t)dm * df, sizeof(float));
        ly->ff2   = (float *)calloc((size_t)df * dm, sizeof(float));
        ly->ff_b1 = (float *)calloc(df, sizeof(float));
        ly->ff_b2 = (float *)calloc(dm, sizeof(float));
        xavier_init(ly->ff1, dm, df, dm * df);
        xavier_init(ly->ff2, df, dm, df * dm);

        ly->ln1_g = (float *)malloc(dm * sizeof(float));
        ly->ln1_b = (float *)calloc(dm, sizeof(float));
        ly->ln2_g = (float *)malloc(dm * sizeof(float));
        ly->ln2_b = (float *)calloc(dm, sizeof(float));
        for (int i = 0; i < dm; i++) {
            ly->ln1_g[i] = 1.0f;
            ly->ln2_g[i] = 1.0f;
        }
    }

    /* Final layer norm */
    m->ln_final_g = (float *)malloc(dm * sizeof(float));
    m->ln_final_b = (float *)calloc(dm, sizeof(float));
    for (int i = 0; i < dm; i++) m->ln_final_g[i] = 1.0f;

    return (int64_t)idx;
}

void aria_transformer_destroy(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return;
    TransformerModel *m = &g_models[id];
    free(m->token_emb);
    free(m->pos_emb);
    for (int l = 0; l < m->n_layers; l++) {
        TransformerLayer *ly = &m->layers[l];
        free(ly->wq);  free(ly->wk);  free(ly->wv);  free(ly->wo);
        free(ly->ff1); free(ly->ff2); free(ly->ff_b1); free(ly->ff_b2);
        free(ly->ln1_g); free(ly->ln1_b);
        free(ly->ln2_g); free(ly->ln2_b);
    }
    free(m->layers);
    free(m->ln_final_g); free(m->ln_final_b);
    m->alive = 0;
}

/* Get model info */
int32_t aria_transformer_d_model(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].d_model;
}

int32_t aria_transformer_n_heads(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].n_heads;
}

int32_t aria_transformer_n_layers(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].n_layers;
}

int32_t aria_transformer_vocab_size(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    return (int32_t)g_models[id].vocab_size;
}

int64_t aria_transformer_param_count(int64_t id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].alive) return 0;
    TransformerModel *m = &g_models[id];
    int64_t dm = m->d_model, df = m->d_ff, vs = m->vocab_size, ms = m->max_seq;
    int64_t emb = vs * dm + ms * dm;
    int64_t per_layer = 4 * dm * dm + 2 * dm * df + df + dm + 4 * dm;
    int64_t final_ln = 2 * dm;
    return emb + (int64_t)m->n_layers * per_layer + final_ln;
}

/* ── Forward pass ──
 * Input: scratch_id of [seq_len × 1] float tensor with token IDs (as floats)
 * Output: scratch_id of [seq_len × vocab_size] logits
 */

/* Helper: alloc scratch and expose */
int64_t aria_transformer_scratch_create(int32_t rows, int32_t cols) {
    int id = alloc_scratch(rows, cols);
    if (id < 0) { set_error("No free scratch slots"); return -1; }
    return (int64_t)id;
}

void aria_transformer_scratch_destroy(int64_t id) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return;
    free(g_scratch[id].data);
    g_scratch[id].data = NULL;
    g_scratch[id].alive = 0;
}

void aria_transformer_scratch_set(int64_t id, int32_t idx, double val) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return;
    int total = g_scratch[id].rows * g_scratch[id].cols;
    if (idx < 0 || idx >= total) return;
    g_scratch[id].data[idx] = (float)val;
}

double aria_transformer_scratch_get(int64_t id, int32_t idx) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return 0.0;
    int total = g_scratch[id].rows * g_scratch[id].cols;
    if (idx < 0 || idx >= total) return 0.0;
    return (double)g_scratch[id].data[idx];
}

int32_t aria_transformer_scratch_rows(int64_t id) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return 0;
    return (int32_t)g_scratch[id].rows;
}

int32_t aria_transformer_scratch_cols(int64_t id) {
    if (id < 0 || id >= MAX_SCRATCH || !g_scratch[id].alive) return 0;
    return (int32_t)g_scratch[id].cols;
}

/* Forward pass */
int64_t aria_transformer_forward(int64_t model_id, int64_t input_scratch_id,
                                 int32_t use_causal_mask) {
    if (model_id < 0 || model_id >= MAX_MODELS || !g_models[model_id].alive) {
        set_error("Invalid model ID");
        return -1;
    }
    if (input_scratch_id < 0 || input_scratch_id >= MAX_SCRATCH ||
        !g_scratch[input_scratch_id].alive) {
        set_error("Invalid input scratch ID");
        return -1;
    }

    TransformerModel *m = &g_models[model_id];
    Scratch *inp = &g_scratch[input_scratch_id];
    int seq_len = inp->rows;
    int dm = m->d_model;

    if (seq_len > m->max_seq) {
        set_error("Sequence length exceeds max_seq");
        return -1;
    }

    /* Work buffers */
    int total = seq_len * dm;
    float *x     = (float *)malloc(total * sizeof(float));
    float *resid = (float *)malloc(total * sizeof(float));
    float *tmp   = (float *)malloc(total * sizeof(float));
    float *Q     = (float *)malloc(total * sizeof(float));
    float *K     = (float *)malloc(total * sizeof(float));
    float *V     = (float *)malloc(total * sizeof(float));
    int attn_sz = seq_len * seq_len;
    float *attn  = (float *)malloc(attn_sz * sizeof(float));
    float *attn_out = (float *)malloc(total * sizeof(float));
    float *ff_hidden = (float *)malloc((size_t)seq_len * m->d_ff * sizeof(float));

    if (!x || !resid || !tmp || !Q || !K || !V || !attn || !attn_out || !ff_hidden) {
        set_error("OOM in forward pass");
        free(x); free(resid); free(tmp); free(Q); free(K); free(V);
        free(attn); free(attn_out); free(ff_hidden);
        return -1;
    }

    /* 1. Embedding lookup: token_emb[token_id] + pos_emb[position] */
    for (int t = 0; t < seq_len; t++) {
        int token = (int)inp->data[t];
        if (token < 0) token = 0;
        if (token >= m->vocab_size) token = m->vocab_size - 1;
        for (int d = 0; d < dm; d++) {
            x[t * dm + d] = m->token_emb[token * dm + d] +
                            m->pos_emb[t * dm + d];
        }
    }

    /* 2. Transformer blocks */
    for (int l = 0; l < m->n_layers; l++) {
        TransformerLayer *ly = &m->layers[l];

        /* Save residual */
        memcpy(resid, x, total * sizeof(float));

        /* Pre-attention layer norm */
        layer_norm(tmp, x, ly->ln1_g, ly->ln1_b, seq_len, dm, 1e-5f);

        /* Q, K, V projections: [seq × dm] × [dm × dm] -> [seq × dm] */
        matmul(tmp, ly->wq, Q, seq_len, dm, dm);
        matmul(tmp, ly->wk, K, seq_len, dm, dm);
        matmul(tmp, ly->wv, V, seq_len, dm, dm);

        /* Multi-head attention */
        int hd = m->head_dim;
        memset(attn_out, 0, total * sizeof(float));

        for (int h = 0; h < m->n_heads; h++) {
            int off = h * hd;
            /* Compute attention scores for this head */
            for (int i = 0; i < seq_len; i++) {
                for (int j = 0; j < seq_len; j++) {
                    float score = 0.0f;
                    for (int d = 0; d < hd; d++)
                        score += Q[i * dm + off + d] * K[j * dm + off + d];
                    attn[i * seq_len + j] = score * m->scale;
                }
            }

            /* Causal mask if requested */
            if (use_causal_mask) apply_causal_mask(attn, seq_len);

            /* Softmax per row */
            softmax_rows(attn, seq_len, seq_len);

            /* Weighted sum of V */
            for (int i = 0; i < seq_len; i++) {
                for (int d = 0; d < hd; d++) {
                    float sum = 0.0f;
                    for (int j = 0; j < seq_len; j++)
                        sum += attn[i * seq_len + j] * V[j * dm + off + d];
                    attn_out[i * dm + off + d] = sum;
                }
            }
        }

        /* Output projection */
        matmul(attn_out, ly->wo, tmp, seq_len, dm, dm);

        /* Residual add */
        for (int i = 0; i < total; i++) x[i] = resid[i] + tmp[i];

        /* Save residual for FFN */
        memcpy(resid, x, total * sizeof(float));

        /* Pre-FFN layer norm */
        layer_norm(tmp, x, ly->ln2_g, ly->ln2_b, seq_len, dm, 1e-5f);

        /* FFN: ff1 (dm→d_ff) + GELU + ff2 (d_ff→dm) */
        matmul(tmp, ly->ff1, ff_hidden, seq_len, m->d_ff, dm);
        add_bias_rows(ff_hidden, ly->ff_b1, seq_len, m->d_ff);
        gelu(ff_hidden, seq_len * m->d_ff);
        matmul(ff_hidden, ly->ff2, tmp, seq_len, dm, m->d_ff);
        add_bias_rows(tmp, ly->ff_b2, seq_len, dm);

        /* Residual add */
        for (int i = 0; i < total; i++) x[i] = resid[i] + tmp[i];
    }

    /* 3. Final layer norm */
    layer_norm(tmp, x, m->ln_final_g, m->ln_final_b, seq_len, dm, 1e-5f);

    /* 4. Logits: [seq × dm] × [dm × vocab] = [seq × vocab]
     * Use token_emb transposed as unembed (weight tying) */
    int out_id = alloc_scratch(seq_len, m->vocab_size);
    if (out_id < 0) {
        set_error("No scratch for output logits");
        free(x); free(resid); free(tmp); free(Q); free(K); free(V);
        free(attn); free(attn_out); free(ff_hidden);
        return -1;
    }
    float *logits = g_scratch[out_id].data;
    /* tmp × token_emb^T  =>  [seq×dm] × [dm×vocab]
     * token_emb is [vocab×dm], so B^T[d,v] = token_emb[v,d] */
    for (int i = 0; i < seq_len; i++) {
        for (int v = 0; v < m->vocab_size; v++) {
            float dot = 0.0f;
            for (int d = 0; d < dm; d++)
                dot += tmp[i * dm + d] * m->token_emb[v * dm + d];
            logits[i * m->vocab_size + v] = dot;
        }
    }

    free(x); free(resid); free(tmp); free(Q); free(K); free(V);
    free(attn); free(attn_out); free(ff_hidden);
    return (int64_t)out_id;
}

/* ── Attention visualization ──
 * Returns [seq_len × seq_len] attention weights for a given layer+head */
int64_t aria_transformer_get_attention(int64_t model_id, int64_t input_scratch_id,
                                       int32_t layer_idx, int32_t head_idx,
                                       int32_t use_causal_mask) {
    if (model_id < 0 || model_id >= MAX_MODELS || !g_models[model_id].alive) {
        set_error("Invalid model ID"); return -1;
    }
    if (input_scratch_id < 0 || input_scratch_id >= MAX_SCRATCH ||
        !g_scratch[input_scratch_id].alive) {
        set_error("Invalid input scratch ID"); return -1;
    }

    TransformerModel *m = &g_models[model_id];
    Scratch *inp = &g_scratch[input_scratch_id];
    int seq_len = inp->rows;
    int dm = m->d_model;
    int hd = m->head_dim;

    if (layer_idx < 0 || layer_idx >= m->n_layers) {
        set_error("Layer index out of range"); return -1;
    }
    if (head_idx < 0 || head_idx >= m->n_heads) {
        set_error("Head index out of range"); return -1;
    }

    /* Run through embeddings + layer norms up to target layer */
    int total = seq_len * dm;
    float *x   = (float *)malloc(total * sizeof(float));
    float *tmp = (float *)malloc(total * sizeof(float));
    float *Q   = (float *)malloc(total * sizeof(float));
    float *K   = (float *)malloc(total * sizeof(float));
    float *V   = (float *)malloc(total * sizeof(float));

    /* Embedding */
    for (int t = 0; t < seq_len; t++) {
        int token = (int)inp->data[t];
        if (token < 0) token = 0;
        if (token >= m->vocab_size) token = m->vocab_size - 1;
        for (int d = 0; d < dm; d++)
            x[t * dm + d] = m->token_emb[token * dm + d] + m->pos_emb[t * dm + d];
    }

    /* Run through layers up to target */
    float *resid = (float *)malloc(total * sizeof(float));
    float *ff_buf = (float *)malloc((size_t)seq_len * m->d_ff * sizeof(float));
    int attn_sz = seq_len * seq_len;
    float *attn = (float *)malloc(attn_sz * sizeof(float));
    float *attn_out_buf = (float *)malloc(total * sizeof(float));

    for (int l = 0; l <= layer_idx; l++) {
        TransformerLayer *ly = &m->layers[l];
        memcpy(resid, x, total * sizeof(float));
        layer_norm(tmp, x, ly->ln1_g, ly->ln1_b, seq_len, dm, 1e-5f);
        matmul(tmp, ly->wq, Q, seq_len, dm, dm);
        matmul(tmp, ly->wk, K, seq_len, dm, dm);
        matmul(tmp, ly->wv, V, seq_len, dm, dm);

        if (l == layer_idx) {
            /* Extract attention weights for target head */
            int off = head_idx * hd;
            for (int i = 0; i < seq_len; i++) {
                for (int j = 0; j < seq_len; j++) {
                    float score = 0.0f;
                    for (int d = 0; d < hd; d++)
                        score += Q[i * dm + off + d] * K[j * dm + off + d];
                    attn[i * seq_len + j] = score * m->scale;
                }
            }
            if (use_causal_mask) apply_causal_mask(attn, seq_len);
            softmax_rows(attn, seq_len, seq_len);

            /* Store and return */
            int out_id = alloc_scratch(seq_len, seq_len);
            if (out_id >= 0)
                memcpy(g_scratch[out_id].data, attn, attn_sz * sizeof(float));

            free(x); free(tmp); free(Q); free(K); free(V);
            free(resid); free(ff_buf); free(attn); free(attn_out_buf);
            return out_id >= 0 ? (int64_t)out_id : -1;
        }

        /* Full attention for earlier layers */
        memset(attn_out_buf, 0, total * sizeof(float));
        for (int h = 0; h < m->n_heads; h++) {
            int off = h * hd;
            for (int i = 0; i < seq_len; i++) {
                for (int j = 0; j < seq_len; j++) {
                    float score = 0.0f;
                    for (int d = 0; d < hd; d++)
                        score += Q[i * dm + off + d] * K[j * dm + off + d];
                    attn[i * seq_len + j] = score * m->scale;
                }
            }
            if (use_causal_mask) apply_causal_mask(attn, seq_len);
            softmax_rows(attn, seq_len, seq_len);
            for (int i = 0; i < seq_len; i++)
                for (int d = 0; d < hd; d++) {
                    float sum = 0.0f;
                    for (int j = 0; j < seq_len; j++)
                        sum += attn[i * seq_len + j] * V[j * dm + off + d];
                    attn_out_buf[i * dm + off + d] = sum;
                }
        }
        matmul(attn_out_buf, ly->wo, tmp, seq_len, dm, dm);
        for (int i = 0; i < total; i++) x[i] = resid[i] + tmp[i];

        /* FFN */
        memcpy(resid, x, total * sizeof(float));
        layer_norm(tmp, x, ly->ln2_g, ly->ln2_b, seq_len, dm, 1e-5f);
        matmul(tmp, ly->ff1, ff_buf, seq_len, m->d_ff, dm);
        add_bias_rows(ff_buf, ly->ff_b1, seq_len, m->d_ff);
        gelu(ff_buf, seq_len * m->d_ff);
        matmul(ff_buf, ly->ff2, tmp, seq_len, dm, m->d_ff);
        add_bias_rows(tmp, ly->ff_b2, seq_len, dm);
        for (int i = 0; i < total; i++) x[i] = resid[i] + tmp[i];
    }

    free(x); free(tmp); free(Q); free(K); free(V);
    free(resid); free(ff_buf); free(attn); free(attn_out_buf);
    set_error("Layer not reached");
    return -1;
}

/* ── Argmax for prediction ── */
int32_t aria_transformer_argmax(int64_t scratch_id, int32_t row) {
    if (scratch_id < 0 || scratch_id >= MAX_SCRATCH ||
        !g_scratch[scratch_id].alive) return -1;
    Scratch *s = &g_scratch[scratch_id];
    if (row < 0 || row >= s->rows) return -1;
    float *r = s->data + row * s->cols;
    int best = 0;
    for (int i = 1; i < s->cols; i++)
        if (r[i] > r[best]) best = i;
    return (int32_t)best;
}

/* ── Softmax on logits (for probabilities) ── */
void aria_transformer_softmax_row(int64_t scratch_id, int32_t row) {
    if (scratch_id < 0 || scratch_id >= MAX_SCRATCH ||
        !g_scratch[scratch_id].alive) return;
    Scratch *s = &g_scratch[scratch_id];
    if (row < 0 || row >= s->rows) return;
    softmax_rows(s->data + row * s->cols, 1, s->cols);
}
