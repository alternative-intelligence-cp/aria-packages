# stdlib/mem — Memory Allocation

**Version:** v0.9.1  
**Backend:** aria-libc (`libaria_libc_mem.so`)  
**Import:** `use "stdlib/mem.aria".*;`

## Overview

Provides direct memory allocation and byte-level access through aria-libc shims. All pointers are represented as `int64` handles for ABI safety. This module is the foundation for buffer-based data structures.

## API Reference

### Allocation

```aria
aria_libc_mem_malloc(size: int64) → int64       // allocate size bytes → handle
aria_libc_mem_calloc(count, size: int64) → int64 // allocate zeroed memory
aria_libc_mem_realloc(ptr, size: int64) → int64  // resize allocation
aria_libc_mem_free(ptr: int64) → void            // free allocation
```

### Bulk Operations

```aria
aria_libc_mem_copy(dst, src, n: int64) → void  // memcpy (non-overlapping)
aria_libc_mem_move(dst, src, n: int64) → void  // memmove (safe for overlap)
aria_libc_mem_zero(ptr, n: int64) → void       // memset to 0
```

### Byte Access

```aria
aria_libc_mem_read_byte(ptr, offset: int64) → int32
aria_libc_mem_write_byte(ptr, offset: int64, val: int32) → void
```

### Word Access

```aria
aria_libc_mem_read_i32(ptr, offset: int64) → int32
aria_libc_mem_write_i32(ptr, offset: int64, val: int32) → void
aria_libc_mem_read_i64(ptr, offset: int64) → int64
aria_libc_mem_write_i64(ptr, offset: int64, val: int64) → void
```

## Example: Dynamic Buffer

```aria
use "stdlib/mem.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    // Allocate 256 bytes
    int64:buf = aria_libc_mem_malloc(256i64);

    // Write bytes
    drop aria_libc_mem_write_byte(buf, 0i64, 72i32);   // 'H'
    drop aria_libc_mem_write_byte(buf, 1i64, 105i32);  // 'i'

    // Read back
    int32:b0 = aria_libc_mem_read_byte(buf, 0i64);
    println(string_from_int(@cast<int64>(b0)));  // 72

    // Write int64 at offset 8
    drop aria_libc_mem_write_i64(buf, 8i64, 42i64);
    int64:val = aria_libc_mem_read_i64(buf, 8i64);
    println(string_from_int(val));  // 42

    // Free
    drop aria_libc_mem_free(buf);
    exit 0;
};
```

## Notes

- All `ptr` values are `int64` representing raw memory addresses. Zero indicates allocation failure.
- Offsets are in **bytes**, not elements. For `i64` arrays, multiply index by 8.
- `aria_libc_mem_copy` is `memcpy` — undefined behavior if regions overlap. Use `aria_libc_mem_move` for overlapping regions.

## Dependencies

Requires `libaria_libc_mem.so` at runtime. Link with `-laria_libc_mem`.
