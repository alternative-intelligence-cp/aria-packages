# aria-sort — Sorting Algorithms

**Version:** 0.2.0  
**Type:** pure Aria (uses aria-libc_mem for buffer management)  
**Ported:** v0.9.2

## API Reference

```aria
sort_quicksort(buf: int64, len: int64) → NIL          // in-place quicksort
sort_insertion(buf: int64, len: int64) → NIL           // in-place insertion sort
sort_is_sorted(buf: int64, len: int64) → int32         // 1 if sorted ascending
sort_reverse(buf: int64, len: int64) → NIL             // reverse array in-place
```

All functions operate on `int64` arrays stored in raw memory buffers (allocated via `aria_libc_mem_malloc`). Elements are accessed at 8-byte stride.

## Example

```aria
use "aria_sort.aria".*;

extern "aria_libc_mem" {
    func:aria_libc_mem_malloc    = int64(int64:size);
    func:aria_libc_mem_free      = void(int64:ptr);
    func:aria_libc_mem_write_i64 = void(int64:ptr, int64:offset, int64:val);
    func:aria_libc_mem_read_i64  = int64(int64:ptr, int64:offset);
}

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:n = 5i64;
    int64:buf = aria_libc_mem_malloc(n * 8i64);

    // Fill: [5, 3, 1, 4, 2]
    drop aria_libc_mem_write_i64(buf, 0i64, 5i64);
    drop aria_libc_mem_write_i64(buf, 8i64, 3i64);
    drop aria_libc_mem_write_i64(buf, 16i64, 1i64);
    drop aria_libc_mem_write_i64(buf, 24i64, 4i64);
    drop aria_libc_mem_write_i64(buf, 32i64, 2i64);

    // Sort
    drop sort_quicksort(buf, n);

    // Verify
    int32:sorted = raw sort_is_sorted(buf, n);
    // sorted == 1

    // Read: [1, 2, 3, 4, 5]
    int64:first = aria_libc_mem_read_i64(buf, 0i64);   // 1
    int64:last  = aria_libc_mem_read_i64(buf, 32i64);  // 5

    drop aria_libc_mem_free(buf);
    exit 0;
};
```

## Performance

| Algorithm | N=100 | N=1k |
|-----------|-------|------|
| Quicksort | 189 μs | 13,595 μs |
| Insertion | 3 μs | 23 μs |

**Note:** Current quicksort uses naive first-element pivot, causing O(n²) on certain inputs. Median-of-three and hybrid sort optimizations planned.

## Dependencies

Requires `libaria_libc_mem.so` at runtime.
