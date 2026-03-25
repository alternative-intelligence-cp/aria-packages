/*  aria_looping_shim.c  –  Looping / iterative-refinement model
 *
 *  Architecture (shared-weight "Universal Transformer" style):
 *    1. Embed tokens + positional encoding
 *    2. Repeat refinement block N times (shared weights):
 *         LayerNorm → Multi-Head Self-Attention → residual
 *         LayerNorm → FFN (SiLU) → residual
 *    3. Optional early stopping when delta L2 < threshold
 *    4. Unembed via weight-tied projection
 *
 *  Compile:  gcc -shared -fPIC -O2 -o libaria_looping_shim.so aria_looping_shim.c -lm
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { char *data; int64_t length; } AriaString;
static char errbuf[256] = "ok";
static uint64_t rng = 0xDEADBEEFCAFEULL;
static float xrand(void){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return (float)(rng&0x7FFFFFFF)/2147483647.0f; }
static float xavier(int fi, int fo){ float s=sqrtf(6.0f/(fi+fo)); return (xrand()*2-1)*s; }

/* ── Scratch pool ──────────────────────────────────────────────────────── */
#define MAX_SCRATCH 128
typedef struct { float *d; int rows,cols,alive; } Scratch;
static Scratch SP[MAX_SCRATCH];
static int64_t sc_alloc(int r,int c){
    for(int i=0;i<MAX_SCRATCH;i++) if(!SP[i].alive){
        SP[i].d=(float*)calloc((size_t)r*c,sizeof(float));
        SP[i].rows=r; SP[i].cols=c; SP[i].alive=1; return i;
    }
    return -1;
}

/* ── Model ─────────────────────────────────────────────────────────────── */
typedef struct {
    int d_model, n_heads, ffn_dim, max_iters, vocab_size, max_seq;
    float conv_thresh;   /* convergence threshold (0 = always run max_iters) */
    int last_iters_used; /* how many iterations the last forward() used */
    /* embeddings */
    float *tok_emb, *pos_emb;
    /* shared refinement block (single set of weights reused each iteration) */
    float *ln1_g, *ln1_b;       /* pre-attention LayerNorm */
    float *Wq, *Wk, *Wv, *Wo;  /* attention */
    float *ln2_g, *ln2_b;       /* pre-FFN LayerNorm */
    float *W1, *b1, *W2, *b2;   /* FFN: d_model → ffn_dim → d_model */
    /* iteration embedding (added each iteration to tell the model which pass) */
    float *iter_emb;             /* [max_iters × d_model] */
    int alive;
} LoopModel;

#define MAX_MODELS 32
static LoopModel MODELS[MAX_MODELS];

/* ── math ──────────────────────────────────────────────────────────────── */
static void layer_norm(float *out, const float *x, const float *g, const float *b, int n){
    float mu=0; for(int i=0;i<n;i++) mu+=x[i]; mu/=n;
    float var=0; for(int i=0;i<n;i++){ float d=x[i]-mu; var+=d*d; } var/=n;
    float inv=1.0f/sqrtf(var+1e-5f);
    for(int i=0;i<n;i++) out[i]=g[i]*(x[i]-mu)*inv+b[i];
}
static void softmax_vec(float *v, int n){
    float mx=-1e30f; for(int i=0;i<n;i++) if(v[i]>mx) mx=v[i];
    float s=0; for(int i=0;i<n;i++){ v[i]=expf(v[i]-mx); s+=v[i]; }
    for(int i=0;i<n;i++) v[i]/=s;
}
static float silu_f(float x){ return x/(1.0f+expf(-x)); }

/* ── single refinement iteration ───────────────────────────────────────── */
static void refine_iter(LoopModel *M, float *act, int seq, int iter_idx){
    int dm=M->d_model, nh=M->n_heads, dk=dm/nh;
    /* add iteration embedding */
    if(iter_idx < M->max_iters){
        for(int t=0;t<seq;t++)
            for(int d=0;d<dm;d++)
                act[t*dm+d] += M->iter_emb[iter_idx*dm+d];
    }
    /* Pre-attention LN */
    float *normed=(float*)malloc((size_t)seq*dm*sizeof(float));
    for(int t=0;t<seq;t++) layer_norm(normed+t*dm, act+t*dm, M->ln1_g, M->ln1_b, dm);
    /* Q K V projections */
    float *Q=(float*)malloc((size_t)seq*dm*sizeof(float));
    float *K=(float*)malloc((size_t)seq*dm*sizeof(float));
    float *V=(float*)malloc((size_t)seq*dm*sizeof(float));
    memset(Q,0,(size_t)seq*dm*sizeof(float));
    memset(K,0,(size_t)seq*dm*sizeof(float));
    memset(V,0,(size_t)seq*dm*sizeof(float));
    for(int i=0;i<seq;i++) for(int k=0;k<dm;k++){
        float n=normed[i*dm+k];
        for(int j=0;j<dm;j++){
            Q[i*dm+j]+=n*M->Wq[k*dm+j];
            K[i*dm+j]+=n*M->Wk[k*dm+j];
            V[i*dm+j]+=n*M->Wv[k*dm+j];
        }
    }
    /* Multi-head attention with causal mask */
    float *attn_out=(float*)calloc((size_t)seq*dm,sizeof(float));
    float scale=1.0f/sqrtf((float)dk);
    float *scores=(float*)malloc((size_t)seq*seq*sizeof(float));
    for(int h=0;h<nh;h++){
        for(int i=0;i<seq;i++) for(int j=0;j<seq;j++){
            float dot=0;
            for(int d=0;d<dk;d++) dot+=Q[i*dm+h*dk+d]*K[j*dm+h*dk+d];
            scores[i*seq+j] = (j>i) ? -1e9f : dot*scale;
        }
        for(int i=0;i<seq;i++) softmax_vec(scores+i*seq,seq);
        for(int i=0;i<seq;i++) for(int d=0;d<dk;d++){
            float v=0; for(int j=0;j<seq;j++) v+=scores[i*seq+j]*V[j*dm+h*dk+d];
            attn_out[i*dm+h*dk+d]=v;
        }
    }
    /* Wo projection + residual */
    for(int i=0;i<seq;i++) for(int j=0;j<dm;j++){
        float v=0; for(int k=0;k<dm;k++) v+=attn_out[i*dm+k]*M->Wo[k*dm+j];
        act[i*dm+j]+=v;
    }
    free(normed); free(Q); free(K); free(V); free(attn_out); free(scores);
    /* Pre-FFN LN */
    normed=(float*)malloc((size_t)seq*dm*sizeof(float));
    for(int t=0;t<seq;t++) layer_norm(normed+t*dm, act+t*dm, M->ln2_g, M->ln2_b, dm);
    /* FFN: W1 → SiLU → W2 + residual */
    int ffn=M->ffn_dim;
    float *hidden=(float*)malloc((size_t)seq*ffn*sizeof(float));
    for(int i=0;i<seq;i++) for(int f=0;f<ffn;f++){
        float v=M->b1[f]; for(int d=0;d<dm;d++) v+=normed[i*dm+d]*M->W1[d*ffn+f];
        hidden[i*ffn+f]=silu_f(v);
    }
    for(int i=0;i<seq;i++) for(int d=0;d<dm;d++){
        float v=M->b2[d]; for(int f=0;f<ffn;f++) v+=hidden[i*ffn+f]*M->W2[f*dm+d];
        act[i*dm+d]+=v;
    }
    free(normed); free(hidden);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ══════════════════════════════════════════════════════════════════════════ */
AriaString aria_looping_last_error(void){
    return (AriaString){errbuf,(int64_t)strlen(errbuf)};
}
void aria_looping_set_seed(int32_t s){ rng=(uint64_t)s*6364136223846793005ULL+1; }

int64_t aria_looping_create(int32_t d_model, int32_t n_heads, int32_t ffn_dim,
                            int32_t max_iters, int32_t vocab_size, int32_t max_seq){
    int slot=-1;
    for(int i=0;i<MAX_MODELS;i++) if(!MODELS[i].alive){ slot=i; break; }
    if(slot<0){ snprintf(errbuf,sizeof(errbuf),"no model slots"); return -1; }
    LoopModel *M=&MODELS[slot];
    memset(M,0,sizeof(LoopModel));
    M->d_model=d_model; M->n_heads=n_heads; M->ffn_dim=ffn_dim;
    M->max_iters=max_iters; M->vocab_size=vocab_size; M->max_seq=max_seq;
    M->conv_thresh=0; M->last_iters_used=0;
    /* embeddings */
    M->tok_emb=(float*)malloc((size_t)vocab_size*d_model*sizeof(float));
    M->pos_emb=(float*)malloc((size_t)max_seq*d_model*sizeof(float));
    for(int i=0;i<vocab_size*d_model;i++) M->tok_emb[i]=xavier(vocab_size,d_model);
    for(int i=0;i<max_seq*d_model;i++) M->pos_emb[i]=xavier(max_seq,d_model);
    /* iteration embeddings */
    M->iter_emb=(float*)malloc((size_t)max_iters*d_model*sizeof(float));
    for(int i=0;i<max_iters*d_model;i++) M->iter_emb[i]=xavier(max_iters,d_model)*0.1f;
    /* LN params */
    M->ln1_g=(float*)malloc(d_model*sizeof(float)); M->ln1_b=(float*)calloc(d_model,sizeof(float));
    M->ln2_g=(float*)malloc(d_model*sizeof(float)); M->ln2_b=(float*)calloc(d_model,sizeof(float));
    for(int i=0;i<d_model;i++){ M->ln1_g[i]=1; M->ln2_g[i]=1; }
    /* Attention */
    M->Wq=(float*)malloc((size_t)d_model*d_model*sizeof(float));
    M->Wk=(float*)malloc((size_t)d_model*d_model*sizeof(float));
    M->Wv=(float*)malloc((size_t)d_model*d_model*sizeof(float));
    M->Wo=(float*)malloc((size_t)d_model*d_model*sizeof(float));
    for(int i=0;i<d_model*d_model;i++){
        M->Wq[i]=xavier(d_model,d_model); M->Wk[i]=xavier(d_model,d_model);
        M->Wv[i]=xavier(d_model,d_model); M->Wo[i]=xavier(d_model,d_model);
    }
    /* FFN */
    M->W1=(float*)malloc((size_t)d_model*ffn_dim*sizeof(float));
    M->b1=(float*)calloc(ffn_dim,sizeof(float));
    M->W2=(float*)malloc((size_t)ffn_dim*d_model*sizeof(float));
    M->b2=(float*)calloc(d_model,sizeof(float));
    for(int i=0;i<d_model*ffn_dim;i++) M->W1[i]=xavier(d_model,ffn_dim);
    for(int i=0;i<ffn_dim*d_model;i++) M->W2[i]=xavier(ffn_dim,d_model);
    M->alive=1;
    return (int64_t)slot;
}

void aria_looping_destroy(int64_t id){
    if(id<0||id>=MAX_MODELS||!MODELS[id].alive) return;
    LoopModel *M=&MODELS[id];
    free(M->tok_emb); free(M->pos_emb); free(M->iter_emb);
    free(M->ln1_g); free(M->ln1_b); free(M->ln2_g); free(M->ln2_b);
    free(M->Wq); free(M->Wk); free(M->Wv); free(M->Wo);
    free(M->W1); free(M->b1); free(M->W2); free(M->b2);
    M->alive=0;
}

/* Metadata */
int32_t aria_looping_d_model(int64_t id){ return MODELS[id].d_model; }
int32_t aria_looping_n_heads(int64_t id){ return MODELS[id].n_heads; }
int32_t aria_looping_ffn_dim(int64_t id){ return MODELS[id].ffn_dim; }
int32_t aria_looping_max_iters(int64_t id){ return MODELS[id].max_iters; }
int32_t aria_looping_vocab_size(int64_t id){ return MODELS[id].vocab_size; }
int32_t aria_looping_last_iters_used(int64_t id){ return MODELS[id].last_iters_used; }

void aria_looping_set_threshold(int64_t id, double thresh){
    MODELS[id].conv_thresh=(float)thresh;
}

int64_t aria_looping_param_count(int64_t id){
    LoopModel *M=&MODELS[id];
    int dm=M->d_model, ffn=M->ffn_dim;
    int64_t p=0;
    p+=(int64_t)M->vocab_size*dm+(int64_t)M->max_seq*dm; /* tok+pos emb */
    p+=(int64_t)M->max_iters*dm;  /* iter emb */
    p+=4*(int64_t)dm*dm;           /* Wq,Wk,Wv,Wo */
    p+=4*dm;                       /* 2 LN */
    p+=(int64_t)dm*ffn+ffn+(int64_t)ffn*dm+dm; /* FFN */
    return p;
}

/* Scratch ops */
int64_t aria_looping_scratch_create(int32_t r,int32_t c){ return sc_alloc(r,c); }
void    aria_looping_scratch_destroy(int64_t id){ if(id>=0&&id<MAX_SCRATCH&&SP[id].alive){ free(SP[id].d); SP[id].alive=0; } }
void    aria_looping_scratch_set(int64_t id,int32_t i,double v){ SP[id].d[i]=(float)v; }
double  aria_looping_scratch_get(int64_t id,int32_t i){ return (double)SP[id].d[i]; }
int32_t aria_looping_scratch_rows(int64_t id){ return SP[id].rows; }
int32_t aria_looping_scratch_cols(int64_t id){ return SP[id].cols; }

/* Forward with specific iteration count */
int64_t aria_looping_forward_n(int64_t mid, int64_t inp_id, int32_t n_iters){
    LoopModel *M=&MODELS[mid];
    Scratch *inp=&SP[inp_id];
    int seq=inp->rows, dm=M->d_model;
    /* Embed */
    float *act=(float*)calloc((size_t)seq*dm,sizeof(float));
    for(int t=0;t<seq;t++){
        int tok=(int)inp->d[t]; if(tok<0) tok=0; if(tok>=M->vocab_size) tok=M->vocab_size-1;
        for(int d=0;d<dm;d++) act[t*dm+d]=M->tok_emb[tok*dm+d]+M->pos_emb[t*dm+d];
    }
    float *prev=NULL;
    if(M->conv_thresh>0) prev=(float*)malloc((size_t)seq*dm*sizeof(float));
    int iters=n_iters>0?n_iters:M->max_iters;
    int used=0;
    for(int it=0;it<iters;it++){
        if(prev) memcpy(prev,act,(size_t)seq*dm*sizeof(float));
        refine_iter(M, act, seq, it);
        used=it+1;
        /* convergence check */
        if(M->conv_thresh>0 && prev){
            float l2=0;
            for(int i=0;i<seq*dm;i++){ float d=act[i]-prev[i]; l2+=d*d; }
            l2=sqrtf(l2/(seq*dm));
            if(l2<M->conv_thresh) break;
        }
    }
    M->last_iters_used=used;
    if(prev) free(prev);
    /* Unembed (weight-tied) */
    int64_t out=sc_alloc(seq,M->vocab_size);
    if(out<0){ free(act); return -1; }
    for(int t=0;t<seq;t++) for(int v=0;v<M->vocab_size;v++){
        float dot=0; for(int d=0;d<dm;d++) dot+=act[t*dm+d]*M->tok_emb[v*dm+d];
        SP[out].d[t*M->vocab_size+v]=dot;
    }
    free(act);
    return out;
}

/* Forward using max_iters (default) */
int64_t aria_looping_forward(int64_t mid, int64_t inp_id){
    return aria_looping_forward_n(mid, inp_id, 0);
}

int32_t aria_looping_argmax(int64_t sid, int32_t row){
    Scratch *s=&SP[sid]; int cols=s->cols, best=0;
    float bv=s->d[row*cols];
    for(int i=1;i<cols;i++) if(s->d[row*cols+i]>bv){ bv=s->d[row*cols+i]; best=i; }
    return best;
}

void aria_looping_softmax_row(int64_t sid, int32_t row){
    softmax_vec(SP[sid].d+row*SP[sid].cols, SP[sid].cols);
}
