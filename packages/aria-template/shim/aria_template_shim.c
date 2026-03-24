/* aria_template_shim.c — String template engine with {{var}} substitution */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define MAX_VARS 128
#define MAX_KEY  128
#define MAX_VAL  4096
#define MAX_OUT  (64 * 1024)

static struct {
    char key[MAX_KEY];
    char val[MAX_VAL];
} vars[MAX_VARS];
static int var_count = 0;
static char output[MAX_OUT];

/* ---- set a template variable ---- */
int32_t aria_template_set(const char *key, const char *value) {
    /* Check if exists */
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].key, key) == 0) {
            strncpy(vars[i].val, value, MAX_VAL - 1);
            vars[i].val[MAX_VAL - 1] = '\0';
            return 1;
        }
    }
    if (var_count >= MAX_VARS) return 0;
    strncpy(vars[var_count].key, key, MAX_KEY - 1);
    vars[var_count].key[MAX_KEY - 1] = '\0';
    strncpy(vars[var_count].val, value, MAX_VAL - 1);
    vars[var_count].val[MAX_VAL - 1] = '\0';
    var_count++;
    return 1;
}

/* ---- get a variable value ---- */
const char *aria_template_get(const char *key) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].key, key) == 0) return vars[i].val;
    }
    return "";
}

/* ---- render template with {{var}} substitution ---- */
const char *aria_template_render(const char *tmpl) {
    char *out = output;
    char *end = output + MAX_OUT - 1;
    const char *p = tmpl;

    while (*p && out < end) {
        if (p[0] == '{' && p[1] == '{') {
            p += 2;
            /* Skip whitespace */
            while (*p == ' ') p++;
            /* Extract key */
            char key[MAX_KEY];
            int ki = 0;
            while (*p && !(p[0] == '}' && p[1] == '}') && ki < MAX_KEY - 1) {
                if (*p != ' ' || (p[1] != '}'))
                    key[ki++] = *p;
                p++;
            }
            /* Trim trailing space */
            while (ki > 0 && key[ki-1] == ' ') ki--;
            key[ki] = '\0';
            if (p[0] == '}' && p[1] == '}') p += 2;

            /* Lookup and substitute */
            const char *val = aria_template_get(key);
            size_t vlen = strlen(val);
            if (out + vlen < end) {
                memcpy(out, val, vlen);
                out += vlen;
            }
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    return output;
}

/* ---- clear all variables ---- */
void aria_template_clear(void) {
    var_count = 0;
}

/* ---- count variables ---- */
int32_t aria_template_count(void) {
    return (int32_t)var_count;
}

/* ---- has variable ---- */
int32_t aria_template_has(const char *key) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].key, key) == 0) return 1;
    }
    return 0;
}

/* ---- remove a variable ---- */
int32_t aria_template_remove(const char *key) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].key, key) == 0) {
            /* Shift remaining */
            for (int j = i; j < var_count - 1; j++)
                vars[j] = vars[j + 1];
            var_count--;
            return 1;
        }
    }
    return 0;
}
