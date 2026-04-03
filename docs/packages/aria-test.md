# aria-test — Test Framework

**Version:** 0.1.0  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
test_begin(suite: string) → NIL                                    // start a test suite
test_set_verbose(v: int32) → NIL                                   // set verbose mode (1=on)
test_end() → int32                                                 // end suite, returns 0 if all passed
test_passed_count() → int64                                        // number of passed tests
test_failed_count() → int64                                        // number of failed tests
test_skipped_count() → int64                                       // number of skipped tests
test_total_count() → int64                                         // total tests run
test_skip(name: string) → NIL                                      // skip a test
test_assert(name: string, cond: int32) → NIL                       // assert condition is true (non-zero)
test_assert_eq_i32(name: string, actual: int32, expected: int32) → NIL
test_assert_eq_i64(name: string, actual: int64, expected: int64) → NIL
test_assert_eq_str(name: string, actual: string, expected: string) → NIL
test_assert_near_f64(name: string, actual: flt64, expected: flt64, epsilon: flt64) → NIL
test_assert_not_empty(name: string, value: string) → NIL
test_assert_gt_i32(name: string, actual: int32, threshold: int32) → NIL
test_assert_gt_i64(name: string, actual: int64, threshold: int64) → NIL
test_assert_ne_i32(name: string, actual: int32, unexpected: int32) → NIL
```

## Example

```aria
use "aria_test.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    drop test_begin("my_suite");
    drop test_assert("truth", 1i32);
    drop test_assert_eq_i64("math", 4i64, 4i64);
    drop test_assert_eq_str("greeting", "hello", "hello");
    int32:rc = raw test_end();
    exit rc;
};
```

## Dependencies

None (pure Aria).
