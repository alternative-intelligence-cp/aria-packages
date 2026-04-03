# aria-datetime — Date/Time

**Version:** 0.2.2  
**Type:** aria-libc backed (aria_libc_time)  
**Ported:** v0.10.2

## API Reference

```aria
// Current time
dt_now() → int64              // Unix epoch seconds
dt_now_ms() → int64           // milliseconds since epoch
dt_now_us() → int64           // microseconds since epoch
dt_monotonic_ms() → int64     // monotonic clock (milliseconds)

// Local time components
dt_year(epoch: int64) → int32
dt_month(epoch: int64) → int32
dt_day(epoch: int64) → int32
dt_hour(epoch: int64) → int32
dt_minute(epoch: int64) → int32
dt_second(epoch: int64) → int32
dt_weekday(epoch: int64) → int32
dt_yearday(epoch: int64) → int32

// UTC time components
dt_utc_year(epoch: int64) → int32
dt_utc_month(epoch: int64) → int32
dt_utc_day(epoch: int64) → int32
dt_utc_hour(epoch: int64) → int32
dt_utc_minute(epoch: int64) → int32
dt_utc_second(epoch: int64) → int32

// Formatting
dt_format(epoch: int64, fmt: string) → string       // strftime-style format (local)
dt_format_utc(epoch: int64, fmt: string) → string   // strftime-style format (UTC)
dt_iso8601(epoch: int64) → string                    // ISO 8601 format

// Construction & arithmetic
dt_make(yr: int32, mo: int32, dy: int32, hr: int32, mn: int32, sc: int32) → int64
dt_make_utc(yr: int32, mo: int32, dy: int32, hr: int32, mn: int32, sc: int32) → int64
dt_diff(a: int64, b: int64) → int64                 // difference in seconds
dt_add_seconds(epoch: int64, seconds: int64) → int64
dt_add_days(epoch: int64, days: int32) → int64

// Utility
dt_sleep_ms(ms: int32) → NIL                        // sleep for milliseconds
```

## Example

```aria
use "aria_datetime.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:now = raw dt_now();
    int32:year = raw dt_year(now);
    println(string_from_int(@cast<int64>(year)));
    string:iso = raw dt_iso8601(now);
    println(iso);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_time.so` at runtime.
