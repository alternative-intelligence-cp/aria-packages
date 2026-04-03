# Aria Standard Library Reference

The Aria standard library is organized into modules that extend the compiler's built-in functions. Modules are imported via `use "stdlib/module.aria".*;`

## Module Status

| Module | Version | Backend | Description |
|--------|---------|---------|-------------|
| `math` | v0.9.1 | aria-libc | Extended math: fmod, hypot, clamp, lerp, smoothstep, gcd/lcm |
| `io` | v0.9.1 | aria-libc | File streaming I/O via FileStream type |
| `string` | v0.9.1 | aria-libc | String manipulation: replace, split, join, reverse, parse |
| `mem` | v0.9.1 | aria-libc | Memory allocation: malloc, free, calloc, realloc, byte/word access |
| `fmt` | v0.9.1 | pure Aria | Formatted output: printf-style with `{0}`, `{1}` placeholders |
| `json` | — | extern | JSON parse/stringify |
| `net` | — | extern | TCP/UDP sockets |
| `thread` | — | extern | POSIX threading primitives |
| `channel` | — | extern | Thread-safe channels |
| `collections` | — | extern | Vec, HashMap, etc. |
| `process` | — | extern | Process spawning and management |
| `actor` | — | extern | Actor model concurrency |
| `toml` | — | extern | TOML parsing |
| `complex` | — | pure Aria | Complex number arithmetic |
| `linalg` | — | pure Aria | Linear algebra (vectors, matrices) |

**Backend key:**
- `aria-libc` — Uses aria-libc shared library shims (portable, ABI-safe)
- `pure Aria` — No FFI; works on any platform with an Aria compiler
- `extern` — Uses direct inline/block extern (to be migrated in future versions)

## Compiler Builtins (No Import Needed)

These are available in every Aria program without any `use` statement:

### I/O
- `print(s)`, `println(s)` — write to stdout
- `stderr_write(s)` — write to stderr
- `stdin_read_line()` — read line from stdin
- `readFile(path)` → `Result<string>` — read entire file
- `writeFile(path, content)` → `Result<int64>` — write entire file
- `fileExists(path)` → `Result<bool>` — check file existence
- `fileSize(path)` → `Result<int64>` — get file size in bytes
- `deleteFile(path)` → `Result<int64>` — delete a file

### Math
- `sin(x)`, `cos(x)`, `tan(x)` — trigonometric functions
- `asin(x)`, `acos(x)`, `atan(x)`, `atan2(y,x)` — inverse trig
- `sqrt(x)`, `cbrt(x)` — roots
- `pow(x,y)`, `exp(x)`, `log(x)`, `ln(x)`, `log2(x)`, `log10(x)` — powers/logs
- `abs(x)`, `floor(x)`, `ceil(x)`, `round(x)`, `trunc(x)` — rounding
- `min(a,b)`, `max(a,b)` — comparison
- `PI()`, `E()`, `TAU()` — constants

### Strings
- `string_length(s)`, `string_is_empty(s)`, `string_equals(a,b)`
- `string_concat(a,b)`, `string_contains(s,sub)`
- `string_starts_with(s,pfx)`, `string_ends_with(s,sfx)`
- `string_index_of(s,sub)` — returns index or -1
- `string_substring(s,start,len)` — extract substring
- `string_repeat(s,n)`, `string_trim(s)`, `string_trim_start(s)`, `string_trim_end(s)`
- `string_to_upper(s)`, `string_to_lower(s)`
- `string_pad_left(s,n,c)`, `string_pad_right(s,n,c)`
- `string_from_int(n)`, `string_from_char(codepoint)`

---

## Detailed Module Documentation

- [math](stdlib/math.md) — Extended mathematics
- [io](stdlib/io.md) — File streaming I/O
- [string](stdlib/string.md) — String manipulation
- [mem](stdlib/mem.md) — Memory allocation
- [fmt](stdlib/fmt.md) — Formatted output
