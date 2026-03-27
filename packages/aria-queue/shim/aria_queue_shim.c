/* aria_queue_shim.c — Ring-buffer FIFO queue for int64 values */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_QUEUES 256
#define INITIAL_CAP 16

typedef struct {
    int64_t *buf;
    int64_t head;
    int64_t tail;
    int64_t count;
    int64_t cap;
} Queue;

static Queue *queues[MAX_QUEUES];
static int next_slot = 0;

static int alloc_slot(void) {
    for (int i = next_slot; i < MAX_QUEUES; i++) {
        if (!queues[i]) { next_slot = i + 1; return i; }
    }
    for (int i = 0; i < next_slot; i++) {
        if (!queues[i]) { next_slot = i + 1; return i; }
    }
    return -1;
}

int64_t aria_queue_shim_create(void) {
    int slot = alloc_slot();
    if (slot < 0) return -1;
    Queue *q = (Queue *)calloc(1, sizeof(Queue));
    if (!q) return -1;
    q->buf = (int64_t *)malloc(INITIAL_CAP * sizeof(int64_t));
    if (!q->buf) { free(q); return -1; }
    q->cap = INITIAL_CAP;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    queues[slot] = q;
    return (int64_t)slot;
}

void aria_queue_shim_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_QUEUES) return;
    Queue *q = queues[handle];
    if (!q) return;
    free(q->buf);
    free(q);
    queues[handle] = NULL;
}

static void grow(Queue *q) {
    int64_t new_cap = q->cap * 2;
    int64_t *new_buf = (int64_t *)malloc(new_cap * sizeof(int64_t));
    if (!new_buf) return;
    /* linearize ring buffer */
    for (int64_t i = 0; i < q->count; i++) {
        new_buf[i] = q->buf[(q->head + i) % q->cap];
    }
    free(q->buf);
    q->buf = new_buf;
    q->head = 0;
    q->tail = q->count;
    q->cap = new_cap;
}

void aria_queue_shim_enqueue(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_QUEUES) return;
    Queue *q = queues[handle];
    if (!q) return;
    if (q->count == q->cap) grow(q);
    q->buf[q->tail] = value;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
}

int64_t aria_queue_shim_dequeue(int64_t handle) {
    if (handle < 0 || handle >= MAX_QUEUES) return 0;
    Queue *q = queues[handle];
    if (!q || q->count == 0) return 0;
    int64_t val = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    return val;
}

int64_t aria_queue_shim_peek(int64_t handle) {
    if (handle < 0 || handle >= MAX_QUEUES) return 0;
    Queue *q = queues[handle];
    if (!q || q->count == 0) return 0;
    return q->buf[q->head];
}

int64_t aria_queue_shim_length(int64_t handle) {
    if (handle < 0 || handle >= MAX_QUEUES) return 0;
    Queue *q = queues[handle];
    if (!q) return 0;
    return q->count;
}

int64_t aria_queue_shim_is_empty(int64_t handle) {
    if (handle < 0 || handle >= MAX_QUEUES) return 1;
    Queue *q = queues[handle];
    if (!q) return 1;
    return q->count == 0 ? 1 : 0;
}

void aria_queue_shim_clear(int64_t handle) {
    if (handle < 0 || handle >= MAX_QUEUES) return;
    Queue *q = queues[handle];
    if (!q) return;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}
