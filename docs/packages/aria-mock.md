# aria-mock — Test Mocking

**Version:** 0.3.2  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
mock_create(name: string) → int32                            // create mock, returns handle
mock_destroy(handle: int32) → NIL                            // destroy mock
mock_reset(handle: int32) → NIL                              // reset call history and return values
mock_get_name(handle: int32) → string                        // get mock name
mock_returns_int(handle: int32, val: int64) → NIL            // set int64 return value
mock_returns_str(handle: int32, val: string) → NIL           // set string return value
mock_call(handle: int32) → int64                             // invoke mock, returns configured int value
mock_call_with_int(handle: int32, arg: int64) → int64        // invoke with int64 argument
mock_call_with_str(handle: int32, arg: string) → int64       // invoke with string argument
mock_call_count(handle: int32) → int32                       // number of times mock was called
mock_call_arg_int(handle: int32, call_idx: int32, arg_idx: int32) → int64  // get int arg from call history
```

## Example

```aria
use "aria_mock.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:m = raw mock_create("my_func");
    drop mock_returns_int(m, 42i64);
    int64:result = raw mock_call(m);
    println(string_from_int(result));  // 42
    int32:count = raw mock_call_count(m);
    println(string_from_int(@cast<int64>(count)));  // 1
    drop mock_destroy(m);
    exit 0;
};
```

## Dependencies

None (pure Aria).
