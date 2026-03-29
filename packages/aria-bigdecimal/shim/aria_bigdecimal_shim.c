/*  aria_bigdecimal_shim.c — Arbitrary precision decimal arithmetic for Aria FFI
 *
 *  Handle-based BigDecimal: stores numbers as string-of-digits with
 *  sign, integer part, and decimal part. Supports add, subtract,
 *  multiply, divide (with configurable precision), compare, and formatting.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_bigdecimal_shim.so aria_bigdecimal_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_DECIMALS    32
#define MAX_DIGITS      512

/* ── BigDecimal storage ──────────────────────────────────────────── */

typedef struct {
    char    digits[MAX_DIGITS];  /* decimal string representation */
    int     in_use;
} BigDec;

static BigDec g_decs[MAX_DECIMALS];

/* ── AriaString return ───────────────────────────────────────────── */

typedef struct { const char *data; int64_t length; } AriaString;

static char g_str_buf[MAX_DIGITS + 16];
static AriaString g_str_ret;

static AriaString make_str(const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (len >= sizeof(g_str_buf)) len = sizeof(g_str_buf) - 1;
    memcpy(g_str_buf, s ? s : "", len);
    g_str_buf[len] = '\0';
    g_str_ret.data = g_str_buf;
    g_str_ret.length = (int64_t)len;
    return g_str_ret;
}

/* ── internal helpers ────────────────────────────────────────────── */

/* Normalize: remove leading zeros (keep at least one), remove trailing
   zeros after decimal point, remove trailing dot */
static void normalize(char *s) {
    if (!s || !*s) return;

    int neg = 0;
    char *p = s;
    if (*p == '-') { neg = 1; p++; }

    /* find decimal point */
    char *dot = strchr(p, '.');

    /* remove leading zeros from integer part */
    char *start = p;
    while (*start == '0' && *(start + 1) && *(start + 1) != '.') start++;
    if (start != p) memmove(p, start, strlen(start) + 1);

    /* if there's a decimal part, remove trailing zeros */
    dot = strchr(p, '.');
    if (dot) {
        size_t len = strlen(s);
        while (len > 1 && s[len - 1] == '0') { s[--len] = '\0'; }
        if (s[len - 1] == '.') s[--len] = '\0';
    }

    /* handle "-0" */
    if (neg && strcmp(p, "0") == 0) {
        memmove(s, p, strlen(p) + 1);
    }
}

/* Compare absolute values of two decimal strings (no sign).
   Returns: -1, 0, or 1 */
static int cmp_abs(const char *a, const char *b) {
    /* split integer and fraction */
    const char *da = strchr(a, '.'), *db = strchr(b, '.');
    size_t ia = da ? (size_t)(da - a) : strlen(a);
    size_t ib = db ? (size_t)(db - b) : strlen(b);

    /* compare integer parts by length, then lexicographically */
    if (ia != ib) return ia > ib ? 1 : -1;
    int c = strncmp(a, b, ia);
    if (c != 0) return c > 0 ? 1 : -1;

    /* compare fractional parts */
    const char *fa = da ? da + 1 : "";
    const char *fb = db ? db + 1 : "";
    size_t fla = strlen(fa), flb = strlen(fb);
    size_t ml = fla > flb ? fla : flb;
    for (size_t i = 0; i < ml; i++) {
        char ca = i < fla ? fa[i] : '0';
        char cb = i < flb ? fb[i] : '0';
        if (ca != cb) return ca > cb ? 1 : -1;
    }
    return 0;
}

/* Add two positive decimal strings, result into buf */
static void add_positive(const char *a, const char *b, char *buf, size_t bufsz) {
    /* Align on decimal point */
    const char *da = strchr(a, '.'), *db = strchr(b, '.');
    size_t ia = da ? (size_t)(da - a) : strlen(a);
    size_t ib = db ? (size_t)(db - b) : strlen(b);
    const char *fa = da ? da + 1 : "";
    const char *fb = db ? db + 1 : "";
    size_t fla = strlen(fa), flb = strlen(fb);

    size_t max_int = ia > ib ? ia : ib;
    size_t max_frac = fla > flb ? fla : flb;
    size_t total = max_int + 1 + max_frac + 2; /* +1 carry, +1 dot, +1 null */
    if (total > bufsz) total = bufsz;

    /* work from right to left */
    char tmp[MAX_DIGITS * 2];
    memset(tmp, 0, sizeof(tmp));
    int carry = 0;

    /* fraction part */
    for (int i = (int)max_frac - 1; i >= 0; i--) {
        int da2 = (size_t)i < fla ? fa[i] - '0' : 0;
        int db2 = (size_t)i < flb ? fb[i] - '0' : 0;
        int s = da2 + db2 + carry;
        carry = s / 10;
        tmp[max_int + 1 + (size_t)i] = (char)('0' + s % 10);
    }
    if (max_frac > 0) tmp[max_int] = '.';

    /* integer part */
    for (int i = 0; i < (int)max_int; i++) {
        int da2 = ((int)ia - 1 - i >= 0) ? a[ia - 1 - (size_t)i] - '0' : 0;
        int db2 = ((int)ib - 1 - i >= 0) ? b[ib - 1 - (size_t)i] - '0' : 0;
        int s = da2 + db2 + carry;
        carry = s / 10;
        tmp[max_int - 1 - (size_t)i] = (char)('0' + s % 10);
    }

    if (carry) {
        memmove(tmp + 1, tmp, strlen(tmp) + 1);
        tmp[0] = (char)('0' + carry);
    }

    size_t rlen = strlen(tmp);
    if (rlen >= bufsz) rlen = bufsz - 1;
    memcpy(buf, tmp, rlen);
    buf[rlen] = '\0';
}

/* Subtract b from a (a >= b, both positive), result into buf */
static void sub_positive(const char *a, const char *b, char *buf, size_t bufsz) {
    const char *da = strchr(a, '.'), *db = strchr(b, '.');
    size_t ia = da ? (size_t)(da - a) : strlen(a);
    size_t ib = db ? (size_t)(db - b) : strlen(b);
    const char *fa = da ? da + 1 : "";
    const char *fb = db ? db + 1 : "";
    size_t fla = strlen(fa), flb = strlen(fb);

    size_t max_int = ia > ib ? ia : ib;
    size_t max_frac = fla > flb ? fla : flb;

    char tmp[MAX_DIGITS * 2];
    memset(tmp, 0, sizeof(tmp));
    int borrow = 0;

    /* fraction part */
    for (int i = (int)max_frac - 1; i >= 0; i--) {
        int da2 = (size_t)i < fla ? fa[i] - '0' : 0;
        int db2 = (size_t)i < flb ? fb[i] - '0' : 0;
        int d = da2 - db2 - borrow;
        if (d < 0) { d += 10; borrow = 1; } else { borrow = 0; }
        tmp[max_int + 1 + (size_t)i] = (char)('0' + d);
    }
    if (max_frac > 0) tmp[max_int] = '.';

    /* integer part */
    for (int i = 0; i < (int)max_int; i++) {
        int da2 = ((int)ia - 1 - i >= 0) ? a[ia - 1 - (size_t)i] - '0' : 0;
        int db2 = ((int)ib - 1 - i >= 0) ? b[ib - 1 - (size_t)i] - '0' : 0;
        int d = da2 - db2 - borrow;
        if (d < 0) { d += 10; borrow = 1; } else { borrow = 0; }
        tmp[max_int - 1 - (size_t)i] = (char)('0' + d);
    }

    size_t rlen = strlen(tmp);
    if (rlen >= bufsz) rlen = bufsz - 1;
    memcpy(buf, tmp, rlen);
    buf[rlen] = '\0';
    normalize(buf);
}

/* ── lifecycle ───────────────────────────────────────────────────── */

int32_t aria_bigdec_create(const char *value) {
    for (int32_t i = 0; i < MAX_DECIMALS; i++) {
        if (!g_decs[i].in_use) {
            g_decs[i].in_use = 1;
            if (value) {
                strncpy(g_decs[i].digits, value, MAX_DIGITS - 1);
                g_decs[i].digits[MAX_DIGITS - 1] = '\0';
            } else {
                strcpy(g_decs[i].digits, "0");
            }
            normalize(g_decs[i].digits);
            return i;
        }
    }
    return -1;
}

void aria_bigdec_destroy(int32_t h) {
    if (h < 0 || h >= MAX_DECIMALS) return;
    g_decs[h].in_use = 0;
}

AriaString aria_bigdec_to_string(int32_t h) {
    if (h < 0 || h >= MAX_DECIMALS || !g_decs[h].in_use) return make_str("0");
    return make_str(g_decs[h].digits);
}

/* ── comparison ──────────────────────────────────────────────────── */

int32_t aria_bigdec_compare(int32_t a, int32_t b) {
    if (a < 0 || a >= MAX_DECIMALS || !g_decs[a].in_use) return 0;
    if (b < 0 || b >= MAX_DECIMALS || !g_decs[b].in_use) return 0;

    const char *sa = g_decs[a].digits, *sb = g_decs[b].digits;
    int nega = (*sa == '-'), negb = (*sb == '-');
    if (nega && !negb) return -1;
    if (!nega && negb) return 1;

    const char *pa = nega ? sa + 1 : sa;
    const char *pb = negb ? sb + 1 : sb;
    int c = cmp_abs(pa, pb);
    return nega ? -c : c;
}

int32_t aria_bigdec_equals(int32_t a, int32_t b) {
    return aria_bigdec_compare(a, b) == 0 ? 1 : 0;
}

/* ── arithmetic ──────────────────────────────────────────────────── */

int32_t aria_bigdec_add(int32_t a, int32_t b) {
    if (a < 0 || a >= MAX_DECIMALS || !g_decs[a].in_use) return -1;
    if (b < 0 || b >= MAX_DECIMALS || !g_decs[b].in_use) return -1;

    const char *sa = g_decs[a].digits, *sb = g_decs[b].digits;
    int nega = (*sa == '-'), negb = (*sb == '-');
    const char *pa = nega ? sa + 1 : sa;
    const char *pb = negb ? sb + 1 : sb;

    char result[MAX_DIGITS];
    memset(result, 0, sizeof(result));

    if (nega == negb) {
        /* same sign: add magnitudes */
        add_positive(pa, pb, result, MAX_DIGITS - 1);
        if (nega) {
            memmove(result + 1, result, strlen(result) + 1);
            result[0] = '-';
        }
    } else {
        /* different signs: subtract smaller from larger */
        int c = cmp_abs(pa, pb);
        if (c == 0) {
            strcpy(result, "0");
        } else if (c > 0) {
            /* |a| > |b| */
            sub_positive(pa, pb, result, MAX_DIGITS - 1);
            if (nega) {
                memmove(result + 1, result, strlen(result) + 1);
                result[0] = '-';
            }
        } else {
            /* |b| > |a| */
            sub_positive(pb, pa, result, MAX_DIGITS - 1);
            if (negb) {
                memmove(result + 1, result, strlen(result) + 1);
                result[0] = '-';
            }
        }
    }

    normalize(result);
    return aria_bigdec_create(result);
}

int32_t aria_bigdec_negate(int32_t a) {
    if (a < 0 || a >= MAX_DECIMALS || !g_decs[a].in_use) return -1;
    char tmp[MAX_DIGITS];
    const char *sa = g_decs[a].digits;
    if (*sa == '-') {
        strncpy(tmp, sa + 1, MAX_DIGITS - 1);
    } else if (strcmp(sa, "0") == 0) {
        strcpy(tmp, "0");
    } else {
        tmp[0] = '-';
        strncpy(tmp + 1, sa, MAX_DIGITS - 2);
    }
    tmp[MAX_DIGITS - 1] = '\0';
    return aria_bigdec_create(tmp);
}

int32_t aria_bigdec_subtract(int32_t a, int32_t b) {
    int32_t neg_b = aria_bigdec_negate(b);
    if (neg_b < 0) return -1;
    int32_t result = aria_bigdec_add(a, neg_b);
    aria_bigdec_destroy(neg_b);
    return result;
}

int32_t aria_bigdec_multiply(int32_t a, int32_t b) {
    if (a < 0 || a >= MAX_DECIMALS || !g_decs[a].in_use) return -1;
    if (b < 0 || b >= MAX_DECIMALS || !g_decs[b].in_use) return -1;

    const char *sa = g_decs[a].digits, *sb = g_decs[b].digits;
    int nega = (*sa == '-'), negb = (*sb == '-');
    const char *pa = nega ? sa + 1 : sa;
    const char *pb = negb ? sb + 1 : sb;
    int neg_result = nega != negb;

    /* count decimal places */
    const char *da = strchr(pa, '.'), *db = strchr(pb, '.');
    size_t dec_a = da ? strlen(da + 1) : 0;
    size_t dec_b = db ? strlen(db + 1) : 0;
    size_t total_dec = dec_a + dec_b;

    /* remove decimal points to get integer strings */
    char ia[MAX_DIGITS], ib[MAX_DIGITS];
    int ia_len = 0, ib_len = 0;
    for (const char *p = pa; *p; p++) {
        if (*p != '.') ia[ia_len++] = *p;
    }
    ia[ia_len] = '\0';
    for (const char *p = pb; *p; p++) {
        if (*p != '.') ib[ib_len++] = *p;
    }
    ib[ib_len] = '\0';

    /* grade-school multiplication */
    int product[MAX_DIGITS * 2];
    memset(product, 0, sizeof(product));
    int plen = ia_len + ib_len;

    for (int i = ia_len - 1; i >= 0; i--) {
        for (int j = ib_len - 1; j >= 0; j--) {
            int mul = (ia[i] - '0') * (ib[j] - '0');
            int p1 = i + j, p2 = i + j + 1;
            int sum = mul + product[p2];
            product[p2] = sum % 10;
            product[p1] += sum / 10;
        }
    }

    /* convert to string and insert decimal point */
    char result[MAX_DIGITS * 2];
    int ri = 0;
    if (neg_result) result[ri++] = '-';

    /* skip leading zeros */
    int start = 0;
    while (start < plen - 1 && product[start] == 0) start++;

    int digits_written = plen - start;
    int point_pos = digits_written - (int)total_dec;

    if (point_pos <= 0) {
        result[ri++] = '0';
        result[ri++] = '.';
        for (int k = 0; k < -point_pos; k++) result[ri++] = '0';
        for (int k = start; k < plen; k++) result[ri++] = (char)('0' + product[k]);
    } else {
        for (int k = start; k < plen; k++) {
            if (k - start == point_pos && total_dec > 0) result[ri++] = '.';
            result[ri++] = (char)('0' + product[k]);
        }
    }
    result[ri] = '\0';
    normalize(result);
    return aria_bigdec_create(result);
}

int32_t aria_bigdec_is_zero(int32_t h) {
    if (h < 0 || h >= MAX_DECIMALS || !g_decs[h].in_use) return 0;
    const char *s = g_decs[h].digits;
    if (*s == '-') s++;
    /* check if all digits are zero */
    for (; *s; s++) {
        if (*s != '0' && *s != '.') return 0;
    }
    return 1;
}

int32_t aria_bigdec_is_negative(int32_t h) {
    if (h < 0 || h >= MAX_DECIMALS || !g_decs[h].in_use) return 0;
    return g_decs[h].digits[0] == '-' ? 1 : 0;
}

/* ================================================================= */
/* ── test harness ────────────────────────────────────────────────── */
/* ================================================================= */

static int t_pass = 0, t_fail = 0;

static void assert_int_eq(int32_t a, int32_t b, const char *msg) {
    if (a == b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %d, expected %d\n", msg, a, b); }
}

static void assert_str_eq(const char *a, const char *b, const char *msg) {
    if (a && b && strcmp(a, b) == 0) { t_pass++; printf("  PASS: %s\n", msg); }
    else { t_fail++; printf("  FAIL: %s — got '%s', expected '%s'\n", msg, a ? a : "(null)", b ? b : "(null)"); }
}

int32_t aria_test_bigdec_run(void) {
    printf("=== aria-bigdecimal tests ===\n");

    /* create and to_string */
    int32_t a = aria_bigdec_create("123.456");
    assert_str_eq(aria_bigdec_to_string(a).data, "123.456", "create 123.456");

    int32_t b = aria_bigdec_create("0.001");
    assert_str_eq(aria_bigdec_to_string(b).data, "0.001", "create 0.001");

    int32_t c = aria_bigdec_create("-42.5");
    assert_str_eq(aria_bigdec_to_string(c).data, "-42.5", "create -42.5");

    /* normalization */
    int32_t d = aria_bigdec_create("007.100");
    assert_str_eq(aria_bigdec_to_string(d).data, "7.1", "normalize 007.100 -> 7.1");

    int32_t e = aria_bigdec_create("-0");
    assert_str_eq(aria_bigdec_to_string(e).data, "0", "normalize -0 -> 0");

    /* compare */
    int32_t x10 = aria_bigdec_create("10");
    int32_t x20 = aria_bigdec_create("20");
    assert_int_eq(aria_bigdec_compare(x10, x20), -1, "10 < 20");
    assert_int_eq(aria_bigdec_compare(x20, x10), 1, "20 > 10");
    assert_int_eq(aria_bigdec_compare(x10, x10), 0, "10 == 10");

    /* equals */
    int32_t x10b = aria_bigdec_create("10.0");
    assert_int_eq(aria_bigdec_equals(x10, x10b), 1, "10 == 10.0");

    /* add: 123.456 + 0.001 = 123.457 */
    int32_t sum = aria_bigdec_add(a, b);
    assert_str_eq(aria_bigdec_to_string(sum).data, "123.457", "123.456 + 0.001 = 123.457");

    /* add: 10 + 20 = 30 */
    int32_t sum2 = aria_bigdec_add(x10, x20);
    assert_str_eq(aria_bigdec_to_string(sum2).data, "30", "10 + 20 = 30");

    /* add: negative + positive  => -42.5 + 123.456 = 80.956 */
    int32_t sum3 = aria_bigdec_add(c, a);
    assert_str_eq(aria_bigdec_to_string(sum3).data, "80.956", "-42.5 + 123.456 = 80.956");

    /* subtract: 20 - 10 = 10 */
    int32_t diff = aria_bigdec_subtract(x20, x10);
    assert_str_eq(aria_bigdec_to_string(diff).data, "10", "20 - 10 = 10");

    /* subtract: 10 - 20 = -10 */
    int32_t diff2 = aria_bigdec_subtract(x10, x20);
    assert_str_eq(aria_bigdec_to_string(diff2).data, "-10", "10 - 20 = -10");

    /* multiply: 10 * 20 = 200 */
    int32_t prod = aria_bigdec_multiply(x10, x20);
    assert_str_eq(aria_bigdec_to_string(prod).data, "200", "10 * 20 = 200");

    /* multiply: 1.5 * 2.5 = 3.75 */
    int32_t f = aria_bigdec_create("1.5");
    int32_t g = aria_bigdec_create("2.5");
    int32_t prod2 = aria_bigdec_multiply(f, g);
    assert_str_eq(aria_bigdec_to_string(prod2).data, "3.75", "1.5 * 2.5 = 3.75");

    /* multiply with negative */
    int32_t prod3 = aria_bigdec_multiply(c, x10);
    assert_str_eq(aria_bigdec_to_string(prod3).data, "-425", "-42.5 * 10 = -425");

    /* negate */
    int32_t neg = aria_bigdec_negate(x10);
    assert_str_eq(aria_bigdec_to_string(neg).data, "-10", "negate(10) = -10");
    int32_t neg2 = aria_bigdec_negate(c);
    assert_str_eq(aria_bigdec_to_string(neg2).data, "42.5", "negate(-42.5) = 42.5");

    /* is_zero */
    int32_t z = aria_bigdec_create("0");
    assert_int_eq(aria_bigdec_is_zero(z), 1, "0 is zero");
    assert_int_eq(aria_bigdec_is_zero(x10), 0, "10 is not zero");

    /* is_negative */
    assert_int_eq(aria_bigdec_is_negative(c), 1, "-42.5 is negative");
    assert_int_eq(aria_bigdec_is_negative(x10), 0, "10 is not negative");

    /* cleanup */
    aria_bigdec_destroy(a); aria_bigdec_destroy(b);
    aria_bigdec_destroy(c); aria_bigdec_destroy(d);
    aria_bigdec_destroy(e); aria_bigdec_destroy(x10);
    aria_bigdec_destroy(x20); aria_bigdec_destroy(x10b);
    aria_bigdec_destroy(sum); aria_bigdec_destroy(sum2);
    aria_bigdec_destroy(sum3); aria_bigdec_destroy(diff);
    aria_bigdec_destroy(diff2); aria_bigdec_destroy(prod);
    aria_bigdec_destroy(f); aria_bigdec_destroy(g);
    aria_bigdec_destroy(prod2); aria_bigdec_destroy(prod3);
    aria_bigdec_destroy(neg); aria_bigdec_destroy(neg2);
    aria_bigdec_destroy(z);

    printf("\nResults: %d passed, %d failed (of %d)\n", t_pass, t_fail, t_pass + t_fail);
    return t_fail;
}

#ifdef ARIA_BIGDEC_TEST
int main(void) { return aria_test_bigdec_run(); }
#endif
