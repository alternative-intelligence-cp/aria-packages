/*  aria_hex_shim.c — Hex encode/decode for Aria FFI
 *
 *  Encode raw bytes (as strings) to hexadecimal, decode hex back.
 *  Supports uppercase/lowercase output.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_hex_shim.so aria_hex_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_HEX_BUF  65536

static char g_result_buf[MAX_HEX_BUF];
static const char *g_last_result = "";

/* ── encode ──────────────────────────────────────────────────────────── */

static const char hex_lower[] = "0123456789abcdef";
static const char hex_upper[] = "0123456789ABCDEF";

/* Encode a string's bytes to hex. Returns length of hex string. */
int32_t aria_hex_encode_s(const char *input, int32_t uppercase) {
    if (!input) {
        g_result_buf[0] = '\0';
        g_last_result = g_result_buf;
        return 0;
    }
    const char *tbl = uppercase ? hex_upper : hex_lower;
    size_t ilen = strlen(input);
    size_t olen = ilen * 2;
    if (olen >= MAX_HEX_BUF) olen = MAX_HEX_BUF - 2;

    size_t j = 0;
    for (size_t i = 0; i < ilen && j + 1 < MAX_HEX_BUF; i++) {
        unsigned char c = (unsigned char)input[i];
        g_result_buf[j++] = tbl[(c >> 4) & 0x0F];
        g_result_buf[j++] = tbl[c & 0x0F];
    }
    g_result_buf[j] = '\0';
    g_last_result = g_result_buf;
    return (int32_t)j;
}

/* ── decode ──────────────────────────────────────────────────────────── */

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

/* Decode hex string back to bytes (as string). Returns length, or -1 on error. */
int32_t aria_hex_decode_s(const char *hex) {
    if (!hex) {
        g_result_buf[0] = '\0';
        g_last_result = g_result_buf;
        return -1;
    }
    size_t hlen = strlen(hex);
    if (hlen % 2 != 0) {
        g_result_buf[0] = '\0';
        g_last_result = g_result_buf;
        return -1;
    }

    size_t olen = hlen / 2;
    if (olen >= MAX_HEX_BUF) olen = MAX_HEX_BUF - 1;

    size_t j = 0;
    for (size_t i = 0; i + 1 < hlen && j < olen; i += 2) {
        int hi = hex_digit(hex[i]);
        int lo = hex_digit(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            g_result_buf[0] = '\0';
            g_last_result = g_result_buf;
            return -1;
        }
        g_result_buf[j++] = (char)((hi << 4) | lo);
    }
    g_result_buf[j] = '\0';
    g_last_result = g_result_buf;
    return (int32_t)j;
}

/* ── utility ─────────────────────────────────────────────────────────── */

/* Validate that a string is valid hex (even length, all hex digits). */
int32_t aria_hex_is_valid(const char *hex) {
    if (!hex) return 0;
    size_t len = strlen(hex);
    if (len == 0 || len % 2 != 0) return 0;
    for (size_t i = 0; i < len; i++) {
        if (hex_digit(hex[i]) < 0) return 0;
    }
    return 1;
}

/* Return encoded/decoded length from last operation. */
const char *aria_hex_last_result(void) { return g_last_result; }

/* ── test helpers ────────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_hex_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_hex_assert_result_eq(const char *expected, const char *msg) {
    if (g_last_result && strcmp(g_last_result, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected,
               g_last_result ? g_last_result : "(null)");
    }
}

void aria_hex_test_summary(void) {
    printf("\n=== aria-hex test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
