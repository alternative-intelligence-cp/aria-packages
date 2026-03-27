// aria_sort_shim.c — Sorting and array manipulation for aria-sort
//
// Manages int64 arrays internally via handle table.
// Provides quicksort, insertion sort, is_sorted checks.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARRAYS 256
#define INITIAL_CAP 16

typedef struct {
    int64_t* data;
    int64_t  length;
    int64_t  capacity;
} SortArray;

static SortArray arrays[MAX_ARRAYS];
static int       used[MAX_ARRAYS];

static int64_t alloc_array(void) {
    for (int i = 0; i < MAX_ARRAYS; i++) {
        if (!used[i]) {
            used[i] = 1;
            arrays[i].data = (int64_t*)malloc(INITIAL_CAP * sizeof(int64_t));
            arrays[i].length = 0;
            arrays[i].capacity = INITIAL_CAP;
            return (int64_t)i;
        }
    }
    return -1;
}

static SortArray* get_array(int64_t handle) {
    if (handle < 0 || handle >= MAX_ARRAYS || !used[handle]) return NULL;
    return &arrays[handle];
}

// Create empty array → handle
int64_t aria_sort_shim_create(void) {
    return alloc_array();
}

// Free array
void aria_sort_shim_free(int64_t handle) {
    SortArray* a = get_array(handle);
    if (a) { free(a->data); a->data = NULL; a->length = 0; used[handle] = 0; }
}

// Push value
void aria_sort_shim_push(int64_t handle, int64_t value) {
    SortArray* a = get_array(handle);
    if (!a) return;
    if (a->length >= a->capacity) {
        a->capacity *= 2;
        a->data = (int64_t*)realloc(a->data, (size_t)a->capacity * sizeof(int64_t));
    }
    a->data[a->length++] = value;
}

// Get element at index
int64_t aria_sort_shim_get(int64_t handle, int64_t index) {
    SortArray* a = get_array(handle);
    if (!a || index < 0 || index >= a->length) return 0;
    return a->data[index];
}

// Get length
int64_t aria_sort_shim_length(int64_t handle) {
    SortArray* a = get_array(handle);
    return a ? a->length : 0;
}

// Quicksort partition
static int64_t partition(int64_t* data, int64_t lo, int64_t hi) {
    int64_t pivot = data[hi];
    int64_t i = lo - 1;
    for (int64_t j = lo; j < hi; j++) {
        if (data[j] <= pivot) {
            i++;
            int64_t tmp = data[i]; data[i] = data[j]; data[j] = tmp;
        }
    }
    int64_t tmp = data[i+1]; data[i+1] = data[hi]; data[hi] = tmp;
    return i + 1;
}

static void quicksort_impl(int64_t* data, int64_t lo, int64_t hi) {
    if (lo < hi) {
        int64_t p = partition(data, lo, hi);
        quicksort_impl(data, lo, p - 1);
        quicksort_impl(data, p + 1, hi);
    }
}

// Sort array in-place (quicksort)
void aria_sort_shim_quicksort(int64_t handle) {
    SortArray* a = get_array(handle);
    if (!a || a->length <= 1) return;
    quicksort_impl(a->data, 0, a->length - 1);
}

// Sort array in-place (insertion sort — stable, good for small arrays)
void aria_sort_shim_insertion_sort(int64_t handle) {
    SortArray* a = get_array(handle);
    if (!a || a->length <= 1) return;
    for (int64_t i = 1; i < a->length; i++) {
        int64_t key = a->data[i];
        int64_t j = i - 1;
        while (j >= 0 && a->data[j] > key) {
            a->data[j + 1] = a->data[j];
            j--;
        }
        a->data[j + 1] = key;
    }
}

// Check if sorted (ascending)
int64_t aria_sort_shim_is_sorted(int64_t handle) {
    SortArray* a = get_array(handle);
    if (!a || a->length <= 1) return 1;
    for (int64_t i = 1; i < a->length; i++) {
        if (a->data[i] < a->data[i-1]) return 0;
    }
    return 1;
}

// Reverse array in-place
void aria_sort_shim_reverse(int64_t handle) {
    SortArray* a = get_array(handle);
    if (!a || a->length <= 1) return;
    for (int64_t i = 0, j = a->length - 1; i < j; i++, j--) {
        int64_t tmp = a->data[i];
        a->data[i] = a->data[j];
        a->data[j] = tmp;
    }
}
