# stdlib/string — String Manipulation

**Version:** v0.9.1  
**Backend:** aria-libc (`libaria_libc_string.so`, `libaria_libc_mem.so`)  
**Import:** `use "stdlib/string.aria".*;`

## Overview

Extends the compiler's built-in string functions with operations that require byte-level access or buffer management. Split operations use opaque handles for memory safety.

## API Reference

### Search & Replace

```aria
str_replace(src, old_s, new_s: string) → string       // replace all occurrences
str_replace_first(src, old_s, new_s: string) → string  // replace first only
str_count(haystack, needle: string) → int64            // count occurrences
```

### Character Access

```aria
str_byte_at(s: string, idx: int64) → int64   // byte value at index (-1 if OOB)
str_char_at(s: string, idx: int64) → string  // single-character string at index
str_reverse(s: string) → string              // reverse bytes (ASCII-safe)
```

### Parsing

```aria
str_to_int(s: string) → int64  // parse string to int64
```

### Split & Join

Split returns an opaque handle that must be freed after use:

```aria
str_split(s, delim: string) → int64        // split by delimiter → handle
str_split_count(handle: int64) → int64     // number of parts
str_split_get(handle: int64, i: int64) → string  // get i-th part (0-based)
str_split_free(handle: int64) → NIL        // free split handle
str_join(handle: int64, delim: string) → string  // join parts with delimiter
```

## Example: Replace and Count

```aria
use "stdlib/string.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:s = "hello world hello aria";

    // Count occurrences
    int64:n = raw str_count(s, "hello");
    println(string_from_int(n));  // 2

    // Replace all
    string:r = raw str_replace(s, "hello", "hi");
    println(r);  // "hi world hi aria"

    exit 0;
};
```

## Example: Split and Join

```aria
use "stdlib/string.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:csv = "alpha,beta,gamma,delta";

    int64:parts = raw str_split(csv, ",");
    int64:count = raw str_split_count(parts);
    println(string_from_int(count));  // 4

    // Access individual parts
    int64:i = 0i64;
    while (i < count) {
        string:p = raw str_split_get(parts, i);
        println(p);
        i = i + 1i64;
    }

    // Rejoin with different delimiter
    string:joined = raw str_join(parts, " | ");
    println(joined);  // "alpha | beta | gamma | delta"

    drop str_split_free(parts);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so` and `libaria_libc_mem.so` at runtime. Link with `-laria_libc_string -laria_libc_mem`.
