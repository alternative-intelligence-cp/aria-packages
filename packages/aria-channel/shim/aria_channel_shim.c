/* aria_channel_shim.c — High-level channel patterns for Aria */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

/* Forward declarations for channel shim functions (from runtime) */
extern int64_t aria_shim_channel_create(int32_t capacity);
extern int32_t aria_shim_channel_send(int64_t handle, int64_t value);
extern int64_t aria_shim_channel_recv(int64_t handle);
extern int32_t aria_shim_channel_close(int64_t handle);
extern int32_t aria_shim_channel_destroy(int64_t handle);
extern bool    aria_shim_channel_is_closed(int64_t handle);
extern int32_t aria_shim_channel_count(int64_t handle);
extern int64_t aria_shim_thread_spawn(void* func, int64_t arg);
extern int32_t aria_shim_thread_join(int64_t handle);

/* ======================================================================
 * Fan-out: one input channel distributes to N output channels (round-robin)
 * ====================================================================== */

#define MAX_FANOUT 32

typedef struct {
    int64_t in_ch;
    int64_t out_chs[MAX_FANOUT];
    int32_t n_out;
    int64_t thread_handle;
    volatile bool running;
} FanOut;

static FanOut* g_fanouts[64];
static int g_fanout_count = 0;

static void fanout_worker(void* env, int64_t slot) {
    (void)env;
    if (slot < 0 || slot >= 64 || !g_fanouts[slot]) return;
    FanOut* fo = g_fanouts[slot];
    int32_t rr = 0;
    while (fo->running) {
        if (aria_shim_channel_is_closed(fo->in_ch) &&
            aria_shim_channel_count(fo->in_ch) == 0) break;
        int64_t val = aria_shim_channel_recv(fo->in_ch);
        if (aria_shim_channel_is_closed(fo->in_ch) &&
            aria_shim_channel_count(fo->in_ch) == 0 && val == 0) break;
        aria_shim_channel_send(fo->out_chs[rr], val);
        rr = (rr + 1) % fo->n_out;
    }
    /* Close all output channels */
    for (int32_t i = 0; i < fo->n_out; i++) {
        aria_shim_channel_close(fo->out_chs[i]);
    }
    fo->running = false;
}

/**
 * Create a fan-out: reads from in_ch, distributes round-robin to n_out new channels.
 * Returns slot handle (>= 0) on success. The output channels can be retrieved with
 * aria_chanpat_fanout_get_output(slot, index).
 */
int64_t aria_chanpat_fanout_create(int64_t in_ch, int32_t n_out, int32_t out_capacity) {
    if (n_out <= 0 || n_out > MAX_FANOUT || g_fanout_count >= 64) return -1;
    FanOut* fo = (FanOut*)calloc(1, sizeof(FanOut));
    if (!fo) return -1;
    fo->in_ch = in_ch;
    fo->n_out = n_out;
    for (int32_t i = 0; i < n_out; i++) {
        fo->out_chs[i] = aria_shim_channel_create(out_capacity);
        if (fo->out_chs[i] < 0) {
            for (int32_t j = 0; j < i; j++) aria_shim_channel_destroy(fo->out_chs[j]);
            free(fo);
            return -1;
        }
    }
    fo->running = true;
    int64_t slot = g_fanout_count;
    g_fanouts[slot] = fo;
    g_fanout_count++;
    fo->thread_handle = aria_shim_thread_spawn((void*)fanout_worker, slot);
    return slot;
}

/** Get output channel handle at index for a fan-out. */
int64_t aria_chanpat_fanout_get_output(int64_t slot, int32_t index) {
    if (slot < 0 || slot >= 64 || !g_fanouts[slot]) return -1;
    FanOut* fo = g_fanouts[slot];
    if (index < 0 || index >= fo->n_out) return -1;
    return fo->out_chs[index];
}

/** Stop and destroy fan-out. */
int32_t aria_chanpat_fanout_destroy(int64_t slot) {
    if (slot < 0 || slot >= 64 || !g_fanouts[slot]) return -1;
    FanOut* fo = g_fanouts[slot];
    fo->running = false;
    aria_shim_channel_close(fo->in_ch);
    aria_shim_thread_join(fo->thread_handle);
    for (int32_t i = 0; i < fo->n_out; i++) {
        aria_shim_channel_destroy(fo->out_chs[i]);
    }
    free(fo);
    g_fanouts[slot] = NULL;
    return 0;
}

/* ======================================================================
 * Fan-in: N input channels merged into one output channel
 * ====================================================================== */

typedef struct {
    int64_t in_chs[MAX_FANOUT];
    int32_t n_in;
    int64_t out_ch;
    int64_t thread_handles[MAX_FANOUT];
    volatile int32_t done_count;
    pthread_mutex_t done_mtx;
} FanIn;

static FanIn* g_fanins[64];
static int g_fanin_count = 0;

typedef struct {
    int64_t fanin_slot;
    int32_t input_index;
} FanInWorkerArg;

static FanInWorkerArg g_fanin_args[64 * MAX_FANOUT];

static void fanin_worker(void* env, int64_t arg_idx) {
    (void)env;
    FanInWorkerArg* wa = &g_fanin_args[arg_idx];
    FanIn* fi = g_fanins[wa->fanin_slot];
    if (!fi) return;
    int64_t in_ch = fi->in_chs[wa->input_index];

    while (1) {
        if (aria_shim_channel_is_closed(in_ch) &&
            aria_shim_channel_count(in_ch) == 0) break;
        int64_t val = aria_shim_channel_recv(in_ch);
        if (aria_shim_channel_is_closed(in_ch) &&
            aria_shim_channel_count(in_ch) == 0 && val == 0) break;
        aria_shim_channel_send(fi->out_ch, val);
    }

    /* Track completion */
    pthread_mutex_lock(&fi->done_mtx);
    fi->done_count++;
    if (fi->done_count >= fi->n_in) {
        aria_shim_channel_close(fi->out_ch);
    }
    pthread_mutex_unlock(&fi->done_mtx);
}

/**
 * Create a fan-in: merges n_in input channels into one new output channel.
 * in_handles: array of channel handles stored as sequential int64 via helper calls.
 * Returns slot handle. Output accessed via aria_chanpat_fanin_get_output(slot).
 */
int64_t aria_chanpat_fanin_create2(int64_t ch0, int64_t ch1, int32_t out_capacity) {
    if (g_fanin_count >= 64) return -1;
    FanIn* fi = (FanIn*)calloc(1, sizeof(FanIn));
    if (!fi) return -1;
    fi->in_chs[0] = ch0;
    fi->in_chs[1] = ch1;
    fi->n_in = 2;
    fi->out_ch = aria_shim_channel_create(out_capacity);
    if (fi->out_ch < 0) { free(fi); return -1; }
    fi->done_count = 0;
    pthread_mutex_init(&fi->done_mtx, NULL);

    int64_t slot = g_fanin_count;
    g_fanins[slot] = fi;
    g_fanin_count++;

    for (int32_t i = 0; i < 2; i++) {
        int64_t arg_idx = slot * MAX_FANOUT + i;
        g_fanin_args[arg_idx].fanin_slot = slot;
        g_fanin_args[arg_idx].input_index = i;
        fi->thread_handles[i] = aria_shim_thread_spawn((void*)fanin_worker, arg_idx);
    }
    return slot;
}

int64_t aria_chanpat_fanin_create3(int64_t ch0, int64_t ch1, int64_t ch2, int32_t out_capacity) {
    if (g_fanin_count >= 64) return -1;
    FanIn* fi = (FanIn*)calloc(1, sizeof(FanIn));
    if (!fi) return -1;
    fi->in_chs[0] = ch0;
    fi->in_chs[1] = ch1;
    fi->in_chs[2] = ch2;
    fi->n_in = 3;
    fi->out_ch = aria_shim_channel_create(out_capacity);
    if (fi->out_ch < 0) { free(fi); return -1; }
    fi->done_count = 0;
    pthread_mutex_init(&fi->done_mtx, NULL);

    int64_t slot = g_fanin_count;
    g_fanins[slot] = fi;
    g_fanin_count++;

    for (int32_t i = 0; i < 3; i++) {
        int64_t arg_idx = slot * MAX_FANOUT + i;
        g_fanin_args[arg_idx].fanin_slot = slot;
        g_fanin_args[arg_idx].input_index = i;
        fi->thread_handles[i] = aria_shim_thread_spawn((void*)fanin_worker, arg_idx);
    }
    return slot;
}

/** Get output channel handle for a fan-in. */
int64_t aria_chanpat_fanin_get_output(int64_t slot) {
    if (slot < 0 || slot >= 64 || !g_fanins[slot]) return -1;
    return g_fanins[slot]->out_ch;
}

/** Destroy fan-in (waits for all merger threads). */
int32_t aria_chanpat_fanin_destroy(int64_t slot) {
    if (slot < 0 || slot >= 64 || !g_fanins[slot]) return -1;
    FanIn* fi = g_fanins[slot];
    for (int32_t i = 0; i < fi->n_in; i++) {
        aria_shim_thread_join(fi->thread_handles[i]);
    }
    aria_shim_channel_destroy(fi->out_ch);
    pthread_mutex_destroy(&fi->done_mtx);
    free(fi);
    g_fanins[slot] = NULL;
    return 0;
}

/* ======================================================================
 * Pipeline: chain channels in sequence (A -> B -> C)
 * ====================================================================== */

typedef struct {
    int64_t in_ch;
    int64_t out_ch;
    void*   transform_func;  // void(void* env, int64_t value) — should send result to out_ch
    int64_t thread_handle;
    volatile bool running;
} PipeStage;

#define MAX_PIPE_STAGES 64
static PipeStage* g_pipe_stages[MAX_PIPE_STAGES];
static int g_pipe_stage_count = 0;

typedef void (*AriaLambdaFunc2)(void* env, int64_t arg);

static void pipe_stage_worker(void* env, int64_t slot) {
    (void)env;
    if (slot < 0 || slot >= MAX_PIPE_STAGES || !g_pipe_stages[slot]) return;
    PipeStage* ps = g_pipe_stages[slot];
    AriaLambdaFunc2 transform = (AriaLambdaFunc2)ps->transform_func;

    while (ps->running) {
        if (aria_shim_channel_is_closed(ps->in_ch) &&
            aria_shim_channel_count(ps->in_ch) == 0) break;
        int64_t val = aria_shim_channel_recv(ps->in_ch);
        if (aria_shim_channel_is_closed(ps->in_ch) &&
            aria_shim_channel_count(ps->in_ch) == 0 && val == 0) break;
        /* Transform sends to out_ch itself */
        transform(NULL, val);
    }
    aria_shim_channel_close(ps->out_ch);
    ps->running = false;
}

/**
 * Create a pipeline stage: reads from in_ch, calls transform_func for each value.
 * The transform_func should send results to out_ch (it receives the value as arg).
 * Returns pipe stage slot, or -1 on error.
 */
int64_t aria_chanpat_pipe_stage(int64_t in_ch, int64_t out_ch, void* transform_func) {
    if (!transform_func || g_pipe_stage_count >= MAX_PIPE_STAGES) return -1;
    PipeStage* ps = (PipeStage*)calloc(1, sizeof(PipeStage));
    if (!ps) return -1;
    ps->in_ch = in_ch;
    ps->out_ch = out_ch;
    ps->transform_func = transform_func;
    ps->running = true;

    int64_t slot = g_pipe_stage_count;
    g_pipe_stages[slot] = ps;
    g_pipe_stage_count++;

    ps->thread_handle = aria_shim_thread_spawn((void*)pipe_stage_worker, slot);
    return slot;
}

/** Wait for a pipe stage to finish. */
int32_t aria_chanpat_pipe_stage_join(int64_t slot) {
    if (slot < 0 || slot >= MAX_PIPE_STAGES || !g_pipe_stages[slot]) return -1;
    PipeStage* ps = g_pipe_stages[slot];
    return aria_shim_thread_join(ps->thread_handle);
}

/** Destroy a pipe stage. */
int32_t aria_chanpat_pipe_stage_destroy(int64_t slot) {
    if (slot < 0 || slot >= MAX_PIPE_STAGES || !g_pipe_stages[slot]) return -1;
    PipeStage* ps = g_pipe_stages[slot];
    ps->running = false;
    aria_shim_channel_close(ps->in_ch);
    aria_shim_thread_join(ps->thread_handle);
    free(ps);
    g_pipe_stages[slot] = NULL;
    return 0;
}
