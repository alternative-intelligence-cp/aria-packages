# aria-math — Math Operations

**Version:** 0.2.0  
**Type:** pure Aria (uses aria-libc for byte access only)  
**Ported:** v0.9.2 (rewritten from C shim to pure Aria)

## API Reference

```aria
math_factorial(n: int64) → int64          // n! (iterative)
math_fibonacci(n: int64) → int64          // nth fibonacci number
math_gcd(a, b: int64) → int64            // greatest common divisor
math_lcm(a, b: int64) → int64            // least common multiple
math_is_prime(n: int64) → int32          // 1 if prime, 0 otherwise
math_power(base, exp: int64) → int64     // integer exponentiation
math_abs_i64(n: int64) → int64           // absolute value (int64)
math_max_i64(a, b: int64) → int64        // maximum
math_min_i64(a, b: int64) → int64        // minimum
math_clamp_i64(v, lo, hi: int64) → int64 // clamp to range
math_sum_range(start, end: int64) → int64 // sum of [start..end]
math_digit_count(n: int64) → int64       // number of digits
math_is_even(n: int64) → int32           // 1 if even
math_is_odd(n: int64) → int32            // 1 if odd
math_combinations(n, r: int64) → int64   // C(n,r)
```

## Example

```aria
use "aria_math.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    println(string_from_int(raw math_factorial(10i64)));  // 3628800
    println(string_from_int(raw math_fibonacci(10i64)));  // 55
    println(string_from_int(raw math_gcd(48i64, 18i64))); // 6
    println(string_from_int(raw math_combinations(10i64, 3i64))); // 120
    exit 0;
};
```
