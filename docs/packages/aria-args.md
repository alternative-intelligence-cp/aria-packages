# aria-args — CLI Argument Parsing

**Version:** 0.2.0  
**Type:** aria-libc backed (aria_libc_string)  
**Ported:** v0.10.2

## API Reference

```aria
args_has(args: string, flag: string) → bool            // check if flag exists
args_get(args: string, key: string) → string           // get value for --key=value
args_get_or(args: string, key: string, default_val: string) → string  // get with default
args_at(args: string, n: int32) → string               // get positional argument by index
args_program(args: string) → string                    // get program name (arg 0)
args_count(args: string) → int32                       // total argument count
args_make1(a0: string) → string                        // build args string from 1 arg
args_make2(a0: string, a1: string) → string            // build from 2 args
args_make3(a0: string, a1: string, a2: string) → string
args_make4(a0: string, a1: string, a2: string, a3: string) → string
args_make5(a0: string, a1: string, a2: string, a3: string, a4: string) → string
args_make6(a0: string, a1: string, a2: string, a3: string, a4: string, a5: string) → string
args_make8(a0: string, a1: string, a2: string, a3: string, a4: string, a5: string, a6: string, a7: string) → string
```

## Example

```aria
use "aria_args.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:a = raw args_make3("prog", "--verbose", "--output=file.txt");
    tbb32:has_v = raw args_has(a, "--verbose");
    string:out = raw args_get(a, "output");
    println(out);  // file.txt
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so` at runtime.
