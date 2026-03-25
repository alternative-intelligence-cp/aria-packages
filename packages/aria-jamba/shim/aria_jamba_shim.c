/*  aria_jamba_shim.c  –  Jamba hybrid model (Transformer attn + Mamba SSM + MoE)
 *
 *  Architecture per layer (decided by attn_every parameter):
 *    Attention layer: LayerNorm → Multi-Head Attention → residual → LayerNorm → MoE-FFN → residual
 *    Mamba layer:     LayerNorm → Mamba SSM block → residual → LayerNorm → MoE-FFN → residual
 *
 *  MoE: top-k gating over N experts (each expert is a 2-layer FFN with SiLU).
 *
 *  Compile:  gcc -shared -fPIC -O2 -o libaria_jamba_shim.so aria_jamba_shim.c -lm
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── helpers ───────────────────────────────────────────────────────────── */
typedef struct { char *data; int64_t length; } AriaString;
static char errbuf[256] = "ok";
static uint64_t rng = 0xDEADBEEFCAFEULL;
static float xrand(void){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return (float)(rng&0x7FFFFFFF)/2147483647.0f; }
static float xavier(int fan_in, int fan_out){ float s=sqrtf(6.0f/(fan_in+fan_out)); return (xrand()*2-1)*s; }

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

/* ── Jamba layer structures ────────────────────────────────────────────── */
typedef struct {  /* Attention layer weights */
    float *ln1_g, *ln1_b;          /* pre-attn layernorm */
    float *Wq, *Wk, *Wv, *Wo;     /* attention projections */
    float *ln2_g, *ln2_b;          /* pre-FFN layernorm */
    /* MoE FFN  */
    float *gate_w;                 /* [d_model × n_experts] */
    float *exp_w1;                 /* n_experts × [d_model × ffn_dim] */
    float *exp_w2;                 /* n_experts × [ffn_dim × d_model] */
} AttnLayer;

typedef struct {  /* Mamba layer weights */
    float *ln1_g, *ln1_b;          /* pre-SSM layernorm */
    float *in_proj;                /* [d_model × 2*d_inner] */
    float *conv_w, *conv_b;       /* depthwise conv  */
    float *x_proj;                /* [d_inner × (dt_rank+2*d_state)] */
    float *dt_proj;               /* [dt_rank × d_inner] */
    float *dt_bias;
    float *A_log;                 /* [d_inner × d_state] */
    float *D;                     /* [d_inner] */
    float *out_proj;              /* [d_inner × d_model] */
    float *ln2_g, *ln2_b;          /* pre-FFN layernorm */
    float *gate_w;                 /* MoE gating */
    float *exp_w1, *exp_w2;
} MambaLayer;

typedef struct {
    int d_model, n_heads, d_inner, d_state, dt_rank, conv_k;
    int n_layers, vocab_size, max_seq;
    int ffn_dim, n_experts, top_k, attn_every;
    float *tok_emb, *pos_emb;      /* embeddings */
    int *layer_is_attn;             /* 1=attention, 0=mamba */
    AttnLayer  *attn_layers;
    MambaLayer *mamba_layers;
    int n_attn, n_mamba;
    int alive;
} JambaModel;

#define MAX_MODELS 32
static JambaModel MODELS[MAX_MODELS];

/* ── init helpers ──────────────────────────────────────────────────────── */
static void init_attn(AttnLayer *a, int dm, int ffn, int ne){
    a->ln1_g=(float*)malloc(dm*sizeof(float)); a->ln1_b=(float*)calloc(dm,sizeof(float));
    a->ln2_g=(float*)malloc(dm*sizeof(float)); a->ln2_b=(float*)calloc(dm,sizeof(float));
    for(int i=0;i<dm;i++){ a->ln1_g[i]=1; a->ln2_g[i]=1; }
    a->Wq=(float*)malloc(dm*dm*sizeof(float));
    a->Wk=(float*)malloc(dm*dm*sizeof(float));
    a->Wv=(float*)malloc(dm*dm*sizeof(float));
    a->Wo=(float*)malloc(dm*dm*sizeof(float));
    for(int i=0;i<dm*dm;i++){ a->Wq[i]=xavier(dm,dm); a->Wk[i]=xavier(dm,dm);
        a->Wv[i]=xavier(dm,dm); a->Wo[i]=xavier(dm,dm); }
    a->gate_w=(float*)malloc(dm*ne*sizeof(float));
    for(int i=0;i<dm*ne;i++) a->gate_w[i]=xavier(dm,ne);
    a->exp_w1=(float*)malloc((size_t)ne*dm*ffn*sizeof(float));
    a->exp_w2=(float*)malloc((size_t)ne*ffn*dm*sizeof(float));
    for(int i=0;i<ne*dm*ffn;i++) a->exp_w1[i]=xavier(dm,ffn);
    for(int i=0;i<ne*ffn*dm;i++) a->exp_w2[i]=xavier(ffn,dm);
}

static void init_mamba(MambaLayer *m, int dm, int di, int ds, int dtr, int ck, int ffn, int ne){
    m->ln1_g=(float*)malloc(dm*sizeof(float)); m->ln1_b=(float*)calloc(dm,sizeof(float));
    m->ln2_g=(float*)malloc(dm*sizeof(float)); m->ln2_b=(float*)calloc(dm,sizeof(float));
    for(int i=0;i<dm;i++){ m->ln1_g[i]=1; m->ln2_g[i]=1; }
    m->in_proj=(float*)malloc(dm*2*di*sizeof(float));
    for(int i=0;i<dm*2*di;i++) m->in_proj[i]=xavier(dm,2*di);
    m->conv_w=(float*)malloc(di*ck*sizeof(float));
    m->conv_b=(float*)calloc(di,sizeof(float));
    for(int i=0;i<di*ck;i++) m->conv_w[i]=xavier(ck,di);
    int xp=dtr+2*ds;
    m->x_proj=(float*)malloc(di*xp*sizeof(float));
    for(int i=0;i<di*xp;i++) m->x_proj[i]=xavier(di,xp);
    m->dt_proj=(float*)malloc(dtr*di*sizeof(float));
    for(int i=0;i<dtr*di;i++) m->dt_proj[i]=xavier(dtr,di);
    m->dt_bias=(float*)calloc(di,sizeof(float));
    m->A_log=(float*)malloc(di*ds*sizeof(float));
    for(int d=0;d<di;d++) for(int s=0;s<ds;s++) m->A_log[d*ds+s]=logf((float)(s+1));
    m->D=(float*)malloc(di*sizeof(float));
    for(int i=0;i<di;i++) m->D[i]=1.0f;
    m->out_proj=(float*)malloc(di*dm*sizeof(float));
    for(int i=0;i<di*dm;i++) m->out_proj[i]=xavier(di,dm);
    m->gate_w=(float*)malloc(dm*ne*sizeof(float));
    for(int i=0;i<dm*ne;i++) m->gate_w[i]=xavier(dm,ne);
    m->exp_w1=(float*)malloc((size_t)ne*dm*ffn*sizeof(float));
    m->exp_w2=(float*)malloc((size_t)ne*ffn*dm*sizeof(float));
    for(int i=0;i<ne*dm*ffn;i++) m->exp_w1[i]=xavier(dm,ffn);
    for(int i=0;i<ne*ffn*dm;i++) m->exp_w2[i]=xavier(ffn,dm);
}

static void free_attn(AttnLayer *a){
    free(a->ln1_g); free(a->ln1_b); free(a->ln2_g); free(a->ln2_b);
    free(a->Wq); free(a->Wk); free(a->Wv); free(a->Wo);
    free(a->gate_w); free(a->exp_w1); free(a->exp_w2);
}
static void free_mamba(MambaLayer *m){
    free(m->ln1_g); free(m->ln1_b); free(m->ln2_g); free(m->ln2_b);
    free(m->in_proj); free(m->conv_w); free(m->conv_b);
    free(m->x_proj); free(m->dt_proj); free(m->dt_bias);
    free(m->A_log); free(m->D); free(m->out_proj);
    free(m->gate_w); free(m->exp_w1); free(m->exp_w2);
}

/* ── math helpers ──────────────────────────────────────────────────────── */
static void layer_norm(float *out, const float *x, const float *g, const float *b, int n){
    float mu=0; for(int i=0;i<n;i++) mu+=x[i]; mu/=n;
    float var=0; for(int i=0;i<n;i++){ float d=x[i]-mu; var+=d*d; } var/=n;
    float inv=1.0f/sqrtf(var+1e-5f);
    for(int i=0;i<n;i++) out[i]=g[i]*(x[i]-mu)*inv+b[i];
}
static void matmul(float *C, const float *A, const float *B, int M, int K, int N){
    memset(C,0,(size_t)M*N*sizeof(float));
    for(int i=0;i<M;i++) for(int k=0;k<K;k++){
        float a=A[i*K+k]; for(int j=0;j<N;j++) C[i*N+j]+=a*B[k*N+j]; }
}
static float silu(float x){ return x/(1.0f+expf(-x)); }
static float gelu(float x){ return 0.5f*x*(1.0f+tanhf(0.7978845608f*(x+0.044715f*x*x*x))); }
static void softmax_vec(float *v, int n){
    float mx=-1e30f; for(int i=0;i<n;i++) if(v[i]>mx) mx=v[i];
    float s=0; for(int i=0;i<n;i++){ v[i]=expf(v[i]-mx); s+=v[i]; }
    for(int i=0;i<n;i++) v[i]/=s;
}

/* ── forward: attention layer ──────────────────────────────────────────── */
static void fwd_attn(AttnLayer *a, float *act, int seq, int dm, int nh){
    int dk=dm/nh;
    float *normed=(float*)malloc((size_t)seq*dm*sizeof(float));
    for(int t=0;t<seq;t++) layer_norm(normed+t*dm, act+t*dm, a->ln1_g, a->ln1_b, dm);
    float *Q=(float*)malloc((size_t)seq*dm*sizeof(float));
    float *K=(float*)malloc((size_t)seq*dm*sizeof(float));
    float *V=(float*)malloc((size_t)seq*dm*sizeof(float));
    matmul(Q,normed,a->Wq,seq,dm,dm);
    matmul(K,normed,a->Wk,seq,dm,dm);
    matmul(V,normed,a->Wv,seq,dm,dm);
    float *attn_out=(float*)calloc((size_t)seq*dm,sizeof(float));
    float scale=1.0f/sqrtf((float)dk);
    float *scores=(float*)malloc((size_t)seq*seq*sizeof(float));
    for(int h=0;h<nh;h++){
        for(int i=0;i<seq;i++) for(int j=0;j<seq;j++){
            float dot=0;
            for(int d=0;d<dk;d++) dot+=Q[i*dm+h*dk+d]*K[j*dm+h*dk+d];
            scores[i*seq+j]=dot*scale;
            if(j>i) scores[i*seq+j]=-1e9f; /* causal mask */
        }
        for(int i=0;i<seq;i++) softmax_vec(scores+i*seq,seq);
        for(int i=0;i<seq;i++) for(int d=0;d<dk;d++){
            float v=0; for(int j=0;j<seq;j++) v+=scores[i*seq+j]*V[j*dm+h*dk+d];
            attn_out[i*dm+h*dk+d]=v;
        }
    }
    float *proj=(float*)malloc((size_t)seq*dm*sizeof(float));
    matmul(proj,attn_out,a->Wo,seq,dm,dm);
    for(int i=0;i<seq*dm;i++) act[i]+=proj[i]; /* residual */
    free(normed); free(Q); free(K); free(V); free(attn_out); free(scores); free(proj);
}

/* ── forward: mamba layer ──────────────────────────────────────────────── */
static void fwd_mamba(MambaLayer *m, float *act, int seq, int dm, int di, int ds, int dtr, int ck){
    float *normed=(float*)malloc((size_t)seq*dm*sizeof(float));
    for(int t=0;t<seq;t++) layer_norm(normed+t*dm, act+t*dm, m->ln1_g, m->ln1_b, dm);
    /* in_proj: [seq×dm] × [dm×2di] → [seq×2di] */
    float *xz=(float*)malloc((size_t)seq*2*di*sizeof(float));
    matmul(xz,normed,m->in_proj,seq,dm,2*di);
    float *x_br=xz, *z_br=xz+0; /* x = first di cols, z = last di cols (in-place) */
    float *xb=(float*)malloc((size_t)seq*di*sizeof(float));
    float *zb=(float*)malloc((size_t)seq*di*sizeof(float));
    for(int t=0;t<seq;t++) for(int d=0;d<di;d++){
        xb[t*di+d]=xz[t*2*di+d];
        zb[t*di+d]=xz[t*2*di+di+d];
    }
    /* 1D causal depthwise conv + SiLU */
    float *conv_out=(float*)calloc((size_t)seq*di,sizeof(float));
    for(int t=0;t<seq;t++) for(int d=0;d<di;d++){
        float v=m->conv_b[d];
        for(int k=0;k<ck;k++){ int tt=t-k; if(tt>=0) v+=xb[tt*di+d]*m->conv_w[d*ck+k]; }
        conv_out[t*di+d]=silu(v);
    }
    /* x_proj: [seq×di] × [di×(dtr+2ds)] → dt_raw, B, C */
    int xp_cols=dtr+2*ds;
    float *xp_out=(float*)malloc((size_t)seq*xp_cols*sizeof(float));
    matmul(xp_out,conv_out,m->x_proj,seq,di,xp_cols);
    /* dt_proj + softplus */
    float *dt=(float*)malloc((size_t)seq*di*sizeof(float));
    for(int t=0;t<seq;t++){
        float *dtr_raw=xp_out+t*xp_cols;
        for(int d=0;d<di;d++){
            float v=m->dt_bias[d];
            for(int r=0;r<dtr;r++) v+=dtr_raw[r]*m->dt_proj[r*di+d];
            dt[t*di+d]=logf(1.0f+expf(v)); /* softplus */
        }
    }
    float *B_mat=xp_out; /* reuse pointer offset below */
    float *C_mat=xp_out;
    /* Selective scan */
    float *state=(float*)calloc((size_t)di*ds,sizeof(float));
    float *ssm_out=(float*)malloc((size_t)seq*di*sizeof(float));
    for(int t=0;t<seq;t++){
        float *Bt=xp_out+t*xp_cols+dtr;
        float *Ct=xp_out+t*xp_cols+dtr+ds;
        for(int d=0;d<di;d++){
            float dt_val=dt[t*di+d], x_val=conv_out[t*di+d], y=0;
            for(int s=0;s<ds;s++){
                float A=-expf(m->A_log[d*ds+s]);
                float dA=expf(dt_val*A);
                float dB=dt_val*Bt[s];
                state[d*ds+s]=dA*state[d*ds+s]+dB*x_val;
                y+=state[d*ds+s]*Ct[s];
            }
            ssm_out[t*di+d]=y+m->D[d]*x_val;
        }
    }
    /* SiLU gate */
    for(int i=0;i<seq*di;i++) ssm_out[i]*=silu(zb[i]);
    /* out_proj: [seq×di] × [di×dm] */
    float *out=(float*)malloc((size_t)seq*dm*sizeof(float));
    matmul(out,ssm_out,m->out_proj,seq,di,dm);
    for(int i=0;i<seq*dm;i++) act[i]+=out[i]; /* residual */
    free(normed); free(xz); free(xb); free(zb); free(conv_out);
    free(xp_out); free(dt); free(state); free(ssm_out); free(out);
}

/* ── forward: MoE FFN ──────────────────────────────────────────────────── */
static void fwd_moe_ffn(float *act, const float *ln_g, const float *ln_b,
                         const float *gate_w, const float *ew1, const float *ew2,
                         int seq, int dm, int ffn, int ne, int topk){
    float *normed=(float*)malloc((size_t)seq*dm*sizeof(float));
    for(int t=0;t<seq;t++) layer_norm(normed+t*dm, act+t*dm, ln_g, ln_b, dm);
    float *gscores=(float*)malloc(ne*sizeof(float));
    float *expert_out=(float*)malloc(dm*sizeof(float));
    float *hidden=(float*)malloc(ffn*sizeof(float));
    for(int t=0;t<seq;t++){
        /* gate: [dm] × [dm×ne] → [ne] then softmax */
        for(int e=0;e<ne;e++){
            float v=0; for(int d=0;d<dm;d++) v+=normed[t*dm+d]*gate_w[d*ne+e];
            gscores[e]=v;
        }
        softmax_vec(gscores,ne);
        /* find top-k experts */
        float accum[1024]; memset(accum,0,dm*sizeof(float)); /* dm <= 1024 assumed */
        for(int ki=0;ki<topk;ki++){
            int best=-1; float bv=-1e30f;
            for(int e=0;e<ne;e++) if(gscores[e]>bv){ bv=gscores[e]; best=e; }
            if(best<0) break;
            float w=gscores[best]; gscores[best]=-1e30f;
            /* expert FFN: x → W1 → SiLU → W2 */
            const float *W1=ew1+(size_t)best*dm*ffn;
            const float *W2=ew2+(size_t)best*ffn*dm;
            for(int f=0;f<ffn;f++){
                float v=0; for(int d=0;d<dm;d++) v+=normed[t*dm+d]*W1[d*ffn+f];
                hidden[f]=silu(v);
            }
            for(int d=0;d<dm;d++){
                float v=0; for(int f=0;f<ffn;f++) v+=hidden[f]*W2[f*dm+d];
                accum[d]+=w*v;
            }
        }
        for(int d=0;d<dm;d++) act[t*dm+d]+=accum[d]; /* residual */
    }
    free(normed); free(gscores); free(expert_out); free(hidden);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ══════════════════════════════════════════════════════════════════════════ */
AriaString aria_jamba_last_error(void){
    return (AriaString){errbuf,(int64_t)strlen(errbuf)};
}
void aria_jamba_set_seed(int32_t s){ rng=(uint64_t)s*6364136223846793005ULL+1; }

/* create(d_model, n_heads, d_inner, d_state, dt_rank, conv_k,
          n_layers, attn_every, n_experts, top_k, vocab_size, max_seq)
   attn_every: every Nth layer is attention (rest are Mamba).  e.g. 2 = alternating */
int64_t aria_jamba_create(int32_t d_model, int32_t n_heads,
        int32_t d_inner, int32_t d_state, int32_t dt_rank, int32_t conv_k,
        int32_t n_layers, int32_t attn_every, int32_t n_experts, int32_t top_k,
        int32_t vocab_size, int32_t max_seq){
    int slot=-1;
    for(int i=0;i<MAX_MODELS;i++) if(!MODELS[i].alive){ slot=i; break; }
    if(slot<0){ snprintf(errbuf,sizeof(errbuf),"no model slots"); return -1; }
    JambaModel *M=&MODELS[slot];
    memset(M,0,sizeof(JambaModel));
    M->d_model=d_model; M->n_heads=n_heads; M->d_inner=d_inner;
    M->d_state=d_state; M->dt_rank=dt_rank; M->conv_k=conv_k;
    M->n_layers=n_layers; M->vocab_size=vocab_size; M->max_seq=max_seq;
    M->ffn_dim=d_model*4; M->n_experts=n_experts; M->top_k=top_k;
    M->attn_every=attn_every>0?attn_every:2;
    /* embeddings */
    M->tok_emb=(float*)malloc((size_t)vocab_size*d_model*sizeof(float));
    M->pos_emb=(float*)malloc((size_t)max_seq*d_model*sizeof(float));
    for(int i=0;i<vocab_size*d_model;i++) M->tok_emb[i]=xavier(vocab_size,d_model);
    for(int i=0;i<max_seq*d_model;i++) M->pos_emb[i]=xavier(max_seq,d_model);
    /* decide layer types */
    M->layer_is_attn=(int*)malloc(n_layers*sizeof(int));
    M->n_attn=0; M->n_mamba=0;
    for(int i=0;i<n_layers;i++){
        M->layer_is_attn[i]=(i%M->attn_every==0)?1:0;
        if(M->layer_is_attn[i]) M->n_attn++; else M->n_mamba++;
    }
    /* allocate layers */
    M->attn_layers=(AttnLayer*)calloc(M->n_attn,sizeof(AttnLayer));
    M->mamba_layers=(MambaLayer*)calloc(M->n_mamba,sizeof(MambaLayer));
    int ai=0, mi=0;
    for(int i=0;i<n_layers;i++){
        if(M->layer_is_attn[i])
            init_attn(&M->attn_layers[ai++],d_model,M->ffn_dim,n_experts);
        else
            init_mamba(&M->mamba_layers[mi++],d_model,d_inner,d_state,dt_rank,conv_k,M->ffn_dim,n_experts);
    }
    M->alive=1;
    return (int64_t)slot;
}

void aria_jamba_destroy(int64_t id){
    if(id<0||id>=MAX_MODELS||!MODELS[id].alive) return;
    JambaModel *M=&MODELS[id];
    free(M->tok_emb); free(M->pos_emb);
    for(int i=0;i<M->n_attn;i++) free_attn(&M->attn_layers[i]);
    for(int i=0;i<M->n_mamba;i++) free_mamba(&M->mamba_layers[i]);
    free(M->attn_layers); free(M->mamba_layers); free(M->layer_is_attn);
    M->alive=0;
}

/* ── Metadata ──────────────────────────────────────────────────────────── */
int32_t aria_jamba_d_model(int64_t id){ return MODELS[id].d_model; }
int32_t aria_jamba_n_heads(int64_t id){ return MODELS[id].n_heads; }
int32_t aria_jamba_d_inner(int64_t id){ return MODELS[id].d_inner; }
int32_t aria_jamba_n_layers(int64_t id){ return MODELS[id].n_layers; }
int32_t aria_jamba_vocab_size(int64_t id){ return MODELS[id].vocab_size; }
int32_t aria_jamba_n_experts(int64_t id){ return MODELS[id].n_experts; }
int32_t aria_jamba_top_k(int64_t id){ return MODELS[id].top_k; }
int32_t aria_jamba_attn_every(int64_t id){ return MODELS[id].attn_every; }
int32_t aria_jamba_layer_is_attn(int64_t id, int32_t layer){
    if(layer<0||layer>=MODELS[id].n_layers) return -1;
    return MODELS[id].layer_is_attn[layer];
}

int64_t aria_jamba_param_count(int64_t id){
    JambaModel *M=&MODELS[id];
    int64_t p=0;
    int dm=M->d_model, ffn=M->ffn_dim, ne=M->n_experts;
    int di=M->d_inner, ds=M->d_state, dtr=M->dt_rank, ck=M->conv_k;
    p+=(int64_t)M->vocab_size*dm + (int64_t)M->max_seq*dm; /* embeddings */
    for(int i=0;i<M->n_attn;i++){
        p+=4*(int64_t)dm*dm; /* Wq,Wk,Wv,Wo */
        p+=4*dm;              /* 2 layernorms */
        p+=(int64_t)dm*ne;   /* gate */
        p+=(int64_t)ne*dm*ffn+(int64_t)ne*ffn*dm; /* experts */
    }
    for(int i=0;i<M->n_mamba;i++){
        p+=(int64_t)dm*2*di;  /* in_proj */
        p+=(int64_t)di*ck+di; /* conv */
        int xp=dtr+2*ds;
        p+=(int64_t)di*xp;    /* x_proj */
        p+=(int64_t)dtr*di+di;/* dt_proj+bias */
        p+=(int64_t)di*ds+di; /* A_log+D */
        p+=(int64_t)di*dm;    /* out_proj */
        p+=4*dm;               /* 2 layernorms */
        p+=(int64_t)dm*ne;    /* gate */
        p+=(int64_t)ne*dm*ffn+(int64_t)ne*ffn*dm;
    }
    return p;
}

/* ── Scratch ops (same as others) ──────────────────────────────────────── */
int64_t aria_jamba_scratch_create(int32_t r,int32_t c){ return sc_alloc(r,c); }
void    aria_jamba_scratch_destroy(int64_t id){ if(id>=0&&id<MAX_SCRATCH&&SP[id].alive){ free(SP[id].d); SP[id].alive=0; } }
void    aria_jamba_scratch_set(int64_t id,int32_t i,double v){ SP[id].d[i]=(float)v; }
double  aria_jamba_scratch_get(int64_t id,int32_t i){ return (double)SP[id].d[i]; }
int32_t aria_jamba_scratch_rows(int64_t id){ return SP[id].rows; }
int32_t aria_jamba_scratch_cols(int64_t id){ return SP[id].cols; }

/* ── Forward ───────────────────────────────────────────────────────────── */
int64_t aria_jamba_forward(int64_t mid, int64_t inp_id){
    JambaModel *M=&MODELS[mid];
    Scratch *inp=&SP[inp_id];
    int seq=inp->rows, dm=M->d_model;
    /* embed */
    float *act=(float*)calloc((size_t)seq*dm,sizeof(float));
    for(int t=0;t<seq;t++){
        int tok=(int)inp->d[t]; if(tok<0) tok=0; if(tok>=M->vocab_size) tok=M->vocab_size-1;
        for(int d=0;d<dm;d++) act[t*dm+d]=M->tok_emb[tok*dm+d]+M->pos_emb[t*dm+d];
    }
    /* layer stack */
    int ai=0, mi=0;
    for(int l=0;l<M->n_layers;l++){
        if(M->layer_is_attn[l]){
            AttnLayer *a=&M->attn_layers[ai++];
            fwd_attn(a, act, seq, dm, M->n_heads);
            fwd_moe_ffn(act, a->ln2_g, a->ln2_b, a->gate_w, a->exp_w1, a->exp_w2,
                        seq, dm, M->ffn_dim, M->n_experts, M->top_k);
        } else {
            MambaLayer *mb=&M->mamba_layers[mi++];
            fwd_mamba(mb, act, seq, dm, M->d_inner, M->d_state, M->dt_rank, M->conv_k);
            fwd_moe_ffn(act, mb->ln2_g, mb->ln2_b, mb->gate_w, mb->exp_w1, mb->exp_w2,
                        seq, dm, M->ffn_dim, M->n_experts, M->top_k);
        }
    }
    /* unembed: [seq×dm] × [dm×vocab] (weight-tied) → logits */
    int64_t out=sc_alloc(seq,M->vocab_size);
    if(out<0){ free(act); return -1; }
    /* tok_emb is [vocab×dm], we want act × tok_emb^T → [seq×vocab] */
    for(int t=0;t<seq;t++) for(int v=0;v<M->vocab_size;v++){
        float dot=0; for(int d=0;d<dm;d++) dot+=act[t*dm+d]*M->tok_emb[v*dm+d];
        SP[out].d[t*M->vocab_size+v]=dot;
    }
    free(act);
    return out;
}

int32_t aria_jamba_argmax(int64_t sid, int32_t row){
    Scratch *s=&SP[sid]; int cols=s->cols, best=0;
    float bv=s->d[row*cols];
    for(int i=1;i<cols;i++) if(s->d[row*cols+i]>bv){ bv=s->d[row*cols+i]; best=i; }
    return best;
}

void aria_jamba_softmax_row(int64_t sid, int32_t row){
    Scratch *s=&SP[sid];
    softmax_vec(s->d+row*s->cols, s->cols);
}
