/*  aria_ringbuf_shim.c — Ring buffer (circular buffer) for Aria FFI
 *
 *  Handle-based ring buffer with fixed capacity, int64 elements.
 *  Supports push, pop (FIFO), peek, size, capacity, full/empty checks.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_ringbuf_shim.so aria_ringbuf_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_RINGBUFS  256
#define MAX_CAPACITY  65536

typedef struct {
    int64_t *data;
    int32_t  capacity;
    int32_t  head;     /* read index  */
    int32_t  tail;     /* write index */
    int32_t  count;
    int      active;
} RingBuf;

static RingBuf g_ringbufs[MAX_RINGBUFS];

/* ── create / destroy ────────────────────────────────────────────────── */

int64_t aria_ringbuf_create(int32_t capacity) {
    if (capacity <= 0 || capacity > MAX_CAPACITY) return -1;
    for (int i = 0; i < MAX_RINGBUFS; i++) {
        if (!g_ringbufs[i].active) {
            g_ringbufs[i].data = calloc((size_t)capacity, sizeof(int64_t));
            if (!g_ringbufs[i].data) return -1;
            g_ringbufs[i].capacity = capacity;
            g_ringbufs[i].head = 0;
            g_ringbufs[i].tail = 0;
            g_ringbufs[i].count = 0;
            g_ringbufs[i].active = 1;
            return (int64_t)i;
        }
    }
    return -1;
}

void aria_ringbuf_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active) return;
    free(rb->data);
    rb->data = NULL;
    rb->active = 0;
}

/* ── push / pop ──────────────────────────────────────────────────────── */

/* Push a value. Returns 1 on success, 0 if full. */
int32_t aria_ringbuf_push(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return 0;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active || rb->count >= rb->capacity) return 0;

    rb->data[rb->tail] = value;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return 1;
}

/* Pop a value (FIFO). Returns the value, or -1 if empty.
 * Caller should check is_empty first to distinguish -1 value from error. */
int64_t aria_ringbuf_pop(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return -1;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active || rb->count == 0) return -1;

    int64_t val = rb->data[rb->head];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return val;
}

/* Peek at front without removing. Returns value, or -1 if empty. */
int64_t aria_ringbuf_peek(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return -1;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active || rb->count == 0) return -1;
    return rb->data[rb->head];
}

/* ── queries ─────────────────────────────────────────────────────────── */

int32_t aria_ringbuf_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return 0;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active) return 0;
    return rb->count;
}

int32_t aria_ringbuf_capacity(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return 0;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active) return 0;
    return rb->capacity;
}

int32_t aria_ringbuf_is_full(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return 0;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active) return 0;
    return rb->count >= rb->capacity ? 1 : 0;
}

int32_t aria_ringbuf_is_empty(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS) return 1;
    RingBuf *rb = &g_ringbufs[handle];
    if (!rb->active) return 1;
    return rb->count == 0 ? 1 : 0;
}

/* ── test helpers ────────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_ringbuf_assert_i32(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_ringbuf_assert_i64(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %ld, got %ld)\n", msg, (long)expected, (long)actual);
    }
}

void aria_ringbuf_test_summary(void) {
    printf("\n=== aria-ringbuf test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
