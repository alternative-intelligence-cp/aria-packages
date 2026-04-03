/*
 * aria_libc_math — Math operations for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_math.so aria_libc_math.c -lm
 */
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>

/* ── Trigonometric ───────────────────────────────────────────────── */

double aria_libc_math_sin(double x)   { return sin(x); }
double aria_libc_math_cos(double x)   { return cos(x); }
double aria_libc_math_tan(double x)   { return tan(x); }
double aria_libc_math_asin(double x)  { return asin(x); }
double aria_libc_math_acos(double x)  { return acos(x); }
double aria_libc_math_atan(double x)  { return atan(x); }
double aria_libc_math_atan2(double y, double x) { return atan2(y, x); }

/* ── Hyperbolic ──────────────────────────────────────────────────── */

double aria_libc_math_sinh(double x)  { return sinh(x); }
double aria_libc_math_cosh(double x)  { return cosh(x); }
double aria_libc_math_tanh(double x)  { return tanh(x); }

/* ── Exponential / Logarithmic ───────────────────────────────────── */

double aria_libc_math_exp(double x)   { return exp(x); }
double aria_libc_math_exp2(double x)  { return exp2(x); }
double aria_libc_math_log(double x)   { return log(x); }
double aria_libc_math_log2(double x)  { return log2(x); }
double aria_libc_math_log10(double x) { return log10(x); }

/* ── Power / Root ────────────────────────────────────────────────── */

double aria_libc_math_pow(double base, double exp_val) { return pow(base, exp_val); }
double aria_libc_math_sqrt(double x)  { return sqrt(x); }
double aria_libc_math_cbrt(double x)  { return cbrt(x); }
double aria_libc_math_hypot(double x, double y) { return hypot(x, y); }

/* ── Rounding ────────────────────────────────────────────────────── */

double aria_libc_math_floor(double x)  { return floor(x); }
double aria_libc_math_ceil(double x)   { return ceil(x); }
double aria_libc_math_round(double x)  { return round(x); }
double aria_libc_math_trunc(double x)  { return trunc(x); }
double aria_libc_math_fmod(double x, double y) { return fmod(x, y); }

/* ── Absolute / Sign ─────────────────────────────────────────────── */

double aria_libc_math_fabs(double x)  { return fabs(x); }
double aria_libc_math_copysign(double mag, double sgn) { return copysign(mag, sgn); }
double aria_libc_math_fma(double a, double b, double c) { return fma(a, b, c); }

/* ── Min / Max ───────────────────────────────────────────────────── */

double aria_libc_math_fmin(double a, double b) { return fmin(a, b); }
double aria_libc_math_fmax(double a, double b) { return fmax(a, b); }

int64_t aria_libc_math_min_i64(int64_t a, int64_t b) { return a < b ? a : b; }
int64_t aria_libc_math_max_i64(int64_t a, int64_t b) { return a > b ? a : b; }

int64_t aria_libc_math_abs_i64(int64_t x) { return x < 0 ? -x : x; }

/* ── Classification ──────────────────────────────────────────────── */

int32_t aria_libc_math_isnan(double x)      { return isnan(x) ? 1 : 0; }
int32_t aria_libc_math_isinf(double x)      { return isinf(x) ? 1 : 0; }
int32_t aria_libc_math_isfinite(double x)   { return isfinite(x) ? 1 : 0; }

/* ── Constants ───────────────────────────────────────────────────── */

double aria_libc_math_pi(void)      { return M_PI; }
double aria_libc_math_e(void)       { return M_E; }
double aria_libc_math_inf(void)     { return INFINITY; }
double aria_libc_math_nan(void)     { return NAN; }
double aria_libc_math_epsilon(void) { return DBL_EPSILON; }

/* ── Bit Casting (for binary serialization) ──────────────────────── */

int64_t aria_libc_math_flt64_bits(double val) {
    int64_t bits;
    __builtin_memcpy(&bits, &val, sizeof(bits));
    return bits;
}

double aria_libc_math_flt64_from_bits(int64_t bits) {
    double val;
    __builtin_memcpy(&val, &bits, sizeof(val));
    return val;
}

int32_t aria_libc_math_flt32_bits(double val) {
    float f = (float)val;
    int32_t bits;
    __builtin_memcpy(&bits, &f, sizeof(bits));
    return bits;
}

double aria_libc_math_flt32_from_bits(int32_t bits) {
    float f;
    __builtin_memcpy(&f, &bits, sizeof(f));
    return (double)f;
}
