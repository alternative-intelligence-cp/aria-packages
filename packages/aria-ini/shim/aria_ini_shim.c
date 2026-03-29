/*  aria_ini_shim.c — INI config parser for Aria FFI
 *
 *  Parses INI text into sections with key-value pairs.
 *  Supports: [section] headers, key=value pairs, ; and # comments,
 *  whitespace trimming, default (global) section for keys before any header.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_ini_shim.so aria_ini_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_SECTIONS  128
#define MAX_KEYS      256
#define MAX_KEY_LEN   256
#define MAX_VAL_LEN   4096

typedef struct {
    char key[MAX_KEY_LEN];
    char val[MAX_VAL_LEN];
} IniEntry;

typedef struct {
    char name[MAX_KEY_LEN];
    IniEntry entries[MAX_KEYS];
    int32_t num_entries;
} IniSection;

static IniSection g_sections[MAX_SECTIONS];
static int32_t    g_num_sections = 0;
static const char *g_last_result = "";
/* persistent buffer for g_last_result so pointer stays valid */
static char g_result_buf[MAX_VAL_LEN];

/* ── helpers ─────────────────────────────────────────────────────────── */

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)*(e - 1))) e--;
    *e = '\0';
    return s;
}

static int find_section(const char *name) {
    for (int i = 0; i < g_num_sections; i++) {
        if (strcmp(g_sections[i].name, name) == 0) return i;
    }
    return -1;
}

static int ensure_section(const char *name) {
    int idx = find_section(name);
    if (idx >= 0) return idx;
    if (g_num_sections >= MAX_SECTIONS) return -1;
    idx = g_num_sections++;
    strncpy(g_sections[idx].name, name, MAX_KEY_LEN - 1);
    g_sections[idx].name[MAX_KEY_LEN - 1] = '\0';
    g_sections[idx].num_entries = 0;
    return idx;
}

/* ── parsing ─────────────────────────────────────────────────────────── */

static void ini_clear(void) {
    g_num_sections = 0;
    g_last_result = "";
}

int32_t aria_ini_parse(const char *text) {
    ini_clear();
    if (!text || !*text) return 0;

    /* start with a default global section "" */
    int cur = ensure_section("");

    /* work on a mutable copy */
    size_t len = strlen(text);
    char *buf = malloc(len + 1);
    if (!buf) return 0;
    memcpy(buf, text, len + 1);

    char *line = buf;
    while (line && *line) {
        /* find end of line */
        char *nl = strchr(line, '\n');
        if (nl) { *nl = '\0'; }

        /* trim CR if present */
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\r') line[llen - 1] = '\0';

        char *trimmed = trim(line);

        /* skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            line = nl ? nl + 1 : NULL;
            continue;
        }

        /* section header */
        if (*trimmed == '[') {
            char *close = strchr(trimmed, ']');
            if (close) {
                *close = '\0';
                char *secname = trim(trimmed + 1);
                cur = ensure_section(secname);
            }
            line = nl ? nl + 1 : NULL;
            continue;
        }

        /* key = value */
        char *eq = strchr(trimmed, '=');
        if (eq && cur >= 0) {
            *eq = '\0';
            char *key = trim(trimmed);
            char *val = trim(eq + 1);

            IniSection *sec = &g_sections[cur];
            if (sec->num_entries < MAX_KEYS) {
                IniEntry *e = &sec->entries[sec->num_entries++];
                strncpy(e->key, key, MAX_KEY_LEN - 1);
                e->key[MAX_KEY_LEN - 1] = '\0';
                strncpy(e->val, val, MAX_VAL_LEN - 1);
                e->val[MAX_VAL_LEN - 1] = '\0';
            }
        }

        line = nl ? nl + 1 : NULL;
    }

    free(buf);

    /* remove the default section if it has no entries */
    if (g_num_sections > 0 && g_sections[0].name[0] == '\0' &&
        g_sections[0].num_entries == 0) {
        /* shift remaining sections down */
        for (int i = 0; i < g_num_sections - 1; i++) {
            g_sections[i] = g_sections[i + 1];
        }
        g_num_sections--;
    }

    return g_num_sections;
}

/* ── accessors ───────────────────────────────────────────────────────── */

int32_t aria_ini_num_sections(void) { return g_num_sections; }

int32_t aria_ini_section_name_s(int32_t idx) {
    if (idx < 0 || idx >= g_num_sections) {
        g_last_result = "";
        return 0;
    }
    strncpy(g_result_buf, g_sections[idx].name, MAX_VAL_LEN - 1);
    g_result_buf[MAX_VAL_LEN - 1] = '\0';
    g_last_result = g_result_buf;
    return 1;
}

int32_t aria_ini_num_keys(const char *section) {
    int idx = find_section(section);
    if (idx < 0) return 0;
    return g_sections[idx].num_entries;
}

int32_t aria_ini_get_s(const char *section, const char *key) {
    int idx = find_section(section);
    if (idx < 0) {
        g_last_result = "";
        return 0;
    }
    IniSection *sec = &g_sections[idx];
    for (int i = 0; i < sec->num_entries; i++) {
        if (strcmp(sec->entries[i].key, key) == 0) {
            strncpy(g_result_buf, sec->entries[i].val, MAX_VAL_LEN - 1);
            g_result_buf[MAX_VAL_LEN - 1] = '\0';
            g_last_result = g_result_buf;
            return 1;
        }
    }
    g_last_result = "";
    return 0;
}

int32_t aria_ini_has_key(const char *section, const char *key) {
    int idx = find_section(section);
    if (idx < 0) return 0;
    IniSection *sec = &g_sections[idx];
    for (int i = 0; i < sec->num_entries; i++) {
        if (strcmp(sec->entries[i].key, key) == 0) return 1;
    }
    return 0;
}

const char *aria_ini_last_result(void) { return g_last_result; }

void aria_ini_cleanup(void) { ini_clear(); }

/* ── test helpers ────────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_ini_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_ini_assert_str_eq(const char *section, const char *key,
                            const char *expected, const char *msg) {
    int32_t found = aria_ini_get_s(section, key);
    const char *actual = g_last_result;
    if (found && strcmp(actual, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected,
               found ? actual : "(not found)");
    }
}

int32_t aria_ini_test_passed(void) { return g_test_passed; }
int32_t aria_ini_test_failed(void) { return g_test_failed; }

void aria_ini_test_summary(void) {
    printf("\n=== aria-ini test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
