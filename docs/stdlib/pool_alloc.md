# pool_alloc — Pool Memory Allocator

**Backend:** aria-libc (aria_libc_mem)  
**Ported:** v0.10.0

## API Reference

```aria
PoolAlloc.create(block_size: int64, block_count: int64) → int64   // create pool (returns handle)
PoolAlloc.destroy(pool: int64) → int32                             // free the pool
PoolAlloc.get(pool: int64) → int64                                 // get a free block index (-1 if exhausted)
PoolAlloc.put(pool: int64, block_index: int64) → int32             // return block to pool
PoolAlloc.write_int64(pool: int64, block_index: int64, offset: int64, value: int64) → int32  // write int64
PoolAlloc.read_int64(pool: int64, block_index: int64, offset: int64) → int64                 // read int64
PoolAlloc.available(pool: int64) → int64                           // number of free blocks
```

## Example

```aria
use "stdlib/pool_alloc.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:pool = raw PoolAlloc.create(64i64, 4i64);
    int64:blk = raw PoolAlloc.get(pool);
    int32:ok = raw PoolAlloc.write_int64(pool, blk, 0i64, 999i64);
    int64:val = raw PoolAlloc.read_int64(pool, blk, 0i64);
    println(string_from_int(val));  // 999
    int32:p = raw PoolAlloc.put(pool, blk);
    int32:d = raw PoolAlloc.destroy(pool);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so` at runtime.
