/*  aria_pqueue_shim.c — Priority queue (min-heap) for Aria FFI
 *
 *  Handle-based priority queue. Each element has an int64 value
 *  and an int32 priority. Lower priority number = higher priority (min-heap).
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_pqueue_shim.so aria_pqueue_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_PQUEUES   256
#define MAX_PQ_SIZE   65536

typedef struct {
    int64_t value;
    int32_t priority;
} PQEntry;

typedef struct {
    PQEntry *data;
    int32_t  size;
    int32_t  capacity;
    int      active;
} PQueue;

static PQueue g_pqueues[MAX_PQUEUES];

/* ── heap helpers ────────────────────────────────────────────────────── */

static void swap(PQEntry *a, PQEntry *b) {
    PQEntry tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sift_up(PQueue *pq, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq->data[idx].priority < pq->data[parent].priority) {
            swap(&pq->data[idx], &pq->data[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

static void sift_down(PQueue *pq, int idx) {
    while (1) {
        int smallest = idx;
        int left  = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left < pq->size && pq->data[left].priority < pq->data[smallest].priority)
            smallest = left;
        if (right < pq->size && pq->data[right].priority < pq->data[smallest].priority)
            smallest = right;
        if (smallest != idx) {
            swap(&pq->data[idx], &pq->data[smallest]);
            idx = smallest;
        } else {
            break;
        }
    }
}

/* ── create / destroy ────────────────────────────────────────────────── */

int64_t aria_pqueue_create(int32_t capacity) {
    if (capacity <= 0 || capacity > MAX_PQ_SIZE) return -1;
    for (int i = 0; i < MAX_PQUEUES; i++) {
        if (!g_pqueues[i].active) {
            g_pqueues[i].data = calloc((size_t)capacity, sizeof(PQEntry));
            if (!g_pqueues[i].data) return -1;
            g_pqueues[i].capacity = capacity;
            g_pqueues[i].size = 0;
            g_pqueues[i].active = 1;
            return (int64_t)i;
        }
    }
    return -1;
}

void aria_pqueue_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_PQUEUES) return;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active) return;
    free(pq->data);
    pq->data = NULL;
    pq->active = 0;
}

/* ── insert / extract ────────────────────────────────────────────────── */

/* Insert a value with priority. Returns 1 on success, 0 if full. */
int32_t aria_pqueue_insert(int64_t handle, int64_t value, int32_t priority) {
    if (handle < 0 || handle >= MAX_PQUEUES) return 0;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active || pq->size >= pq->capacity) return 0;

    pq->data[pq->size].value = value;
    pq->data[pq->size].priority = priority;
    sift_up(pq, pq->size);
    pq->size++;
    return 1;
}

/* Extract the minimum-priority element. Returns value, or -1 if empty. */
int64_t aria_pqueue_extract(int64_t handle) {
    if (handle < 0 || handle >= MAX_PQUEUES) return -1;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active || pq->size == 0) return -1;

    int64_t val = pq->data[0].value;
    pq->size--;
    if (pq->size > 0) {
        pq->data[0] = pq->data[pq->size];
        sift_down(pq, 0);
    }
    return val;
}

/* Peek at the minimum-priority element without removing. */
int64_t aria_pqueue_peek(int64_t handle) {
    if (handle < 0 || handle >= MAX_PQUEUES) return -1;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active || pq->size == 0) return -1;
    return pq->data[0].value;
}

/* Get the priority of the front element. */
int32_t aria_pqueue_peek_priority(int64_t handle) {
    if (handle < 0 || handle >= MAX_PQUEUES) return -1;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active || pq->size == 0) return -1;
    return pq->data[0].priority;
}

/* ── queries ─────────────────────────────────────────────────────────── */

int32_t aria_pqueue_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_PQUEUES) return 0;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active) return 0;
    return pq->size;
}

int32_t aria_pqueue_is_empty(int64_t handle) {
    if (handle < 0 || handle >= MAX_PQUEUES) return 1;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active) return 1;
    return pq->size == 0 ? 1 : 0;
}

int32_t aria_pqueue_capacity(int64_t handle) {
    if (handle < 0 || handle >= MAX_PQUEUES) return 0;
    PQueue *pq = &g_pqueues[handle];
    if (!pq->active) return 0;
    return pq->capacity;
}

/* ── test helpers ────────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_pqueue_assert_i32(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_pqueue_assert_i64(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %ld, got %ld)\n", msg, (long)expected, (long)actual);
    }
}

void aria_pqueue_test_summary(void) {
    printf("\n=== aria-pqueue test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
