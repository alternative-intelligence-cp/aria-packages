# Aria v0.9.3 — Performance Benchmark Report

**Date:** June 2025  
**Platform:** Linux x86_64, RTX 3090, GCC toolchain  
**Compiler:** ariac (dev branch)  
**Iterations:** 100k unless noted

---

## 1. Math: Direct libm vs aria-libc Shim

Measures overhead of the aria-libc `.so` shim layer vs direct `extern func` to libm.

| Function | Direct libm | aria-libc shim | Overhead |
|----------|------------|----------------|----------|
| sin      | 8 ns/op    | 28 ns/op       | 3.5×     |
| cos      | 8 ns/op    | 23 ns/op       | 2.9×     |
| sqrt     | 6 ns/op    | 9 ns/op        | 1.5×     |
| exp      | 7 ns/op    | 26 ns/op       | 3.7×     |
| log      | 6 ns/op    | 18 ns/op       | 3.0×     |
| pow      | 6 ns/op    | 26 ns/op       | 4.3×     |
| floor    | 4 ns/op    | 7 ns/op        | 1.8×     |

**Analysis:** The aria-libc shim adds 1.5–4.3× overhead due to shared library indirection. For hot math loops, this is measurable but still sub-30ns per call. The `sqrt` and `floor` paths have minimal overhead since they map nearly 1:1 to hardware instructions. Trig/exp/pow show higher overhead from PLT trampoline + wrapper function calls.

**Verdict:** Acceptable for general use. Hot inner loops should use compiler builtins (`sin`, `cos`, `sqrt` etc.) which inline directly.

---

## 2. Pure Aria Math (Taylor Series) vs libm

Compares the `aria-puremath` package (pure Aria, zero FFI) against `aria-libc_math` (backed by glibc libm).

| Function | libm (shim) | Pure Aria  | Ratio     |
|----------|-------------|------------|-----------|
| sin      | 29 ns/op    | 9 ns/op    | 0.3× ✓   |
| cos      | 23 ns/op    | 8 ns/op    | 0.3× ✓   |
| exp      | 25 ns/op    | 177 ns/op  | 7.1×      |
| ln       | 10 ns/op    | 123 ns/op  | 12.3×     |
| sqrt     | 5 ns/op     | 22 ns/op   | 4.4×      |

**Analysis:**
- **sin/cos (pure Aria wins):** The 11-term Taylor series with angle reduction is faster than the PLT+shim+libm path. This is because the series converges quickly for reduced angles, and the function call overhead of going through `.so` dominates for libm.
- **exp (libm wins ~7×):** libm uses hardware-assisted range reduction and lookup tables. Our 16-term Taylor series is accurate but slower.
- **ln (libm wins ~12×):** Our 30-term ln(1+t) series converges slowly. libm uses polynomial approximations tuned for x87/SSE.
- **sqrt (libm wins ~4×):** libm maps to the `fsqrt`/`sqrtsd` hardware instruction. Newton's method needs multiple iterations.

**Verdict:** `aria-puremath` is viable for embedded/freestanding targets. For hosted platforms with libm, the compiler builtins remain preferred for exp/ln/sqrt. Sin/cos can use either path.

---

## 3. Hex Encode/Decode (Pure Aria)

Measures throughput of pure Aria hex operations (1k iterations).

| Operation             | Time      |
|-----------------------|-----------|
| encode short (12 B)   | 4,773 ns  |
| decode short (24 hex) | 4,212 ns  |
| encode medium (54 B)  | 12,234 ns |
| decode medium (108 hex)| 5,305 ns |
| roundtrip short       | 2,569 ns  |

**Analysis:** Encode scales linearly with input size (~88 ns/byte for short, ~226 ns/byte for medium — the medium cost is higher due to string_concat pressure). Decode is faster because it processes 2 hex chars → 1 byte.

**Note:** Iteration counts reduced from 10k to 1k due to string allocation pressure in the current runtime (no GC). This is a known limitation tracked for future optimization.

---

## 4. Sort (Pure Aria)

Measures pure Aria quicksort and insertion sort at varying sizes.

| Algorithm       | N=100  | N=1k     | N=10k     | N=50k       |
|-----------------|--------|----------|-----------|-------------|
| Quicksort       | 189 μs | 13,595 μs| 548,798 μs| 13,081,194 μs|
| Insertion sort  | 3 μs   | 23 μs    | skipped   | skipped     |

**Analysis:**
- **Insertion sort** is extremely fast for small inputs (3 μs for 100 elements) thanks to simple loop + memory locality.
- **Quicksort** shows O(n²) degradation. The current implementation uses a naive first-element pivot that degrades on LCG-generated data. At N=100 it's already 63× slower than insertion sort.

**Action items:**
- [ ] Implement median-of-three pivot selection in aria-sort
- [ ] Add hybrid sort (quicksort + insertion sort fallback for N<32)
- [ ] Consider introsort (switch to heapsort on recursion depth limit)

---

## Summary

| Category | Status | Notes |
|----------|--------|-------|
| aria-libc shim overhead | ✅ Acceptable | 1.5–4.3× vs direct; sub-30ns |
| Pure Aria math (sin/cos) | ✅ Competitive | Faster than shim path |
| Pure Aria math (exp/ln/sqrt) | ⚠️ Slower | 4–12× vs libm; acceptable for embedded |
| Hex encode/decode | ✅ Working | String alloc pressure limits iteration |
| Quicksort | ❌ Needs work | O(n²) with naive pivot |
| Insertion sort | ✅ Fast | Best for small arrays |
