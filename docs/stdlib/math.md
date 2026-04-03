# stdlib/math — Extended Mathematics

**Version:** v0.9.1  
**Backend:** aria-libc (`libaria_libc_math.so`)  
**Import:** `use "stdlib/math.aria".*;`

## Overview

Extends the compiler's built-in math functions with higher-level operations. All functions are accessed through the `Math` type.

## API Reference

### Floating-Point Classification

```aria
Math.is_nan(x: flt64) → bool       // true if x is NaN
Math.is_inf(x: flt64) → bool       // true if x is ±infinity
Math.is_finite(x: flt64) → bool    // true if x is neither NaN nor infinity
```

### Value Manipulation

```aria
Math.sign(x: flt64) → flt64        // returns -1.0, 0.0, or 1.0
Math.clamp(val, lo, hi: flt64) → flt64     // restrict to [lo, hi]
Math.clamp_i64(val, lo, hi: int64) → int64 // integer clamp
Math.wrap(value, max: flt64) → flt64       // wrap into [0, max)
```

### Interpolation

```aria
Math.lerp(a, b, t: flt64) → flt64           // a + t*(b-a)
Math.inverse_lerp(a, b, val: flt64) → flt64 // find t for lerp(a,b,t)==val
Math.map_range(val, in_min, in_max, out_min, out_max: flt64) → flt64
Math.smoothstep(edge0, edge1, x: flt64) → flt64     // Hermite [0,1]
Math.smootherstep(edge0, edge1, x: flt64) → flt64   // Perlin's variant
```

### Angle Conversion

```aria
Math.deg_to_rad(degrees: flt64) → flt64
Math.rad_to_deg(radians: flt64) → flt64
```

### Integer Math

```aria
Math.gcd(a, b: int64) → int64      // greatest common divisor
Math.lcm(a, b: int64) → int64      // least common multiple
Math.factorial(n: int64) → int64    // n! for n in [0..20]
```

### libm Wrappers

```aria
Math.fmod_f64(x, y: flt64) → flt64        // floating-point remainder
Math.hypot_f64(x, y: flt64) → flt64       // sqrt(x²+y²) without overflow
Math.copysign_f64(mag, sgn: flt64) → flt64 // magnitude with sign's sign bit
Math.fma_f64(x, y, z: flt64) → flt64      // fused multiply-add: x*y+z
```

## Example

```aria
use "stdlib/math.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    // Lerp between 0 and 100 at 25%
    flt64:val = raw Math.lerp(0.0, 100.0, 0.25);
    println(string_from_int(@cast<int64>(val)));  // 25

    // Clamp temperature to sensor range
    flt64:temp = raw Math.clamp(105.0, 0.0, 100.0);
    println(string_from_int(@cast<int64>(temp))); // 100

    // GCD
    int64:g = raw Math.gcd(48i64, 18i64);
    println(string_from_int(g));  // 6

    exit 0;
};
```

## Dependencies

Requires `libaria_libc_math.so` at runtime. Link with `-laria_libc_math -lm`.
