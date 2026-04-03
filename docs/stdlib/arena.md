# arena — Arena Memory Allocator

**Backend:** aria-libc (aria_libc_mem)  
**Ported:** v0.10.0

## API Reference

```aria
Arena.create(capacity: int64) → int64                           // create arena with given byte capacity
Arena.destroy(arena: int64) → int32                             // free the arena
Arena.alloc(arena: int64, nbytes: int64) → int64                // allocate bytes, returns offset (-1 on failure)
Arena.reset(arena: int64) → int32                               // reset arena (free all allocations)
Arena.used(arena: int64) → int64                                // bytes currently allocated
Arena.remaining(arena: int64) → int64                           // bytes available
Arena.write_int64(arena: int64, offset: int64, value: int64) → int32  // write int64 at offset
Arena.read_int64(arena: int64, offset: int64) → int64                 // read int64 from offset
```

## Example

```aria
use "stdlib/arena.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:a = raw Arena.create(256i64);
    int64:off = raw Arena.alloc(a, 64i64);
    int32:ok = raw Arena.write_int64(a, off, 12345i64);
    int64:val = raw Arena.read_int64(a, off);
    println(string_from_int(val));  // 12345
    int32:r = raw Arena.reset(a);
    int32:d = raw Arena.destroy(a);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so` at runtime.
