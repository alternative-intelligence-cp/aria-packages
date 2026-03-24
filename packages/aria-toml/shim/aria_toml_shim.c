/*
 * aria_toml_shim.c — Minimal TOML parser/serializer for Aria packages
 *
 * Flat key-value store with dotted-key lookup (e.g., "server.port").
 * Supports: strings, integers, booleans, floats.
 * Handles: basic tables ([section]), key = "value" / key = 123 / key = true
 *
 * This is a minimal parser for config-file use cases, not a full TOML spec
 * implementation. Covers the common subset used by most configurations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

/* ═══════════════════ constants ═══════════════════ */

#define MAX_ENTRIES 512
#define MAX_KEY_LEN 256
#define MAX_VAL_LEN 4096

/* Value types */
#define TOML_STRING  0
#define TOML_INT     1
#define TOML_FLOAT   2
#define TOML_BOOL    3

/* ═══════════════════ state ═══════════════════ */

typedef struct {
    char    key[MAX_KEY_LEN];
    char    str_val[MAX_VAL_LEN];
    int64_t int_val;
    double  float_val;
    int     type;
    int     active;
} toml_entry_t;

static toml_entry_t g_entries[MAX_ENTRIES];
static int          g_count = 0;
static char         g_err[512] = "";
static char         g_result[MAX_VAL_LEN] = "";

/* ═══════════════════ helpers ═══════════════════ */

static void set_err(const char *msg) {
    snprintf(g_err, sizeof(g_err), "%s", msg);
}

static char *skip_ws(char *p) {
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return p;
}

static int find_entry(const char *key) {
    for (int i = 0; i < g_count; i++) {
        if (g_entries[i].active && strcmp(g_entries[i].key, key) == 0)
            return i;
    }
    return -1;
}

static int add_entry(const char *key) {
    int idx = find_entry(key);
    if (idx >= 0) return idx; /* overwrite existing */
    if (g_count >= MAX_ENTRIES) return -1;
    idx = g_count++;
    g_entries[idx].active = 1;
    snprintf(g_entries[idx].key, MAX_KEY_LEN, "%s", key);
    return idx;
}

/* ═══════════════════ parser ═══════════════════ */

static int parse_toml(const char *text) {
    g_count = 0;
    g_err[0] = '\0';

    char current_table[MAX_KEY_LEN] = "";
    const char *p = text;

    while (*p) {
        /* Skip whitespace and blank lines */
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0') break;

        /* Skip comment lines */
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Table header: [table] */
        if (*p == '[') {
            p++;
            /* Check for array-of-tables [[...]] — skip for now */
            if (*p == '[') {
                while (*p && *p != '\n') p++;
                continue;
            }
            char *dst = current_table;
            char *end = current_table + MAX_KEY_LEN - 1;
            while (*p && *p != ']' && dst < end) {
                if (*p != ' ' && *p != '\t')
                    *dst++ = *p;
                p++;
            }
            *dst = '\0';
            if (*p == ']') p++;
            continue;
        }

        /* Key = Value */
        char key[MAX_KEY_LEN];
        char full_key[MAX_KEY_LEN];
        char *kp = key;
        char *kend = key + MAX_KEY_LEN - 2;

        /* Parse key (bare or quoted) */
        if (*p == '"') {
            p++;
            while (*p && *p != '"' && kp < kend) *kp++ = *p++;
            if (*p == '"') p++;
        } else {
            while (*p && *p != '=' && *p != ' ' && *p != '\t' && kp < kend)
                *kp++ = *p++;
        }
        *kp = '\0';

        /* Skip to = */
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '=') {
            while (*p && *p != '\n') p++;
            continue;
        }
        p++; /* skip = */
        while (*p == ' ' || *p == '\t') p++;

        /* Build full key with table prefix */
        if (current_table[0])
            snprintf(full_key, MAX_KEY_LEN, "%s.%s", current_table, key);
        else
            snprintf(full_key, MAX_KEY_LEN, "%s", key);

        int idx = add_entry(full_key);
        if (idx < 0) {
            set_err("toml: too many entries");
            return -1;
        }

        /* Parse value */
        if (*p == '"') {
            /* String value */
            p++;
            char *vp = g_entries[idx].str_val;
            char *vend = vp + MAX_VAL_LEN - 1;
            while (*p && *p != '"' && vp < vend) {
                if (*p == '\\' && *(p+1)) {
                    p++;
                    switch (*p) {
                        case 'n':  *vp++ = '\n'; break;
                        case 't':  *vp++ = '\t'; break;
                        case '\\': *vp++ = '\\'; break;
                        case '"':  *vp++ = '"';  break;
                        default:   *vp++ = '\\'; *vp++ = *p; break;
                    }
                } else {
                    *vp++ = *p;
                }
                p++;
            }
            *vp = '\0';
            if (*p == '"') p++;
            g_entries[idx].type = TOML_STRING;
        } else if (strncmp(p, "true", 4) == 0 && (!p[4] || p[4] == ' ' || p[4] == '\n' || p[4] == '\r' || p[4] == '#')) {
            g_entries[idx].int_val = 1;
            g_entries[idx].type = TOML_BOOL;
            p += 4;
        } else if (strncmp(p, "false", 5) == 0 && (!p[5] || p[5] == ' ' || p[5] == '\n' || p[5] == '\r' || p[5] == '#')) {
            g_entries[idx].int_val = 0;
            g_entries[idx].type = TOML_BOOL;
            p += 5;
        } else {
            /* Number (int or float) */
            char numbuf[128];
            int ni = 0;
            while (*p && *p != '\n' && *p != '#' && *p != ' ' && *p != '\t' && ni < 127)
                numbuf[ni++] = *p++;
            numbuf[ni] = '\0';
            /* Remove underscores (TOML allows 1_000_000) */
            char clean[128];
            int ci = 0;
            for (int i = 0; numbuf[i]; i++)
                if (numbuf[i] != '_') clean[ci++] = numbuf[i];
            clean[ci] = '\0';

            if (strchr(clean, '.') || strchr(clean, 'e') || strchr(clean, 'E')) {
                g_entries[idx].float_val = strtod(clean, NULL);
                g_entries[idx].type = TOML_FLOAT;
            } else {
                g_entries[idx].int_val = strtoll(clean, NULL, 0);
                g_entries[idx].type = TOML_INT;
            }
        }

        /* Skip rest of line */
        while (*p && *p != '\n') p++;
    }

    return g_count;
}

/* ═══════════════════ public API ═══════════════════ */

const char *aria_toml_error(void) { return g_err; }
const char *aria_toml_last_result(void) { return g_result; }

int32_t aria_toml_parse(const char *text) {
    return (int32_t)parse_toml(text);
}

int32_t aria_toml_parse_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(g_err, sizeof(g_err), "toml: cannot open '%s': %s", path, strerror(errno));
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1024 * 1024) {
        fclose(f);
        set_err("toml: file too large or empty");
        return -1;
    }
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); set_err("toml: out of memory"); return -1; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    int32_t r = (int32_t)parse_toml(buf);
    free(buf);
    return r;
}

int32_t aria_toml_get_type(const char *key) {
    int idx = find_entry(key);
    if (idx < 0) return -1;
    return g_entries[idx].type;
}

const char *aria_toml_get_string(const char *key) {
    int idx = find_entry(key);
    if (idx < 0) { g_result[0] = '\0'; return g_result; }
    if (g_entries[idx].type == TOML_STRING) {
        return g_entries[idx].str_val;
    }
    /* Convert other types to string */
    if (g_entries[idx].type == TOML_INT)
        snprintf(g_result, sizeof(g_result), "%ld", (long)g_entries[idx].int_val);
    else if (g_entries[idx].type == TOML_FLOAT)
        snprintf(g_result, sizeof(g_result), "%g", g_entries[idx].float_val);
    else if (g_entries[idx].type == TOML_BOOL)
        snprintf(g_result, sizeof(g_result), "%s", g_entries[idx].int_val ? "true" : "false");
    return g_result;
}

int64_t aria_toml_get_int(const char *key) {
    int idx = find_entry(key);
    if (idx < 0) return 0;
    return g_entries[idx].int_val;
}

double aria_toml_get_float(const char *key) {
    int idx = find_entry(key);
    if (idx < 0) return 0.0;
    return g_entries[idx].float_val;
}

int32_t aria_toml_get_bool(const char *key) {
    int idx = find_entry(key);
    if (idx < 0) return 0;
    return (int32_t)g_entries[idx].int_val;
}

int32_t aria_toml_has_key(const char *key) {
    return find_entry(key) >= 0 ? 1 : 0;
}

int32_t aria_toml_set_string(const char *key, const char *val) {
    int idx = add_entry(key);
    if (idx < 0) return -1;
    g_entries[idx].type = TOML_STRING;
    snprintf(g_entries[idx].str_val, MAX_VAL_LEN, "%s", val);
    return 0;
}

int32_t aria_toml_set_int(const char *key, int64_t val) {
    int idx = add_entry(key);
    if (idx < 0) return -1;
    g_entries[idx].type = TOML_INT;
    g_entries[idx].int_val = val;
    return 0;
}

int32_t aria_toml_set_bool(const char *key, int32_t val) {
    int idx = add_entry(key);
    if (idx < 0) return -1;
    g_entries[idx].type = TOML_BOOL;
    g_entries[idx].int_val = val ? 1 : 0;
    return 0;
}

int32_t aria_toml_count(void) {
    return (int32_t)g_count;
}

const char *aria_toml_key_at(int32_t idx) {
    if (idx < 0 || idx >= g_count) return "";
    return g_entries[idx].key;
}

void aria_toml_clear(void) {
    g_count = 0;
    g_err[0] = '\0';
}

/* Serialize current entries back to TOML string */
const char *aria_toml_serialize(void) {
    static char out[MAX_VAL_LEN * 4];
    int pos = 0;
    char last_table[MAX_KEY_LEN] = "";

    for (int i = 0; i < g_count; i++) {
        if (!g_entries[i].active) continue;

        /* Extract table prefix and bare key */
        char *dot = strrchr(g_entries[i].key, '.');
        char table[MAX_KEY_LEN] = "";
        const char *bare_key;
        if (dot) {
            size_t tlen = dot - g_entries[i].key;
            if (tlen >= MAX_KEY_LEN) tlen = MAX_KEY_LEN - 1;
            memcpy(table, g_entries[i].key, tlen);
            table[tlen] = '\0';
            bare_key = dot + 1;
        } else {
            bare_key = g_entries[i].key;
        }

        /* Emit table header if changed */
        if (strcmp(table, last_table) != 0) {
            if (table[0]) {
                if (pos > 0) pos += snprintf(out + pos, sizeof(out) - pos, "\n");
                pos += snprintf(out + pos, sizeof(out) - pos, "[%s]\n", table);
            }
            snprintf(last_table, sizeof(last_table), "%s", table);
        }

        /* Emit key = value */
        switch (g_entries[i].type) {
            case TOML_STRING:
                pos += snprintf(out + pos, sizeof(out) - pos, "%s = \"%s\"\n", bare_key, g_entries[i].str_val);
                break;
            case TOML_INT:
                pos += snprintf(out + pos, sizeof(out) - pos, "%s = %ld\n", bare_key, (long)g_entries[i].int_val);
                break;
            case TOML_FLOAT:
                pos += snprintf(out + pos, sizeof(out) - pos, "%s = %g\n", bare_key, g_entries[i].float_val);
                break;
            case TOML_BOOL:
                pos += snprintf(out + pos, sizeof(out) - pos, "%s = %s\n", bare_key, g_entries[i].int_val ? "true" : "false");
                break;
        }
    }
    out[pos] = '\0';
    return out;
}
