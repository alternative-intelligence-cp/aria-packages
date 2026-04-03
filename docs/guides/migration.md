# FFI Migration Guide

## Overview

As of v0.9.1, Aria is migrating stdlib modules and packages from direct `extern func` calls to **aria-libc shims**. This provides:

- **ABI safety:** All pointers passed as `int64` handles, avoiding codegen bugs
- **Portability:** Single C bridge layer that can be retargeted per platform
- **Maintainability:** One source of truth for C bindings
- **Testability:** Shim functions can be mocked or instrumented

## Migration Status

### Stdlib (4 of 42 migrated)

| Module | Status | Version |
|--------|--------|---------|
| `math.aria` | ✅ Migrated | v0.9.1 |
| `io.aria` | ✅ Migrated | v0.9.1 |
| `string.aria` | ✅ Migrated | v0.9.1 |
| `mem.aria` | ✅ Migrated | v0.9.1 |
| `fmt.aria` | ✅ Pure Aria | v0.9.1 |
| 37 others | ⚠️ Deprecated | Uses direct extern |

### Packages (7 ported to pure Aria)

| Package | Status | Version |
|---------|--------|---------|
| `aria-math` | ✅ Pure Aria | v0.2.0 |
| `aria-bench` | ✅ Pure Aria | v0.2.0 |
| `aria-path` | ✅ Pure Aria | v0.2.0 |
| `aria-env` | ✅ Pure Aria | v0.2.0 |
| `aria-hex` | ✅ Pure Aria | v0.2.0 |
| `aria-sort` | ✅ Pure Aria | v0.2.0 |
| `aria-regex` | ✅ Pure Aria | v0.2.0 |
| `aria-puremath` | ✅ Pure Aria (new) | v0.1.0 |

## For Package Authors: How to Migrate

### Before (direct extern)

```aria
extern func:my_c_strlen = int64(string:s);    // inline extern — returns Result<T>
extern {
    func:my_c_strlen = int64(string:s);        // block extern — returns T directly
}
```

### After (aria-libc shim)

1. Add your C function to the appropriate aria-libc domain library
2. Declare via block extern with the `aria_libc_*` naming convention:

```aria
extern "aria_libc_string" {
    func:aria_libc_string_byte_at = int32(string:s, int64:idx);
}
```

3. Call directly (block extern returns unwrapped values):

```aria
int32:byte = aria_libc_string_byte_at(mystr, 0i64);
```

### Key Rules

- String params → `const char *` at C ABI (Aria `string` maps to this)
- String returns → `AriaString {char*, int64_t}` by value
- Pointers → always `int64` (never use Aria pointer types in extern)
- Void returns → use `drop` keyword: `drop my_func(args);`
- Name collisions → avoid `free` in extern names (use `_release` or `_destroy`)

## Alternative: Pure Aria

For packages that don't **need** C (math, hex, sorting, etc.), consider rewriting to pure Aria using aria-libc only for low-level byte access. Benefits:

- No `.so` dependency for the package's own logic
- Easier to test and debug
- Works on any platform with an Aria compiler
- See `aria-puremath` as the zero-FFI reference example
