# aria-bench — Micro-Benchmarking

**Version:** 0.2.0  
**Type:** pure Aria (uses aria-libc_time for monotonic clock)  
**Ported:** v0.9.2

## API Reference

```aria
bench_start() → int64                    // capture start time (ns)
bench_end(start: int64) → int64          // elapsed nanoseconds since start
bench_run(iters: int64) → int64          // start timer for N iterations
bench_report(label: string, elapsed: int64, iters: int64) → NIL  // print results
```

## Example

```aria
use "aria_bench.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:iters = 100000i64;
    int64:t0 = raw bench_start();

    int64:i = 0i64;
    int64:sink = 0i64;
    while (i < iters) {
        sink = sink + i * i;
        i = i + 1i64;
    }

    int64:elapsed = raw bench_end(t0);
    drop bench_report("square_sum", elapsed, iters);
    // Output: "square_sum: 3 ns/op (100000 iterations)"
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_time.so` at runtime.
