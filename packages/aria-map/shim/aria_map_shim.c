/* aria_map_shim.c — String-keyed int64 hash map
 * CRITICAL: Aria passes string params as const char* (not AriaString struct)
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_MAPS 256
#define INITIAL_BUCKETS 64
#define LOAD_FACTOR_NUM 3
#define LOAD_FACTOR_DEN 4

typedef struct Entry {
    char *key;
    int64_t key_len;
    int64_t value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
    int64_t num_buckets;
    int64_t count;
} Map;

static Map *maps[MAX_MAPS];
static int next_slot = 0;

static int alloc_slot(void) {
    for (int i = next_slot; i < MAX_MAPS; i++) {
        if (!maps[i]) { next_slot = i + 1; return i; }
    }
    for (int i = 0; i < next_slot; i++) {
        if (!maps[i]) { next_slot = i + 1; return i; }
    }
    return -1;
}

static uint64_t hash_key(const char *key, int64_t len) {
    uint64_t h = 14695981039346656037ULL;
    for (int64_t i = 0; i < len; i++) {
        h ^= (uint8_t)key[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void rehash(Map *m) {
    int64_t new_nb = m->num_buckets * 2;
    Entry **new_b = (Entry **)calloc(new_nb, sizeof(Entry *));
    if (!new_b) return;
    for (int64_t i = 0; i < m->num_buckets; i++) {
        Entry *e = m->buckets[i];
        while (e) {
            Entry *next = e->next;
            uint64_t idx = hash_key(e->key, e->key_len) % (uint64_t)new_nb;
            e->next = new_b[idx];
            new_b[idx] = e;
            e = next;
        }
    }
    free(m->buckets);
    m->buckets = new_b;
    m->num_buckets = new_nb;
}

int64_t aria_map_shim_create(void) {
    int slot = alloc_slot();
    if (slot < 0) return -1;
    Map *m = (Map *)calloc(1, sizeof(Map));
    if (!m) return -1;
    m->buckets = (Entry **)calloc(INITIAL_BUCKETS, sizeof(Entry *));
    if (!m->buckets) { free(m); return -1; }
    m->num_buckets = INITIAL_BUCKETS;
    m->count = 0;
    maps[slot] = m;
    return (int64_t)slot;
}

void aria_map_shim_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_MAPS) return;
    Map *m = maps[handle];
    if (!m) return;
    for (int64_t i = 0; i < m->num_buckets; i++) {
        Entry *e = m->buckets[i];
        while (e) {
            Entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    free(m);
    maps[handle] = NULL;
}

void aria_map_shim_set(int64_t handle, const char *key, int64_t value) {
    if (handle < 0 || handle >= MAX_MAPS || !key) return;
    Map *m = maps[handle];
    if (!m) return;
    int64_t klen = (int64_t)strlen(key);

    uint64_t idx = hash_key(key, klen) % (uint64_t)m->num_buckets;
    Entry *e = m->buckets[idx];
    while (e) {
        if (e->key_len == klen && memcmp(e->key, key, klen) == 0) {
            e->value = value;
            return;
        }
        e = e->next;
    }
    Entry *ne = (Entry *)malloc(sizeof(Entry));
    if (!ne) return;
    ne->key = (char *)malloc(klen + 1);
    if (!ne->key) { free(ne); return; }
    memcpy(ne->key, key, klen);
    ne->key[klen] = '\0';
    ne->key_len = klen;
    ne->value = value;
    ne->next = m->buckets[idx];
    m->buckets[idx] = ne;
    m->count++;

    if (m->count * LOAD_FACTOR_DEN > m->num_buckets * LOAD_FACTOR_NUM) {
        rehash(m);
    }
}

int64_t aria_map_shim_get(int64_t handle, const char *key, int64_t default_val) {
    if (handle < 0 || handle >= MAX_MAPS || !key) return default_val;
    Map *m = maps[handle];
    if (!m) return default_val;
    int64_t klen = (int64_t)strlen(key);
    uint64_t idx = hash_key(key, klen) % (uint64_t)m->num_buckets;
    Entry *e = m->buckets[idx];
    while (e) {
        if (e->key_len == klen && memcmp(e->key, key, klen) == 0) {
            return e->value;
        }
        e = e->next;
    }
    return default_val;
}

int64_t aria_map_shim_has(int64_t handle, const char *key) {
    if (handle < 0 || handle >= MAX_MAPS || !key) return 0;
    Map *m = maps[handle];
    if (!m) return 0;
    int64_t klen = (int64_t)strlen(key);
    uint64_t idx = hash_key(key, klen) % (uint64_t)m->num_buckets;
    Entry *e = m->buckets[idx];
    while (e) {
        if (e->key_len == klen && memcmp(e->key, key, klen) == 0) {
            return 1;
        }
        e = e->next;
    }
    return 0;
}

int64_t aria_map_shim_remove(int64_t handle, const char *key) {
    if (handle < 0 || handle >= MAX_MAPS || !key) return 0;
    Map *m = maps[handle];
    if (!m) return 0;
    int64_t klen = (int64_t)strlen(key);
    uint64_t idx = hash_key(key, klen) % (uint64_t)m->num_buckets;
    Entry **pp = &m->buckets[idx];
    while (*pp) {
        Entry *e = *pp;
        if (e->key_len == klen && memcmp(e->key, key, klen) == 0) {
            *pp = e->next;
            free(e->key);
            free(e);
            m->count--;
            return 1;
        }
        pp = &e->next;
    }
    return 0;
}

int64_t aria_map_shim_length(int64_t handle) {
    if (handle < 0 || handle >= MAX_MAPS) return 0;
    Map *m = maps[handle];
    if (!m) return 0;
    return m->count;
}

void aria_map_shim_clear(int64_t handle) {
    if (handle < 0 || handle >= MAX_MAPS) return;
    Map *m = maps[handle];
    if (!m) return;
    for (int64_t i = 0; i < m->num_buckets; i++) {
        Entry *e = m->buckets[i];
        while (e) {
            Entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
        m->buckets[i] = NULL;
    }
    m->count = 0;
}
