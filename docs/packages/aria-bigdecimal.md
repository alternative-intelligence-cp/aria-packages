# aria-bigdecimal — Arbitrary Precision Decimals

**Version:** 0.3.2  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
bigdec_create(value: string) → int32              // create from string (e.g. "3.14"), returns handle
bigdec_destroy(handle: int32) → NIL               // free a bigdecimal
bigdec_to_string(handle: int32) → string           // convert to string representation
bigdec_compare(a: int32, b: int32) → int32         // -1, 0, or 1
bigdec_equals(a: int32, b: int32) → int32          // 1 if equal
bigdec_add(a: int32, b: int32) → int32             // a + b, returns new handle
bigdec_negate(a: int32) → int32                    // -a, returns new handle
bigdec_subtract(a: int32, b: int32) → int32        // a - b, returns new handle
bigdec_multiply(a: int32, b: int32) → int32        // a * b, returns new handle
bigdec_is_zero(handle: int32) → int32              // 1 if zero
bigdec_is_negative(handle: int32) → int32          // 1 if negative
```

## Example

```aria
use "aria_bigdecimal.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:a = raw bigdec_create("123.456");
    int32:b = raw bigdec_create("0.544");
    int32:sum = raw bigdec_add(a, b);
    println(raw bigdec_to_string(sum));  // 124.000
    drop bigdec_destroy(a);
    drop bigdec_destroy(b);
    drop bigdec_destroy(sum);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_string.so` at runtime.
