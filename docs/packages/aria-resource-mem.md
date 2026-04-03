# aria-resource-mem — Memory Resource Management

**Version:** 0.2.0  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
// Status codes
rm_live() → int64           // resource is live
rm_write_ok() → int64       // write succeeded
rm_exp_reads() → int64      // expired: max reads reached
rm_exp_writes() → int64     // expired: max writes reached
rm_exp_ttl() → int64        // expired: TTL elapsed
rm_no_limit() → int64       // sentinel for unlimited reads/writes/TTL

// Resource lifecycle
rm_create(cell: wild int64->, value: int64, max_reads: int64, max_writes: int64, ttl_ticks: int64, ttl_reset_on_write: int64) → NIL
rm_read(cell: wild int64->, result: wild int64->) → NIL
rm_write(cell: wild int64->, new_value: int64, result: wild int64->) → NIL

// Inspection
rm_reads_left(cell: wild int64->) → int64
rm_writes_left(cell: wild int64->) → int64
rm_ttl_left(cell: wild int64->) → int64
rm_check_expiry(cell: wild int64->) → int64    // returns status code

// Result helpers
rm_fill_ok(result: wild int64->, cell: wild int64->, status_code: int64) → NIL
rm_fill_err(result: wild int64->, cell: wild int64->, status_code: int64) → NIL
```

## Dependencies

None (pure Aria).
