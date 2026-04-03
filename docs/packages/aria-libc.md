# aria-libc — Core C Bridge Library

**Version:** 0.9.1  
**Type:** C shared libraries (10 `.so` files)  
**Dependencies:** glibc

## Overview

aria-libc is the bridge layer between Aria programs and the C standard library. It provides 231 C wrapper functions organized into 10 domain-specific shared libraries. All stdlib modules and many packages depend on aria-libc.

## Libraries

| Library | Functions | Domain |
|---------|-----------|--------|
| `libaria_libc_math.so` | 33 | libm wrappers (sin, cos, exp, log, etc.) |
| `libaria_libc_io.so` | 8 | File streaming I/O (fopen, fgets, fputs, etc.) |
| `libaria_libc_string.so` | 4 | Byte-level string access |
| `libaria_libc_mem.so` | 14 | Memory allocation (malloc, free, byte/word access) |
| `libaria_libc_time.so` | 12 | Time: monotonic clock, formatting, timers |
| `libaria_libc_env.so` | 5 | Environment variables |
| `libaria_libc_fs.so` | 24 | Filesystem operations |
| `libaria_libc_proc.so` | 16 | Process management |
| `libaria_libc_sys.so` | 14 | System info (uname, sysinfo, hostname) |
| `libaria_libc_net.so` | 21 | Networking (socket, connect, bind, etc.) |

## Usage

In Aria source, declare the extern block for the library you need:

```aria
extern "aria_libc_math" {
    func:aria_libc_math_sin = flt64(flt64:x);
    func:aria_libc_math_cos = flt64(flt64:x);
}
```

Compile with library flags:

```bash
ariac myfile.aria -o myfile -L/path/to/shim -laria_libc_math -lm
```

Run with library path:

```bash
LD_LIBRARY_PATH=/path/to/shim ./myfile
```

## Building from Source

```bash
cd aria-packages/packages/aria-libc/shim
make              # builds all 10 .so files
make clean        # remove built libraries
```

## Design

All functions follow the naming convention `aria_libc_{domain}_{function}`. Pointers are passed as `int64` (not Aria pointer types) to avoid codegen bugs with the optional-wrapper. String parameters use `const char *` at the C ABI level, returned strings use the `AriaString {char*, int64_t}` struct.
