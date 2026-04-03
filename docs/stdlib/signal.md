# signal — Signal Handling

**Backend:** aria-libc (aria_libc_process)  
**Ported:** v0.10.0

## API Reference

```aria
Signal.register(signo: int32) → int32   // register handler for signal (returns 0 on success)
Signal.pending(signo: int32) → int32    // check if signal was received (1=yes, clears flag)
Signal.ignore(signo: int32) → int32     // ignore a signal
Signal.restore(signo: int32) → int32    // restore default handler for signal
```

Signal constants are accessed via aria-libc shim functions:
- `aria_libc_process_SIGQUIT()` → int32
- `aria_libc_process_SIGUSR1()` → int32
- `aria_libc_process_SIGUSR2()` → int32

## Example

```aria
use "stdlib/signal.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:sig = raw aria_libc_process_SIGUSR1();
    int32:ok = raw Signal.register(sig);
    int32:p = raw Signal.pending(sig);  // 0 — no signal yet
    int32:ig = raw Signal.ignore(sig);
    int32:rs = raw Signal.restore(sig);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_process.so` at runtime.
