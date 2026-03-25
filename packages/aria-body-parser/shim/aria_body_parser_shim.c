/*
 * aria_body_parser_shim.c — Request body parsing for Aria
 *
 * Supports:
 *   - JSON field extraction (top-level string, number, bool, null)
 *   - URL-encoded form data (key=value&key2=value2)
 *   - Content-type detection
 *   - Raw body passthrough
 *
 * Thread-safety: single-threaded (global state like aria-server).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* ── limits ──────────────────────────────────────────────────────── */
#define MAX_FIELDS     128
#define MAX_KEY_LEN    256
#define MAX_VAL_LEN    4096

/* ── parsed field storage ─────────────────────────────────────── */
static char   g_keys[MAX_FIELDS][MAX_KEY_LEN];
static char   g_vals[MAX_FIELDS][MAX_VAL_LEN];
static int    g_field_count = 0;

/* content type constants */
#define CT_UNKNOWN      0
#define CT_JSON         1
#define CT_FORM         2
#define CT_MULTIPART    3
#define CT_TEXT         4

static int    g_content_type = CT_UNKNOWN;
static int    g_parsed       = 0;
static char   g_raw_body[65536] = "";
static int    g_raw_len = 0;

/* error tracking */
static char   g_error[512] = "";

/* test tracking */
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ── helpers ─────────────────────────────────────────────────────── */

static void set_error(const char *msg) {
    snprintf(g_error, sizeof(g_error), "%s", msg);
}

static void clear_state(void) {
    g_field_count  = 0;
    g_content_type = CT_UNKNOWN;
    g_parsed       = 0;
    g_raw_body[0]  = '\0';
    g_raw_len      = 0;
    g_error[0]     = '\0';
}

/* URL-decode in place: %XX → char, + → space */
static void url_decode(char *dst, const char *src, size_t max) {
    size_t di = 0;
    for (size_t i = 0; src[i] && di < max - 1; ) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i+1]) && isxdigit((unsigned char)src[i+2])) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            dst[di++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[di++] = ' ';
            i++;
        } else {
            dst[di++] = src[i++];
        }
    }
    dst[di] = '\0';
}

/* ── JSON parser (top-level object fields) ──────────────────────── */

/* Skip whitespace, return pointer to next non-ws char */
static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

/* Parse a JSON string starting at opening quote, store in dst.
 * Returns pointer past the closing quote, or NULL on error. */
static const char *parse_json_string(const char *p, char *dst, size_t max) {
    if (*p != '"') return NULL;
    p++; /* skip opening quote */
    size_t di = 0;
    while (*p && *p != '"' && di < max - 1) {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '"':  dst[di++] = '"';  break;
                case '\\': dst[di++] = '\\'; break;
                case '/':  dst[di++] = '/';  break;
                case 'b':  dst[di++] = '\b'; break;
                case 'f':  dst[di++] = '\f'; break;
                case 'n':  dst[di++] = '\n'; break;
                case 'r':  dst[di++] = '\r'; break;
                case 't':  dst[di++] = '\t'; break;
                case 'u':  /* simplified: store as \uXXXX literal */
                    dst[di++] = '\\'; dst[di++] = 'u';
                    for (int j = 0; j < 4 && *p; j++) { p++; dst[di++] = *p; }
                    break;
                default:   dst[di++] = *p;   break;
            }
            p++;
        } else {
            dst[di++] = *p++;
        }
    }
    dst[di] = '\0';
    if (*p == '"') p++; /* skip closing quote */
    return p;
}

/* Parse a JSON value (string, number, bool, null, nested object/array)
 * and store as string representation in dst.
 * Returns pointer past the value. */
static const char *parse_json_value(const char *p, char *dst, size_t max) {
    p = skip_ws(p);
    if (*p == '"') {
        return parse_json_string(p, dst, max);
    }
    /* number, bool, null — read until delimiter */
    if (*p == '-' || isdigit((unsigned char)*p)) {
        size_t di = 0;
        while (*p && *p != ',' && *p != '}' && *p != ']' &&
               *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && di < max - 1) {
            dst[di++] = *p++;
        }
        dst[di] = '\0';
        return p;
    }
    if (strncmp(p, "true", 4) == 0)  { snprintf(dst, max, "true");  return p + 4; }
    if (strncmp(p, "false", 5) == 0) { snprintf(dst, max, "false"); return p + 5; }
    if (strncmp(p, "null", 4) == 0)  { snprintf(dst, max, "null");  return p + 4; }

    /* nested object or array — capture as raw string */
    if (*p == '{' || *p == '[') {
        char open = *p, close_ch = (open == '{') ? '}' : ']';
        int depth = 1;
        size_t di = 0;
        dst[di++] = *p++;
        int in_str = 0;
        while (*p && depth > 0 && di < max - 1) {
            if (*p == '\\' && in_str) {
                dst[di++] = *p++; if (*p && di < max - 1) dst[di++] = *p++;
                continue;
            }
            if (*p == '"') in_str = !in_str;
            if (!in_str) {
                if (*p == open) depth++;
                if (*p == close_ch) depth--;
            }
            dst[di++] = *p++;
        }
        dst[di] = '\0';
        return p;
    }

    dst[0] = '\0';
    return p;
}

/* Parse top-level JSON object into key/value pairs */
static int parse_json_body(const char *body) {
    const char *p = skip_ws(body);
    if (*p != '{') {
        set_error("JSON body must be an object");
        return -1;
    }
    p++; /* skip '{' */

    while (g_field_count < MAX_FIELDS) {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        /* parse key */
        if (*p != '"') {
            set_error("Expected string key in JSON object");
            return -1;
        }
        char key[MAX_KEY_LEN];
        p = parse_json_string(p, key, sizeof(key));
        if (!p) { set_error("Malformed JSON key"); return -1; }

        p = skip_ws(p);
        if (*p != ':') { set_error("Expected ':' after JSON key"); return -1; }
        p++;

        /* parse value */
        char val[MAX_VAL_LEN];
        p = parse_json_value(p, val, sizeof(val));
        if (!p) { set_error("Malformed JSON value"); return -1; }

        /* store */
        snprintf(g_keys[g_field_count], MAX_KEY_LEN, "%s", key);
        snprintf(g_vals[g_field_count], MAX_VAL_LEN, "%s", val);
        g_field_count++;
    }
    return g_field_count;
}

/* ── URL-encoded form parser ────────────────────────────────────── */

static int parse_form_body(const char *body) {
    if (!body || !*body) return 0;

    /* work on a copy */
    char buf[65536];
    snprintf(buf, sizeof(buf), "%s", body);

    char *saveptr;
    char *pair = strtok_r(buf, "&", &saveptr);
    while (pair && g_field_count < MAX_FIELDS) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            url_decode(g_keys[g_field_count], pair, MAX_KEY_LEN);
            url_decode(g_vals[g_field_count], eq + 1, MAX_VAL_LEN);
        } else {
            url_decode(g_keys[g_field_count], pair, MAX_KEY_LEN);
            g_vals[g_field_count][0] = '\0';
        }
        g_field_count++;
        pair = strtok_r(NULL, "&", &saveptr);
    }
    return g_field_count;
}

/* ── public API ─────────────────────────────────────────────────── */

/* Detect content type from a Content-Type header value */
int32_t aria_body_parser_detect(const char *content_type) {
    if (!content_type || !*content_type) return CT_UNKNOWN;
    if (strstr(content_type, "application/json"))                  return CT_JSON;
    if (strstr(content_type, "application/x-www-form-urlencoded")) return CT_FORM;
    if (strstr(content_type, "multipart/form-data"))               return CT_MULTIPART;
    if (strstr(content_type, "text/"))                             return CT_TEXT;
    return CT_UNKNOWN;
}

/* Parse a body string with the given content-type header value.
 * Returns the number of parsed fields, or -1 on error. */
int32_t aria_body_parser_parse(const char *body, const char *content_type) {
    clear_state();
    if (!body) { set_error("NULL body"); return -1; }

    /* store raw */
    snprintf(g_raw_body, sizeof(g_raw_body), "%s", body);
    g_raw_len = (int)strlen(body);

    g_content_type = aria_body_parser_detect(content_type);

    int result = 0;
    switch (g_content_type) {
        case CT_JSON:
            result = parse_json_body(body);
            break;
        case CT_FORM:
            result = parse_form_body(body);
            break;
        case CT_TEXT:
        case CT_UNKNOWN:
        default:
            /* raw body — no field parsing, just store it */
            result = 0;
            break;
    }

    g_parsed = (result >= 0) ? 1 : 0;
    return (int32_t)result;
}

/* Get a field value by key. Returns "" if not found. */
const char *aria_body_parser_field(const char *key) {
    if (!key) return "";
    for (int i = 0; i < g_field_count; i++) {
        if (strcmp(g_keys[i], key) == 0) return g_vals[i];
    }
    return "";
}

/* Check if a field exists. Returns 1 if found, 0 otherwise. */
int32_t aria_body_parser_has_field(const char *key) {
    if (!key) return 0;
    for (int i = 0; i < g_field_count; i++) {
        if (strcmp(g_keys[i], key) == 0) return 1;
    }
    return 0;
}

/* Get field count */
int32_t aria_body_parser_field_count(void) {
    return (int32_t)g_field_count;
}

/* Get field key by index */
const char *aria_body_parser_field_key(int32_t index) {
    if (index < 0 || index >= g_field_count) return "";
    return g_keys[index];
}

/* Get field value by index */
const char *aria_body_parser_field_value(int32_t index) {
    if (index < 0 || index >= g_field_count) return "";
    return g_vals[index];
}

/* Get detected content type code */
int32_t aria_body_parser_content_type(void) {
    return (int32_t)g_content_type;
}

/* Get raw body */
const char *aria_body_parser_raw(void) {
    return g_raw_body;
}

/* Get raw body length */
int32_t aria_body_parser_raw_length(void) {
    return (int32_t)g_raw_len;
}

/* Get parse error message (empty if no error) */
const char *aria_body_parser_error(void) {
    return g_error;
}

/* Reset state */
void aria_body_parser_reset(void) {
    clear_state();
}

/* ── test helpers ────────────────────────────────────────────────── */

void aria_body_parser_assert_str_eq(const char *label, const char *expected, const char *actual) {
    if (strcmp(expected, actual) == 0) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected \"%s\", got \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_body_parser_assert_int_eq(const char *label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected %d, got %d\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_body_parser_test_summary(void) {
    printf("\n--- aria-body-parser: %d passed, %d failed ---\n",
           g_tests_passed, g_tests_failed);
    g_tests_passed = 0;
    g_tests_failed = 0;
}
