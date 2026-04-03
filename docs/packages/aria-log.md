# aria-log — Logging

**Version:** 0.2.2  
**Type:** aria-libc backed (aria_libc_io, aria_libc_time)  
**Ported:** v0.10.2

## API Reference

```aria
// Log levels
LOG_TRACE() → int32    // 0
LOG_DEBUG() → int32    // 1
LOG_INFO() → int32     // 2
LOG_WARN() → int32     // 3
LOG_ERROR() → int32    // 4
LOG_FATAL() → int32    // 5

// Lifecycle
log_init() → NIL                      // initialize logging system
log_cleanup() → NIL                   // cleanup resources

// Configuration
log_set_level(level: int32) → NIL     // set minimum log level
log_get_level() → int32               // get current log level
log_to_file(path: string) → NIL       // direct output to file
log_to_stderr() → NIL                 // direct output to stderr
log_to_stdout() → NIL                 // direct output to stdout
log_show_timestamp(show: int32) → NIL // toggle timestamp display (1=on)
log_show_level(show: int32) → NIL     // toggle level label display (1=on)
```

## Example

```aria
use "aria_log.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    drop log_init();
    drop log_set_level(raw LOG_INFO());
    drop log_to_stderr();
    drop log_show_timestamp(1i32);
    drop log_show_level(1i32);
    // Use with println for now — structured log_msg coming in future version
    drop log_cleanup();
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_io.so`, `libaria_libc_time.so` at runtime.
