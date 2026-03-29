/*  aria_msgpack_shim.c — MessagePack serialization for Aria FFI
 *
 *  Handle-based pack/unpack buffer with support for:
 *  - Integers (int64), floats (double), booleans, nil, strings
 *  - Implements a subset of the MessagePack specification.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_msgpack_shim.so aria_msgpack_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define MAX_BUFFERS   64
#define MAX_BUF_SIZE  65536

/* ── MessagePack format bytes ────────────────────────────────────── */
/* https://github.com/msgpack/msgpack/blob/master/spec.md */

#define MP_NIL          0xc0
#define MP_FALSE        0xc2
#define MP_TRUE         0xc3
#define MP_FLOAT64      0xcb
#define MP_INT64        0xd3
#define MP_STR8         0xd9
#define MP_FIXSTR_MASK  0xa0  /* 101XXXXX, up to 31 bytes */
#define MP_POS_FIXINT   0x00  /* 0XXXXXXX, 0..127 */
#define MP_NEG_FIXINT   0xe0  /* 111XXXXX, -32..-1 */

/* ── buffer storage ──────────────────────────────────────────────── */

typedef struct {
    uint8_t data[MAX_BUF_SIZE];
    int32_t len;        /* write cursor (packing) */
    int32_t pos;        /* read cursor (unpacking) */
    int32_t active;
} MsgPackBuf;

static MsgPackBuf g_buffers[MAX_BUFFERS];
static int32_t    g_init = 0;

/* last unpacked values */
static int64_t  g_last_int   = 0;
static double   g_last_float = 0.0;
static char     g_last_str[MAX_BUF_SIZE];
static int32_t  g_last_type  = 0;  /* 0=nil, 1=bool, 2=int, 3=float, 4=string */
static int32_t  g_last_bool  = 0;

static void ensure_init(void) {
    if (!g_init) {
        memset(g_buffers, 0, sizeof(g_buffers));
        g_init = 1;
    }
}

/* ── buffer lifecycle ────────────────────────────────────────────── */

int64_t aria_msgpack_create(void) {
    ensure_init();
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!g_buffers[i].active) {
            memset(&g_buffers[i], 0, sizeof(MsgPackBuf));
            g_buffers[i].active = 1;
            return (int64_t)i;
        }
    }
    return -1;
}

void aria_msgpack_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_BUFFERS) return;
    g_buffers[handle].active = 0;
}

void aria_msgpack_clear(int64_t handle) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return;
    g_buffers[handle].len = 0;
    g_buffers[handle].pos = 0;
}

int32_t aria_msgpack_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return 0;
    return g_buffers[handle].len;
}

/* ── packing ─────────────────────────────────────────────────────── */

static int buf_write(MsgPackBuf *b, const uint8_t *data, int32_t n) {
    if (b->len + n > MAX_BUF_SIZE) return -1;
    memcpy(b->data + b->len, data, n);
    b->len += n;
    return 0;
}

static int buf_write_byte(MsgPackBuf *b, uint8_t byte) {
    return buf_write(b, &byte, 1);
}

int32_t aria_msgpack_pack_nil(int64_t handle) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return -1;
    return buf_write_byte(&g_buffers[handle], MP_NIL);
}

int32_t aria_msgpack_pack_bool(int64_t handle, int32_t val) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return -1;
    return buf_write_byte(&g_buffers[handle], val ? MP_TRUE : MP_FALSE);
}

int32_t aria_msgpack_pack_int(int64_t handle, int64_t val) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return -1;
    MsgPackBuf *b = &g_buffers[handle];

    if (val >= 0 && val <= 127) {
        return buf_write_byte(b, (uint8_t)val);
    } else if (val >= -32 && val < 0) {
        return buf_write_byte(b, (uint8_t)(MP_NEG_FIXINT | (val & 0x1f)));
    } else {
        /* Full int64 encoding */
        uint8_t buf[9];
        buf[0] = MP_INT64;
        for (int i = 0; i < 8; i++) {
            buf[8 - i] = (uint8_t)(val & 0xff);
            val >>= 8;
        }
        return buf_write(b, buf, 9);
    }
}

int32_t aria_msgpack_pack_float(int64_t handle, double val) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return -1;
    MsgPackBuf *b = &g_buffers[handle];

    uint8_t buf[9];
    buf[0] = MP_FLOAT64;
    uint64_t bits;
    memcpy(&bits, &val, 8);
    for (int i = 0; i < 8; i++) {
        buf[8 - i] = (uint8_t)(bits & 0xff);
        bits >>= 8;
    }
    return buf_write(b, buf, 9);
}

int32_t aria_msgpack_pack_str(int64_t handle, const char *str) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return -1;
    if (!str) return -1;
    MsgPackBuf *b = &g_buffers[handle];

    size_t slen = strlen(str);
    if (slen <= 31) {
        uint8_t hdr = MP_FIXSTR_MASK | (uint8_t)slen;
        if (buf_write_byte(b, hdr) < 0) return -1;
    } else if (slen <= 255) {
        if (buf_write_byte(b, MP_STR8) < 0) return -1;
        if (buf_write_byte(b, (uint8_t)slen) < 0) return -1;
    } else {
        return -1;  /* strings >255 bytes not supported in this version */
    }
    return buf_write(b, (const uint8_t *)str, (int32_t)slen);
}

/* ── unpacking ───────────────────────────────────────────────────── */

/* Reset read cursor to start. */
void aria_msgpack_rewind(int64_t handle) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return;
    g_buffers[handle].pos = 0;
}

static int buf_read(MsgPackBuf *b, uint8_t *out, int32_t n) {
    if (b->pos + n > b->len) return -1;
    memcpy(out, b->data + b->pos, n);
    b->pos += n;
    return 0;
}

static int buf_read_byte(MsgPackBuf *b, uint8_t *out) {
    return buf_read(b, out, 1);
}

/* Unpack next value. Returns type: 0=nil, 1=bool, 2=int, 3=float, 4=string, -1=error */
int32_t aria_msgpack_unpack_next(int64_t handle) {
    if (handle < 0 || handle >= MAX_BUFFERS || !g_buffers[handle].active) return -1;
    MsgPackBuf *b = &g_buffers[handle];

    uint8_t tag;
    if (buf_read_byte(b, &tag) < 0) return -1;

    if (tag == MP_NIL) {
        g_last_type = 0;
        return 0;
    } else if (tag == MP_FALSE) {
        g_last_type = 1;
        g_last_bool = 0;
        return 1;
    } else if (tag == MP_TRUE) {
        g_last_type = 1;
        g_last_bool = 1;
        return 1;
    } else if ((tag & 0x80) == 0) {
        /* positive fixint */
        g_last_type = 2;
        g_last_int = (int64_t)tag;
        return 2;
    } else if ((tag & 0xe0) == MP_NEG_FIXINT) {
        /* negative fixint */
        g_last_type = 2;
        g_last_int = (int64_t)(int8_t)tag;
        return 2;
    } else if (tag == MP_INT64) {
        uint8_t buf[8];
        if (buf_read(b, buf, 8) < 0) return -1;
        int64_t val = 0;
        for (int i = 0; i < 8; i++) {
            val = (val << 8) | buf[i];
        }
        g_last_type = 2;
        g_last_int = val;
        return 2;
    } else if (tag == MP_FLOAT64) {
        uint8_t buf[8];
        if (buf_read(b, buf, 8) < 0) return -1;
        uint64_t bits = 0;
        for (int i = 0; i < 8; i++) {
            bits = (bits << 8) | buf[i];
        }
        memcpy(&g_last_float, &bits, 8);
        g_last_type = 3;
        return 3;
    } else if ((tag & 0xe0) == MP_FIXSTR_MASK) {
        int32_t slen = tag & 0x1f;
        if (buf_read(b, (uint8_t *)g_last_str, slen) < 0) return -1;
        g_last_str[slen] = '\0';
        g_last_type = 4;
        return 4;
    } else if (tag == MP_STR8) {
        uint8_t slen_byte;
        if (buf_read_byte(b, &slen_byte) < 0) return -1;
        int32_t slen = slen_byte;
        if (buf_read(b, (uint8_t *)g_last_str, slen) < 0) return -1;
        g_last_str[slen] = '\0';
        g_last_type = 4;
        return 4;
    }

    return -1;  /* unknown tag */
}

/* Accessors for last unpacked value. */
int64_t aria_msgpack_get_int(void)     { return g_last_int; }
int32_t aria_msgpack_get_bool(void)    { return g_last_bool; }
const char *aria_msgpack_get_str(void) { return g_last_str; }
int32_t aria_msgpack_get_type(void)    { return g_last_type; }

/* Get float as int64 (bit representation) — Aria passes flt64 as double. */
double aria_msgpack_get_float(void)    { return g_last_float; }

/* ── test helpers ────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_msgpack_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_msgpack_assert_i64_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %ld, got %ld)\n", msg, expected, actual);
    }
}

void aria_msgpack_assert_str_eq(const char *actual, const char *expected, const char *msg) {
    if (actual && expected && strcmp(actual, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected '%s', got '%s')\n", msg,
               expected ? expected : "(null)", actual ? actual : "(null)");
    }
}

void aria_msgpack_test_summary(void) {
    printf("\n=== aria-msgpack test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
