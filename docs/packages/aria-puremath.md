# aria-puremath — Pure Aria Math (Taylor Series)

**Version:** 0.1.0  
**Type:** pure Aria (zero FFI)  
**Dependencies:** none

## Overview

Implements fundamental math functions entirely in Aria using Taylor series and numerical methods. Designed for freestanding/embedded platforms without libc. For hosted platforms, prefer compiler builtins or aria-libc_math (backed by hardware-optimized libm).

## API Reference

### Constants

```aria
puremath_pi() → flt64     // 3.14159265358979323846
puremath_e() → flt64      // 2.71828182845904523536
puremath_tau() → flt64    // 6.28318530717958647692
```

### Trigonometric (11-term Taylor series)

```aria
puremath_sin(x: flt64) → flt64   // sine with angle reduction to [-π, π]
puremath_cos(x: flt64) → flt64   // cosine with angle reduction
puremath_tan(x: flt64) → flt64   // sin/cos
```

### Exponential & Logarithmic

```aria
puremath_exp(x: flt64) → flt64   // e^x via range reduction + 16-term Taylor
puremath_ln(x: flt64) → flt64    // natural log via 30-term series on [1,2)
```

### Power & Roots

```aria
puremath_sqrt(x: flt64) → flt64              // Newton's method (1e-15 tolerance)
puremath_pow_int(base: flt64, exp: int64) → flt64  // exponentiation by squaring
puremath_pow(base, exp: flt64) → flt64       // general: exp(b*ln(a))
```

### Rounding & Absolute Value

```aria
puremath_abs(x: flt64) → flt64    // absolute value
puremath_floor(x: flt64) → flt64  // floor via int64 cast
puremath_ceil(x: flt64) → flt64   // ceil
```

## Performance (vs libm)

| Function | libm (via shim) | Pure Aria | Notes |
|----------|----------------|-----------|-------|
| sin | 29 ns | 9 ns | Pure wins (less overhead) |
| cos | 23 ns | 8 ns | Pure wins |
| exp | 25 ns | 177 ns | libm 7× faster |
| ln | 10 ns | 123 ns | libm 12× faster |
| sqrt | 5 ns | 22 ns | libm 4× faster (hw instr) |

## Example

```aria
use "aria_puremath.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    flt64:pi = puremath_pi();
    flt64:s = raw puremath_sin(pi / 6.0);   // ~0.5
    flt64:c = raw puremath_cos(0.0);         // 1.0
    flt64:r = raw puremath_sqrt(2.0);        // ~1.41421

    // Pythagorean identity: sin²+cos² ≈ 1.0
    flt64:angle = 0.7;
    flt64:sv = raw puremath_sin(angle);
    flt64:cv = raw puremath_cos(angle);
    flt64:identity = sv * sv + cv * cv;
    // identity ≈ 1.0 (within 1e-12)

    exit 0;
};
```

## Accuracy

All functions pass 31 validation tests covering:
- Standard angles (0, π/6, π/4, π/3, π/2, π)
- Special values (0, 1, negative inputs)
- Pythagorean identity verification
- Cross-validation against known constants
- Tolerance: ≤ 1e-6 for trig, ≤ 1e-4 for exp/ln
