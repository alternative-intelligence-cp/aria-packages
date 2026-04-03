/*
 * aria_libc_thread — Threading, synchronization, channels, actors, thread pools & shared memory for Aria
 * Build: cc -O2 -shared -fPIC -lpthread -o libaria_libc_thread.so aria_libc_thread.c
 *
 * Standalone shim — uses pthreads directly, no dependency on Aria runtime.
 * Handle-based API: all objects stored in static pools, returned as int64 handles.
 *
 * v0.11.0: Created for stdlib concurrency migration from direct extern to aria-libc.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Handle Pool Sizes
 * ════════════════════════════════════════════════════════════════════════════ */

#define MAX_THREADS     256
#define MAX_MUTEXES     256
#define MAX_CONDVARS    128
#define MAX_RWLOCKS     128
#define MAX_CHANNELS     64
#define MAX_ACTORS       64
#define MAX_POOLS        32
#define MAX_SHM          32

/* ════════════════════════════════════════════════════════════════════════════
 * Thread API — aria_libc_thread_*
 * ════════════════════════════════════════════════════════════════════════════ */

/* Aria lambda calling convention: void func(void* env, int64_t arg)
 * For non-closure functions, env is always NULL.
 * The compiler extracts method_ptr from the fat pointer {method_ptr, env_ptr}. */
typedef void (*AriaLambdaFunc)(void* env, int64_t arg);

typedef struct {
    AriaLambdaFunc func;
    int64_t        arg;
} ThreadTrampData;

static pthread_t g_threads[MAX_THREADS];
static bool      g_thread_used[MAX_THREADS];

static int64_t alloc_thread_slot(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (!g_thread_used[i]) { g_thread_used[i] = true; return (int64_t)i; }
    }
    return -1;
}

static void* thread_trampoline(void* raw) {
    ThreadTrampData* data = (ThreadTrampData*)raw;
    AriaLambdaFunc func = data->func;
    int64_t arg = data->arg;
    free(data);
    func(NULL, arg);
    return NULL;
}

int64_t aria_libc_thread_spawn(void* func, int64_t arg) {
    if (!func) return -1;
    int64_t slot = alloc_thread_slot();
    if (slot < 0) return -1;
    ThreadTrampData* data = (ThreadTrampData*)malloc(sizeof(ThreadTrampData));
    if (!data) { g_thread_used[slot] = false; return -1; }
    data->func = (AriaLambdaFunc)func;
    data->arg = arg;
    if (pthread_create(&g_threads[slot], NULL, thread_trampoline, data) != 0) {
        free(data);
        g_thread_used[slot] = false;
        return -1;
    }
    return slot;
}

int32_t aria_libc_thread_join(int64_t handle) {
    if (handle < 0 || handle >= MAX_THREADS || !g_thread_used[handle]) return -1;
    int r = pthread_join(g_threads[handle], NULL);
    g_thread_used[handle] = false;
    return (r == 0) ? 0 : -1;
}

int32_t aria_libc_thread_detach(int64_t handle) {
    if (handle < 0 || handle >= MAX_THREADS || !g_thread_used[handle]) return -1;
    int r = pthread_detach(g_threads[handle]);
    g_thread_used[handle] = false;
    return (r == 0) ? 0 : -1;
}

void aria_libc_thread_yield(void) {
    sched_yield();
}

void aria_libc_thread_sleep_ns(int64_t ns) {
    struct timespec ts;
    ts.tv_sec  = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;
    nanosleep(&ts, NULL);
}

void aria_libc_thread_sleep_ms(int64_t ms) {
    aria_libc_thread_sleep_ns(ms * 1000000LL);
}

void aria_libc_thread_set_name(const char* name) {
    if (!name) return;
    char buf[16];
    strncpy(buf, name, 15);
    buf[15] = '\0';
    pthread_setname_np(pthread_self(), buf);
}

int32_t aria_libc_thread_hardware_concurrency(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int32_t)n : 1;
}

int64_t aria_libc_thread_current_id(void) {
    return (int64_t)pthread_self();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Mutex API — aria_libc_mutex_*
 * ════════════════════════════════════════════════════════════════════════════ */

static pthread_mutex_t* g_mutexes[MAX_MUTEXES];

static int64_t alloc_mutex_slot(pthread_mutex_t* m) {
    for (int i = 0; i < MAX_MUTEXES; i++) {
        if (!g_mutexes[i]) { g_mutexes[i] = m; return (int64_t)i; }
    }
    return -1;
}

int64_t aria_libc_mutex_create(int32_t mutex_type) {
    pthread_mutex_t* m = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t));
    if (!m) return -1;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (mutex_type == 1) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    } else {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }
    if (pthread_mutex_init(m, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(m);
        return -1;
    }
    pthread_mutexattr_destroy(&attr);
    int64_t slot = alloc_mutex_slot(m);
    if (slot < 0) { pthread_mutex_destroy(m); free(m); return -1; }
    return slot;
}

int32_t aria_libc_mutex_lock(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    return (pthread_mutex_lock(g_mutexes[handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_mutex_trylock(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    int r = pthread_mutex_trylock(g_mutexes[handle]);
    if (r == 0) return 1;   /* locked */
    return 0;                /* busy (EBUSY) */
}

int32_t aria_libc_mutex_unlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    return (pthread_mutex_unlock(g_mutexes[handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_mutex_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    pthread_mutex_t* m = g_mutexes[handle];
    pthread_mutex_destroy(m);
    free(m);
    g_mutexes[handle] = NULL;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Condition Variable API — aria_libc_condvar_*
 * ════════════════════════════════════════════════════════════════════════════ */

static pthread_cond_t* g_condvars[MAX_CONDVARS];

static int64_t alloc_condvar_slot(pthread_cond_t* c) {
    for (int i = 0; i < MAX_CONDVARS; i++) {
        if (!g_condvars[i]) { g_condvars[i] = c; return (int64_t)i; }
    }
    return -1;
}

int64_t aria_libc_condvar_create(void) {
    pthread_cond_t* c = (pthread_cond_t*)calloc(1, sizeof(pthread_cond_t));
    if (!c) return -1;
    if (pthread_cond_init(c, NULL) != 0) { free(c); return -1; }
    int64_t slot = alloc_condvar_slot(c);
    if (slot < 0) { pthread_cond_destroy(c); free(c); return -1; }
    return slot;
}

int32_t aria_libc_condvar_wait(int64_t cv_handle, int64_t mutex_handle) {
    if (cv_handle < 0 || cv_handle >= MAX_CONDVARS || !g_condvars[cv_handle]) return -1;
    if (mutex_handle < 0 || mutex_handle >= MAX_MUTEXES || !g_mutexes[mutex_handle]) return -1;
    return (pthread_cond_wait(g_condvars[cv_handle], g_mutexes[mutex_handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_condvar_timedwait(int64_t cv_handle, int64_t mutex_handle, int64_t timeout_ns) {
    if (cv_handle < 0 || cv_handle >= MAX_CONDVARS || !g_condvars[cv_handle]) return -1;
    if (mutex_handle < 0 || mutex_handle >= MAX_MUTEXES || !g_mutexes[mutex_handle]) return -1;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += timeout_ns / 1000000000LL;
    ts.tv_nsec += timeout_ns % 1000000000LL;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec  += 1;
        ts.tv_nsec -= 1000000000L;
    }
    int r = pthread_cond_timedwait(g_condvars[cv_handle], g_mutexes[mutex_handle], &ts);
    if (r == 0) return 1;    /* signaled */
    return 0;                 /* timed out (ETIMEDOUT) */
}

int32_t aria_libc_condvar_signal(int64_t handle) {
    if (handle < 0 || handle >= MAX_CONDVARS || !g_condvars[handle]) return -1;
    return (pthread_cond_signal(g_condvars[handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_condvar_broadcast(int64_t handle) {
    if (handle < 0 || handle >= MAX_CONDVARS || !g_condvars[handle]) return -1;
    return (pthread_cond_broadcast(g_condvars[handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_condvar_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_CONDVARS || !g_condvars[handle]) return -1;
    pthread_cond_t* c = g_condvars[handle];
    pthread_cond_destroy(c);
    free(c);
    g_condvars[handle] = NULL;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Read-Write Lock API — aria_libc_rwlock_*
 * ════════════════════════════════════════════════════════════════════════════ */

static pthread_rwlock_t* g_rwlocks[MAX_RWLOCKS];

static int64_t alloc_rwlock_slot(pthread_rwlock_t* r) {
    for (int i = 0; i < MAX_RWLOCKS; i++) {
        if (!g_rwlocks[i]) { g_rwlocks[i] = r; return (int64_t)i; }
    }
    return -1;
}

int64_t aria_libc_rwlock_create(void) {
    pthread_rwlock_t* r = (pthread_rwlock_t*)calloc(1, sizeof(pthread_rwlock_t));
    if (!r) return -1;
    if (pthread_rwlock_init(r, NULL) != 0) { free(r); return -1; }
    int64_t slot = alloc_rwlock_slot(r);
    if (slot < 0) { pthread_rwlock_destroy(r); free(r); return -1; }
    return slot;
}

int32_t aria_libc_rwlock_rdlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    return (pthread_rwlock_rdlock(g_rwlocks[handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_rwlock_tryrdlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    int r = pthread_rwlock_tryrdlock(g_rwlocks[handle]);
    if (r == 0) return 1;
    return 0;
}

int32_t aria_libc_rwlock_wrlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    return (pthread_rwlock_wrlock(g_rwlocks[handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_rwlock_trywrlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    int r = pthread_rwlock_trywrlock(g_rwlocks[handle]);
    if (r == 0) return 1;
    return 0;
}

int32_t aria_libc_rwlock_unlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    return (pthread_rwlock_unlock(g_rwlocks[handle]) == 0) ? 0 : -1;
}

int32_t aria_libc_rwlock_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    pthread_rwlock_t* r = g_rwlocks[handle];
    pthread_rwlock_destroy(r);
    free(r);
    g_rwlocks[handle] = NULL;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Channel API — aria_libc_channel_*
 * Lock-based MPMC channel for int64 values.
 * Modes: BUFFERED (ring buffer), UNBUFFERED (rendezvous), ONESHOT (auto-close)
 * ════════════════════════════════════════════════════════════════════════════ */

#define CHANNEL_BUF_SIZE 1024
#define CHANNEL_MODE_BUFFERED   0
#define CHANNEL_MODE_UNBUFFERED 1
#define CHANNEL_MODE_ONESHOT    2

typedef struct {
    int64_t         buf[CHANNEL_BUF_SIZE];
    int32_t         capacity;
    int32_t         count;
    int32_t         head;
    int32_t         tail;
    int32_t         mode;
    pthread_mutex_t mtx;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    pthread_cond_t  rendezvous;
    bool            closed;
    bool            has_rendezvous_value;
    int64_t         rendezvous_value;
} LibcChannel;

static LibcChannel* g_channels[MAX_CHANNELS];

static int64_t alloc_channel_slot(LibcChannel* ch) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!g_channels[i]) { g_channels[i] = ch; return (int64_t)i; }
    }
    return -1;
}

static LibcChannel* libc_channel_alloc(int32_t capacity, int32_t mode) {
    LibcChannel* ch = (LibcChannel*)calloc(1, sizeof(LibcChannel));
    if (!ch) return NULL;
    ch->capacity = capacity;
    ch->mode = mode;
    pthread_mutex_init(&ch->mtx, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    pthread_cond_init(&ch->not_full, NULL);
    pthread_cond_init(&ch->rendezvous, NULL);
    return ch;
}

int64_t aria_libc_channel_create(int32_t capacity) {
    if (capacity <= 0 || capacity > CHANNEL_BUF_SIZE) capacity = CHANNEL_BUF_SIZE;
    LibcChannel* ch = libc_channel_alloc(capacity, CHANNEL_MODE_BUFFERED);
    if (!ch) return -1;
    return alloc_channel_slot(ch);
}

int64_t aria_libc_channel_create_unbuffered(void) {
    LibcChannel* ch = libc_channel_alloc(0, CHANNEL_MODE_UNBUFFERED);
    if (!ch) return -1;
    return alloc_channel_slot(ch);
}

int64_t aria_libc_channel_create_oneshot(void) {
    LibcChannel* ch = libc_channel_alloc(1, CHANNEL_MODE_ONESHOT);
    if (!ch) return -1;
    return alloc_channel_slot(ch);
}

int32_t aria_libc_channel_get_mode(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    return g_channels[handle]->mode;
}

int32_t aria_libc_channel_capacity(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    return g_channels[handle]->capacity;
}

int32_t aria_libc_channel_send(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    LibcChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);

    if (ch->closed) { pthread_mutex_unlock(&ch->mtx); return -1; }

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        while (ch->has_rendezvous_value && !ch->closed)
            pthread_cond_wait(&ch->rendezvous, &ch->mtx);
        if (ch->closed) { pthread_mutex_unlock(&ch->mtx); return -1; }
        ch->rendezvous_value = value;
        ch->has_rendezvous_value = true;
        pthread_cond_signal(&ch->not_empty);
        while (ch->has_rendezvous_value && !ch->closed)
            pthread_cond_wait(&ch->rendezvous, &ch->mtx);
        pthread_mutex_unlock(&ch->mtx);
        return (ch->closed && ch->has_rendezvous_value) ? -1 : 0;
    }

    /* Buffered / oneshot */
    while (ch->count >= ch->capacity && !ch->closed)
        pthread_cond_wait(&ch->not_full, &ch->mtx);
    if (ch->closed) { pthread_mutex_unlock(&ch->mtx); return -1; }
    ch->buf[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->not_empty);
    if (ch->mode == CHANNEL_MODE_ONESHOT) {
        ch->closed = true;
        pthread_cond_broadcast(&ch->not_empty);
        pthread_cond_broadcast(&ch->not_full);
    }
    pthread_mutex_unlock(&ch->mtx);
    return 0;
}

int32_t aria_libc_channel_try_send(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    LibcChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);
    if (ch->closed) { pthread_mutex_unlock(&ch->mtx); return -1; }

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        if (ch->has_rendezvous_value) { pthread_mutex_unlock(&ch->mtx); return -1; }
        ch->rendezvous_value = value;
        ch->has_rendezvous_value = true;
        pthread_cond_signal(&ch->not_empty);
        pthread_mutex_unlock(&ch->mtx);
        return 0;
    }

    if (ch->count >= ch->capacity) { pthread_mutex_unlock(&ch->mtx); return -1; }
    ch->buf[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->not_empty);
    if (ch->mode == CHANNEL_MODE_ONESHOT) {
        ch->closed = true;
        pthread_cond_broadcast(&ch->not_empty);
        pthread_cond_broadcast(&ch->not_full);
    }
    pthread_mutex_unlock(&ch->mtx);
    return 0;
}

int64_t aria_libc_channel_recv(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return 0;
    LibcChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        while (!ch->has_rendezvous_value && !ch->closed)
            pthread_cond_wait(&ch->not_empty, &ch->mtx);
        if (!ch->has_rendezvous_value) { pthread_mutex_unlock(&ch->mtx); return 0; }
        int64_t value = ch->rendezvous_value;
        ch->has_rendezvous_value = false;
        pthread_cond_signal(&ch->rendezvous);
        pthread_mutex_unlock(&ch->mtx);
        return value;
    }

    while (ch->count == 0 && !ch->closed)
        pthread_cond_wait(&ch->not_empty, &ch->mtx);
    if (ch->count == 0) { pthread_mutex_unlock(&ch->mtx); return 0; }
    int64_t value = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mtx);
    return value;
}

int64_t aria_libc_channel_try_recv(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return 0;
    LibcChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        if (!ch->has_rendezvous_value) { pthread_mutex_unlock(&ch->mtx); return 0; }
        int64_t value = ch->rendezvous_value;
        ch->has_rendezvous_value = false;
        pthread_cond_signal(&ch->rendezvous);
        pthread_mutex_unlock(&ch->mtx);
        return value;
    }

    if (ch->count == 0) { pthread_mutex_unlock(&ch->mtx); return 0; }
    int64_t value = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mtx);
    return value;
}

int32_t aria_libc_channel_count(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return 0;
    LibcChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);
    int32_t c = ch->count;
    if (ch->mode == CHANNEL_MODE_UNBUFFERED && ch->has_rendezvous_value) c = 1;
    pthread_mutex_unlock(&ch->mtx);
    return c;
}

int32_t aria_libc_channel_close(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    LibcChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);
    ch->closed = true;
    pthread_cond_broadcast(&ch->not_empty);
    pthread_cond_broadcast(&ch->not_full);
    pthread_cond_broadcast(&ch->rendezvous);
    pthread_mutex_unlock(&ch->mtx);
    return 0;
}

int32_t aria_libc_channel_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    LibcChannel* ch = g_channels[handle];
    aria_libc_channel_close(handle);
    pthread_mutex_destroy(&ch->mtx);
    pthread_cond_destroy(&ch->not_empty);
    pthread_cond_destroy(&ch->not_full);
    pthread_cond_destroy(&ch->rendezvous);
    free(ch);
    g_channels[handle] = NULL;
    return 0;
}

bool aria_libc_channel_is_closed(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return true;
    LibcChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);
    bool c = ch->closed;
    pthread_mutex_unlock(&ch->mtx);
    return c;
}

/* Channel select — poll multiple channels for first ready */
static int32_t libc_channel_select(int64_t* chs, int32_t n, int64_t timeout_ms) {
    if (!chs || n <= 0 || n > MAX_CHANNELS) return -1;

    /* Quick non-blocking scan */
    for (int32_t i = 0; i < n; i++) {
        int64_t h = chs[i];
        if (h < 0 || h >= MAX_CHANNELS || !g_channels[h]) continue;
        LibcChannel* ch = g_channels[h];
        pthread_mutex_lock(&ch->mtx);
        bool has_data = (ch->mode == CHANNEL_MODE_UNBUFFERED)
                        ? ch->has_rendezvous_value : (ch->count > 0);
        pthread_mutex_unlock(&ch->mtx);
        if (has_data) return i;
    }
    if (timeout_ms == 0) return -1;

    /* Blocking: spin-poll with 1ms sleeps */
    int64_t elapsed_ms = 0;
    struct timespec ts = { 0, 1000000L };
    while (timeout_ms < 0 || elapsed_ms < timeout_ms) {
        nanosleep(&ts, NULL);
        elapsed_ms++;
        bool all_closed = true;
        for (int32_t i = 0; i < n; i++) {
            int64_t h = chs[i];
            if (h < 0 || h >= MAX_CHANNELS || !g_channels[h]) continue;
            LibcChannel* ch = g_channels[h];
            pthread_mutex_lock(&ch->mtx);
            bool has_data = (ch->mode == CHANNEL_MODE_UNBUFFERED)
                            ? ch->has_rendezvous_value : (ch->count > 0);
            if (!ch->closed) all_closed = false;
            pthread_mutex_unlock(&ch->mtx);
            if (has_data) return i;
        }
        if (all_closed) return -1;
    }
    return -1;
}

int32_t aria_libc_channel_select2(int64_t ch0, int64_t ch1, int64_t timeout_ms) {
    int64_t chs[2] = { ch0, ch1 };
    return libc_channel_select(chs, 2, timeout_ms);
}

int32_t aria_libc_channel_select3(int64_t ch0, int64_t ch1, int64_t ch2, int64_t timeout_ms) {
    int64_t chs[3] = { ch0, ch1, ch2 };
    return libc_channel_select(chs, 3, timeout_ms);
}

int32_t aria_libc_channel_select4(int64_t ch0, int64_t ch1, int64_t ch2, int64_t ch3, int64_t timeout_ms) {
    int64_t chs[4] = { ch0, ch1, ch2, ch3 };
    return libc_channel_select(chs, 4, timeout_ms);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Actor API — aria_libc_actor_*
 * Lightweight actors with mailbox channels.
 * ════════════════════════════════════════════════════════════════════════════ */

#define ACTOR_MAILBOX_CAP 256
#define ACTOR_STATE_INIT    0
#define ACTOR_STATE_RUNNING 1
#define ACTOR_STATE_STOPPED 2

typedef struct {
    int64_t         mailbox_handle;
    int64_t         thread_handle;
    void*           behavior_func;
    int32_t         state;
    pthread_mutex_t state_mtx;
} LibcActor;

static LibcActor* g_actors[MAX_ACTORS];

static int64_t alloc_actor_slot(LibcActor* a) {
    for (int i = 0; i < MAX_ACTORS; i++) {
        if (!g_actors[i]) { g_actors[i] = a; return (int64_t)i; }
    }
    return -1;
}

/* Actor loop — called as a thread trampoline target */
static void actor_loop_wrapper(void* env, int64_t actor_handle) {
    (void)env;
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return;
    LibcActor* actor = g_actors[actor_handle];
    AriaLambdaFunc handler = (AriaLambdaFunc)actor->behavior_func;

    pthread_mutex_lock(&actor->state_mtx);
    actor->state = ACTOR_STATE_RUNNING;
    pthread_mutex_unlock(&actor->state_mtx);

    while (1) {
        pthread_mutex_lock(&actor->state_mtx);
        if (actor->state == ACTOR_STATE_STOPPED) {
            pthread_mutex_unlock(&actor->state_mtx);
            break;
        }
        pthread_mutex_unlock(&actor->state_mtx);

        if (aria_libc_channel_is_closed(actor->mailbox_handle) &&
            aria_libc_channel_count(actor->mailbox_handle) == 0)
            break;

        int64_t msg = aria_libc_channel_recv(actor->mailbox_handle);

        pthread_mutex_lock(&actor->state_mtx);
        if (actor->state == ACTOR_STATE_STOPPED) {
            pthread_mutex_unlock(&actor->state_mtx);
            break;
        }
        pthread_mutex_unlock(&actor->state_mtx);

        if (aria_libc_channel_is_closed(actor->mailbox_handle) &&
            aria_libc_channel_count(actor->mailbox_handle) == 0 && msg == 0)
            break;

        handler(NULL, msg);
    }

    pthread_mutex_lock(&actor->state_mtx);
    actor->state = ACTOR_STATE_STOPPED;
    pthread_mutex_unlock(&actor->state_mtx);
}

int64_t aria_libc_actor_spawn(void* behavior_func) {
    if (!behavior_func) return -1;
    int64_t mailbox = aria_libc_channel_create((int32_t)ACTOR_MAILBOX_CAP);
    if (mailbox < 0) return -1;

    LibcActor* actor = (LibcActor*)calloc(1, sizeof(LibcActor));
    if (!actor) { aria_libc_channel_destroy(mailbox); return -1; }
    actor->mailbox_handle = mailbox;
    actor->behavior_func = behavior_func;
    actor->state = ACTOR_STATE_INIT;
    pthread_mutex_init(&actor->state_mtx, NULL);

    int64_t actor_handle = alloc_actor_slot(actor);
    if (actor_handle < 0) {
        aria_libc_channel_destroy(mailbox);
        free(actor);
        return -1;
    }

    /* Spawn actor thread — the trampoline calls actor_loop_wrapper(NULL, actor_handle) */
    int64_t slot = alloc_thread_slot();
    if (slot < 0) {
        aria_libc_channel_destroy(mailbox);
        g_actors[actor_handle] = NULL;
        free(actor);
        return -1;
    }
    ThreadTrampData* data = (ThreadTrampData*)malloc(sizeof(ThreadTrampData));
    if (!data) {
        g_thread_used[slot] = false;
        aria_libc_channel_destroy(mailbox);
        g_actors[actor_handle] = NULL;
        free(actor);
        return -1;
    }
    data->func = actor_loop_wrapper;
    data->arg = actor_handle;
    if (pthread_create(&g_threads[slot], NULL, thread_trampoline, data) != 0) {
        free(data);
        g_thread_used[slot] = false;
        aria_libc_channel_destroy(mailbox);
        g_actors[actor_handle] = NULL;
        free(actor);
        return -1;
    }
    actor->thread_handle = slot;
    return actor_handle;
}

int32_t aria_libc_actor_send(int64_t actor_handle, int64_t message) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    return aria_libc_channel_send(g_actors[actor_handle]->mailbox_handle, message);
}

int32_t aria_libc_actor_try_send(int64_t actor_handle, int64_t message) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    return aria_libc_channel_try_send(g_actors[actor_handle]->mailbox_handle, message);
}

int64_t aria_libc_actor_mailbox(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    return g_actors[actor_handle]->mailbox_handle;
}

int32_t aria_libc_actor_pending(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return 0;
    return aria_libc_channel_count(g_actors[actor_handle]->mailbox_handle);
}

int32_t aria_libc_actor_is_alive(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    LibcActor* actor = g_actors[actor_handle];
    pthread_mutex_lock(&actor->state_mtx);
    int32_t alive = (actor->state == ACTOR_STATE_RUNNING) ? 1 : 0;
    pthread_mutex_unlock(&actor->state_mtx);
    return alive;
}

int32_t aria_libc_actor_stop(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    LibcActor* actor = g_actors[actor_handle];
    pthread_mutex_lock(&actor->state_mtx);
    actor->state = ACTOR_STATE_STOPPED;
    pthread_mutex_unlock(&actor->state_mtx);
    aria_libc_channel_close(actor->mailbox_handle);
    return aria_libc_thread_join(actor->thread_handle);
}

int32_t aria_libc_actor_destroy(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    LibcActor* actor = g_actors[actor_handle];
    pthread_mutex_lock(&actor->state_mtx);
    bool running = (actor->state == ACTOR_STATE_RUNNING);
    pthread_mutex_unlock(&actor->state_mtx);
    if (running) aria_libc_actor_stop(actor_handle);
    aria_libc_channel_destroy(actor->mailbox_handle);
    pthread_mutex_destroy(&actor->state_mtx);
    free(actor);
    g_actors[actor_handle] = NULL;
    return 0;
}

/* ── Actor Reply Channel helpers ─────────────────────────────────────────── */
/* A global reply channel handle so actor behaviors (lambdas) can send acks  */
/* without needing to capture Aria-side variables.                           */
static int64_t g_reply_channel = -1;

void aria_libc_set_reply_channel(int64_t ch) { g_reply_channel = ch; }
int64_t aria_libc_get_reply_channel(void)    { return g_reply_channel; }
int32_t aria_libc_reply(int64_t value) {
    if (g_reply_channel < 0) return -1;
    return aria_libc_channel_send(g_reply_channel, value);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Thread Pool API — aria_libc_pool_*
 * Fixed-size worker pool with circular task queue.
 * ════════════════════════════════════════════════════════════════════════════ */

#define MAX_POOL_WORKERS   256
#define TASK_QUEUE_CAP    4096

typedef struct {
    AriaLambdaFunc func;
    int64_t        arg;
} PoolTask;

typedef struct {
    PoolTask        tasks[TASK_QUEUE_CAP];
    int             head;
    int             tail;
    int             count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    bool            shutdown;
} TaskQueue;

static void tq_init(TaskQueue* q) {
    q->head = 0; q->tail = 0; q->count = 0; q->shutdown = false;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

static void tq_destroy(TaskQueue* q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

static int tq_push(TaskQueue* q, AriaLambdaFunc func, int64_t arg) {
    pthread_mutex_lock(&q->lock);
    while (q->count == TASK_QUEUE_CAP && !q->shutdown)
        pthread_cond_wait(&q->not_full, &q->lock);
    if (q->shutdown) { pthread_mutex_unlock(&q->lock); return -1; }
    q->tasks[q->tail].func = func;
    q->tasks[q->tail].arg  = arg;
    q->tail = (q->tail + 1) % TASK_QUEUE_CAP;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

static int tq_pop(TaskQueue* q, PoolTask* out) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->shutdown)
        pthread_cond_wait(&q->not_empty, &q->lock);
    if (q->count == 0 && q->shutdown) { pthread_mutex_unlock(&q->lock); return -1; }
    *out = q->tasks[q->head];
    q->head = (q->head + 1) % TASK_QUEUE_CAP;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

typedef struct {
    pthread_t    workers[MAX_POOL_WORKERS];
    int          num_workers;
    TaskQueue    queue;
    volatile int64_t active_tasks;
    pthread_mutex_t  active_lock;
    bool         alive;
} LibcPool;

static LibcPool* g_pools[MAX_POOLS];

static void* pool_worker(void* raw) {
    LibcPool* pool = (LibcPool*)raw;
    PoolTask task;
    while (tq_pop(&pool->queue, &task) == 0) {
        __sync_fetch_and_add(&pool->active_tasks, 1);
        task.func(NULL, task.arg);
        __sync_fetch_and_sub(&pool->active_tasks, 1);
    }
    return NULL;
}

int64_t aria_libc_pool_create(int32_t num_workers) {
    if (num_workers <= 0) num_workers = 4;
    if (num_workers > MAX_POOL_WORKERS) num_workers = MAX_POOL_WORKERS;

    LibcPool* pool = (LibcPool*)calloc(1, sizeof(LibcPool));
    if (!pool) return -1;
    pool->num_workers = num_workers;
    pool->active_tasks = 0;
    pool->alive = true;
    pthread_mutex_init(&pool->active_lock, NULL);
    tq_init(&pool->queue);

    for (int i = 0; i < num_workers; i++) {
        if (pthread_create(&pool->workers[i], NULL, pool_worker, pool) != 0) {
            pool->queue.shutdown = true;
            pthread_cond_broadcast(&pool->queue.not_empty);
            for (int j = 0; j < i; j++) pthread_join(pool->workers[j], NULL);
            tq_destroy(&pool->queue);
            pthread_mutex_destroy(&pool->active_lock);
            free(pool);
            return -1;
        }
    }

    for (int i = 0; i < MAX_POOLS; i++) {
        if (!g_pools[i]) { g_pools[i] = pool; return (int64_t)i; }
    }
    /* No slot — shut down */
    pool->queue.shutdown = true;
    pthread_cond_broadcast(&pool->queue.not_empty);
    for (int i = 0; i < num_workers; i++) pthread_join(pool->workers[i], NULL);
    tq_destroy(&pool->queue);
    pthread_mutex_destroy(&pool->active_lock);
    free(pool);
    return -1;
}

int32_t aria_libc_pool_submit(int64_t handle, void* func, int64_t arg) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    if (!func) return -1;
    LibcPool* pool = g_pools[handle];
    if (!pool->alive) return -1;
    return (tq_push(&pool->queue, (AriaLambdaFunc)func, arg) == 0) ? 0 : -1;
}

int32_t aria_libc_pool_shutdown(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    LibcPool* pool = g_pools[handle];
    if (!pool->alive) return -1;
    pool->alive = false;
    pool->queue.shutdown = true;
    pthread_cond_broadcast(&pool->queue.not_empty);
    pthread_cond_broadcast(&pool->queue.not_full);
    for (int i = 0; i < pool->num_workers; i++)
        pthread_join(pool->workers[i], NULL);
    tq_destroy(&pool->queue);
    pthread_mutex_destroy(&pool->active_lock);
    free(pool);
    g_pools[handle] = NULL;
    return 0;
}

int64_t aria_libc_pool_active_tasks(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return 0;
    return g_pools[handle]->active_tasks;
}

int64_t aria_libc_pool_pending_tasks(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return 0;
    LibcPool* pool = g_pools[handle];
    pthread_mutex_lock(&pool->queue.lock);
    int64_t c = (int64_t)pool->queue.count;
    pthread_mutex_unlock(&pool->queue.lock);
    return c;
}

int32_t aria_libc_pool_worker_count(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return 0;
    return g_pools[handle]->num_workers;
}

int32_t aria_libc_pool_wait_idle(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    LibcPool* pool = g_pools[handle];
    /* Spin with short sleeps until queue is empty and no active tasks */
    struct timespec ts = { 0, 500000L }; /* 0.5ms */
    while (1) {
        pthread_mutex_lock(&pool->queue.lock);
        int pending = pool->queue.count;
        pthread_mutex_unlock(&pool->queue.lock);
        if (pending == 0 && pool->active_tasks == 0) break;
        nanosleep(&ts, NULL);
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Shared Memory API — aria_libc_shm_*
 * Anonymous mmap-based shared memory regions.
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void*   ptr;
    int64_t size;
} ShmRegion;

static ShmRegion* g_shm[MAX_SHM];

int64_t aria_libc_shm_create(int64_t size) {
    if (size <= 0) return -1;
    void* ptr = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return -1;
    ShmRegion* r = (ShmRegion*)malloc(sizeof(ShmRegion));
    if (!r) { munmap(ptr, (size_t)size); return -1; }
    r->ptr = ptr;
    r->size = size;
    for (int i = 0; i < MAX_SHM; i++) {
        if (!g_shm[i]) { g_shm[i] = r; return (int64_t)i; }
    }
    munmap(ptr, (size_t)size);
    free(r);
    return -1;
}

int32_t aria_libc_shm_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return -1;
    ShmRegion* r = g_shm[handle];
    munmap(r->ptr, (size_t)r->size);
    free(r);
    g_shm[handle] = NULL;
    return 0;
}

int32_t aria_libc_shm_write_int64(int64_t handle, int64_t offset, int64_t value) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return -1;
    ShmRegion* r = g_shm[handle];
    if (offset < 0 || offset + (int64_t)sizeof(int64_t) > r->size) return -1;
    *(int64_t*)((char*)r->ptr + offset) = value;
    return 0;
}

int64_t aria_libc_shm_read_int64(int64_t handle, int64_t offset) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return 0;
    ShmRegion* r = g_shm[handle];
    if (offset < 0 || offset + (int64_t)sizeof(int64_t) > r->size) return 0;
    return *(int64_t*)((char*)r->ptr + offset);
}

int64_t aria_libc_shm_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return -1;
    return g_shm[handle]->size;
}
