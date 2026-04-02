# aria-libc

Standard C library wrappers for Aria — libc without the boilerplate.

Provides 10 shim libraries that expose C standard library and POSIX functionality
to Aria programs through the FFI extern block mechanism. Each library is a thin
C shim compiled as a shared library (`.so`) with corresponding Aria source wrappers.

## Libraries

| Library | Aria Source | Functions | Description |
|---------|-----------|-----------|-------------|
| `aria_libc_string` | `libc_string.aria` | 20+ | strlen, strcmp, strstr, substr, toupper/tolower, trim, ctype, atoi/atof |
| `aria_libc_io` | `libc_io.aria` | 15+ | fopen/fclose/fread/fwrite, fgets/fputs, fseek/ftell, read_file/write_file |
| `aria_libc_mem` | `libc_mem.aria` | 20+ | malloc/calloc/realloc/free, read/write primitives, rand, getenv |
| `aria_libc_math` | `libc_math.aria` | 40+ | trig, hyperbolic, exp/log, pow/sqrt, rounding, abs, min/max, constants |
| `aria_libc_time` | `libc_time.aria` | 15+ | time/clock, monotonic, UTC decomposition, make_utc, format/iso8601, sleep |
| `aria_libc_process` | `libc_process.aria` | 15+ | getpid, fork, exec, waitpid, system, kill, signals, pipe, getcwd/chdir |
| `aria_libc_net` | `libc_net.aria` | 15+ | TCP/UDP sockets, bind/listen/accept/connect, send/recv, DNS resolve |
| `aria_libc_posix` | `libc_posix.aria` | 25+ | open/close/read/write, stat helpers, unlink/rename, mkdir/rmdir, chmod, symlinks |
| `aria_libc_fs` | `libc_fs.aria` | 15+ | opendir/readdir/closedir, path ops (join/basename/dirname/ext), list_dir |
| `aria_libc_regex` | `libc_regex.aria` | 10+ | POSIX regex compile/match/search, capture groups, replace, case-insensitive |

## Usage

```aria
// Import via extern block — declare the functions you need
extern "aria_libc_string" {
    func:aria_libc_string_strlen = int64(string:s);
    func:aria_libc_string_strcmp = int32(string:a, string:b);
}

// Or use the Aria wrapper source files
use "aria-libc/src/libc_string.aria".*;
use "aria-libc/src/libc_io.aria".*;
```

## Building the Shim Libraries

```bash
cd shim
cc -O2 -shared -fPIC -o libaria_libc_string.so aria_libc_string.c
cc -O2 -shared -fPIC -o libaria_libc_io.so aria_libc_io.c
cc -O2 -shared -fPIC -o libaria_libc_mem.so aria_libc_mem.c
cc -O2 -shared -fPIC -o libaria_libc_math.so aria_libc_math.c -lm
cc -O2 -shared -fPIC -o libaria_libc_time.so aria_libc_time.c
cc -O2 -shared -fPIC -o libaria_libc_process.so aria_libc_process.c
cc -O2 -shared -fPIC -o libaria_libc_net.so aria_libc_net.c
cc -O2 -shared -fPIC -o libaria_libc_posix.so aria_libc_posix.c
cc -O2 -shared -fPIC -o libaria_libc_fs.so aria_libc_fs.c
cc -O2 -shared -fPIC -o libaria_libc_regex.so aria_libc_regex.c
```

## Running Tests

```bash
bash run_tests.sh
```

9 test suites, 131 assertions covering all 10 libraries.

## ABI Conventions

- String params: `string` in Aria → `const char*` in C
- String returns: `string` in Aria → `const char*` (static buffer) in C
- File/socket handles: `int64` (avoids optional-wrapper ABI issues)
- Integer types: `int32` ↔ `int32_t`, `int64` ↔ `int64_t`
- Float types: `flt64` ↔ `double`
- Void returns: `void` in both

## License

Apache-2.0
