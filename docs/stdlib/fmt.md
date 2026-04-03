# stdlib/fmt — String Formatting

**Version:** v0.9.1  
**Backend:** pure Aria (zero FFI)  
**Import:** `use "stdlib/fmt.aria".*;`

## Overview

Printf-style string formatting using `{}` placeholders. Pure Aria implementation — works on any platform without external dependencies.

## API Reference

### Format Functions

```aria
fmt(template: string, arg1: string) → string       // single substitution
fmt2(template: string, a1, a2: string) → string     // two substitutions
fmt3(template: string, a1, a2, a3: string) → string // three substitutions
fmt4(template: string, a1, a2, a3, a4: string) → string
```

Placeholders `{0}`, `{1}`, `{2}`, `{3}` are replaced with the corresponding argument.

### Conversion Functions

```aria
fmt_int(n: int64) → string              // integer to string
fmt_float(x: flt64, decimals: int64) → string  // float with precision
fmt_bool(b: bool) → string              // "true" or "false"
fmt_hex(n: int64) → string              // integer to hex string
```

### Padding

```aria
fmt_pad_left(s: string, width: int64, pad: string) → string
fmt_pad_right(s: string, width: int64, pad: string) → string
fmt_repeat(s: string, n: int64) → string
```

## Example

```aria
use "stdlib/fmt.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    // Simple substitution
    string:msg = raw fmt("Hello, {0}!", "Aria");
    println(msg);  // "Hello, Aria!"

    // Multiple arguments
    string:info = raw fmt3("{0} is version {1} on {2}",
        "AriaX", "0.9.3", "Linux");
    println(info);  // "AriaX is version 0.9.3 on Linux"

    // Number formatting
    string:n = raw fmt_int(42i64);
    string:f = raw fmt_float(3.14159, 2i64);
    string:h = raw fmt_hex(255i64);
    println(n);  // "42"
    println(f);  // "3.14"
    println(h);  // "ff"

    // Padding
    string:padded = raw fmt_pad_left("42", 8i64, "0");
    println(padded);  // "00000042"

    exit 0;
};
```

## Dependencies

None — pure Aria, zero runtime libraries required.
