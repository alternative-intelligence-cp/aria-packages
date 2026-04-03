# aria-rate-limit — Rate Limiting

**Version:** 0.2.9  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
rate_limit_configure(max_tokens: int32, refill_per_sec: int32, window_sec: int32) → NIL
rate_limit_max(max_tokens: int32) → NIL                // set max tokens
rate_limit_rate(tokens_per_sec: int32) → NIL           // set refill rate
rate_limit_check(key: string) → int32                  // check if request allowed (1=yes, 0=rate limited)
rate_limit_check_cost(key: string, cost: int32) → int32 // check with custom cost
rate_limit_remaining(key: string) → int32              // tokens remaining for key
rate_limit_retry_after(key: string) → int32            // seconds until next token available
rate_limit_headers(key: string) → string               // HTTP-style rate limit headers
rate_limit_reset() → NIL                               // reset all state
rate_limit_test_set_tokens(key: string, tokens: int32) → NIL // test helper: set token count
```

## Example

```aria
use "aria_rate_limit.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    drop rate_limit_configure(10i32, 1i32, 60i32);
    int32:ok = raw rate_limit_check("user:123");  // 1 — allowed
    int32:rem = raw rate_limit_remaining("user:123");
    println(string_from_int(@cast<int64>(rem)));  // 9
    drop rate_limit_reset();
    exit 0;
};
```

## Dependencies

None (pure Aria).
