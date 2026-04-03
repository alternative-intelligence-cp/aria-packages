/*
 * aria_libc_mem — Memory allocation and stdlib utilities for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_mem.so aria_libc_mem.c
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ── Allocation ──────────────────────────────────────────────────── */

int64_t aria_libc_mem_malloc(int64_t size) {
    if (size <= 0) return 0;
    void *p = malloc((size_t)size);
    return (int64_t)(uintptr_t)p;
}

int64_t aria_libc_mem_calloc(int64_t count, int64_t size) {
    if (count <= 0 || size <= 0) return 0;
    void *p = calloc((size_t)count, (size_t)size);
    return (int64_t)(uintptr_t)p;
}

int64_t aria_libc_mem_realloc(int64_t ptr, int64_t size) {
    void *p = realloc((void *)(uintptr_t)ptr, (size_t)size);
    return (int64_t)(uintptr_t)p;
}

void aria_libc_mem_free(int64_t ptr) {
    if (ptr) free((void *)(uintptr_t)ptr);
}

/* ── Sized Operations ────────────────────────────────────────────── */

int64_t aria_libc_mem_alloc_zeroed(int64_t size) {
    if (size <= 0) return 0;
    void *p = calloc(1, (size_t)size);
    return (int64_t)(uintptr_t)p;
}

void aria_libc_mem_copy(int64_t dst, int64_t src, int64_t n) {
    if (dst && src && n > 0)
        memcpy((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, (size_t)n);
}

void aria_libc_mem_move(int64_t dst, int64_t src, int64_t n) {
    if (dst && src && n > 0)
        memmove((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, (size_t)n);
}

void aria_libc_mem_zero(int64_t ptr, int64_t n) {
    if (ptr && n > 0)
        memset((void *)(uintptr_t)ptr, 0, (size_t)n);
}

/* ── Read/Write Primitives ───────────────────────────────────────── */

int64_t aria_libc_mem_read_i64(int64_t ptr, int64_t offset) {
    if (!ptr) return 0;
    return *(int64_t *)((uintptr_t)ptr + (uintptr_t)offset);
}

int32_t aria_libc_mem_read_i32(int64_t ptr, int64_t offset) {
    if (!ptr) return 0;
    return *(int32_t *)((uintptr_t)ptr + (uintptr_t)offset);
}

void aria_libc_mem_write_i64(int64_t ptr, int64_t offset, int64_t val) {
    if (!ptr) return;
    *(int64_t *)((uintptr_t)ptr + (uintptr_t)offset) = val;
}

void aria_libc_mem_write_i32(int64_t ptr, int64_t offset, int32_t val) {
    if (!ptr) return;
    *(int32_t *)((uintptr_t)ptr + (uintptr_t)offset) = val;
}

int32_t aria_libc_mem_read_byte(int64_t ptr, int64_t offset) {
    if (!ptr) return 0;
    return (int32_t)*(uint8_t *)((uintptr_t)ptr + (uintptr_t)offset);
}

void aria_libc_mem_write_byte(int64_t ptr, int64_t offset, int32_t val) {
    if (!ptr) return;
    *(uint8_t *)((uintptr_t)ptr + (uintptr_t)offset) = (uint8_t)val;
}

/* ── Conversion ──────────────────────────────────────────────────── */

int64_t aria_libc_mem_atoi(const char *s) {
    if (!s) return 0;
    return (int64_t)atoll(s);
}

double aria_libc_mem_atof(const char *s) {
    if (!s) return 0.0;
    return atof(s);
}

/* ── Random ──────────────────────────────────────────────────────── */

void aria_libc_mem_srand(int64_t seed) {
    srand((unsigned int)seed);
}

int32_t aria_libc_mem_rand(void) {
    return (int32_t)rand();
}

int32_t aria_libc_mem_rand_range(int32_t lo, int32_t hi) {
    if (hi <= lo) return lo;
    return lo + (rand() % (hi - lo + 1));
}

/* ── Process ─────────────────────────────────────────────────────── */

void aria_libc_mem_exit(int32_t code) {
    exit(code);
}

void aria_libc_mem_abort(void) {
    abort();
}

/* ── Environment ─────────────────────────────────────────────────── */

const char *aria_libc_mem_getenv(const char *name) {
    if (!name) return "";
    const char *v = getenv(name);
    return v ? v : "";
}

int32_t aria_libc_mem_setenv(const char *name, const char *value) {
    if (!name || !value) return -1;
    return setenv(name, value, 1);
}

int32_t aria_libc_mem_unsetenv(const char *name) {
    if (!name) return -1;
    return unsetenv(name);
}

int32_t aria_libc_mem_has_env(const char *name) {
    if (!name) return 0;
    return getenv(name) != NULL ? 1 : 0;
}

extern char **environ;

int32_t aria_libc_mem_environ_count(void) {
    int32_t count = 0;
    if (environ) {
        for (char **e = environ; *e; e++) count++;
    }
    return count;
}

/* ── Pointer Arithmetic Helpers ──────────────────────────────────── */

int64_t aria_libc_mem_ptr_add(int64_t ptr, int64_t offset) {
    return ptr + offset;
}

/* ── Global Slots (module-level state for pure Aria packages) ──── */

#define GLOBAL_SLOTS 64
static int64_t g_global_slots[GLOBAL_SLOTS] = {0};

int64_t aria_libc_mem_get_global(int32_t idx) {
    if (idx < 0 || idx >= GLOBAL_SLOTS) return 0;
    return g_global_slots[idx];
}

void aria_libc_mem_set_global(int32_t idx, int64_t val) {
    if (idx < 0 || idx >= GLOBAL_SLOTS) return;
    g_global_slots[idx] = val;
}

int64_t aria_libc_mem_size_of_i32(void) { return (int64_t)sizeof(int32_t); }
int64_t aria_libc_mem_size_of_i64(void) { return (int64_t)sizeof(int64_t); }
int64_t aria_libc_mem_size_of_f64(void) { return (int64_t)sizeof(double); }
int64_t aria_libc_mem_size_of_ptr(void) { return (int64_t)sizeof(void *); }

/* ── String / Buffer Helpers (for binary serialization) ──────────── */

const char *aria_libc_mem_make_string(int64_t src_ptr, int64_t offset, int64_t len) {
    if (len <= 0) return "";
    char *buf = (char *)malloc((size_t)(len + 1));
    if (!buf) return "";
    memcpy(buf, (const char *)((uintptr_t)src_ptr + (uintptr_t)offset), (size_t)len);
    buf[len] = '\0';
    return buf;
}

void aria_libc_mem_copy_string(int64_t dst_ptr, int64_t offset,
                                const char *src, int64_t len) {
    if (!src || len <= 0) return;
    memcpy((char *)((uintptr_t)dst_ptr + (uintptr_t)offset), src, (size_t)len);
}
