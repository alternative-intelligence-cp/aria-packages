# aria-libc

Standard C library wrappers for Aria — libc without the boilerplate.

**This is a stub.** The full source lives at https://github.com/alternative-intelligence-cp/aria-libc

## Modules

| Module | Description |
|--------|-------------|
| io | File I/O, stat, directory operations |
| mem | Memory allocation, copy, compare, read/write |
| string | String manipulation, conversion, classification |
| math | Trigonometry, logarithms, rounding, constants |
| time | Time, clocks, sleep, formatting |
| process | Environment, process control, working directory |
| net | BSD sockets, TCP/UDP, address resolution |
| posix | Signals, fork/exec, pipe, mmap |
| fs | Filesystem extras: access, chmod, symlinks, glob |
| regex | POSIX regex: compile, match, groups, replace |

## Usage

```aria
use "aria-libc/src/aria_libc_io.aria".*;
use "aria-libc/src/aria_libc_string.aria".*;
```

See the [full repository](https://github.com/alternative-intelligence-cp/aria-libc) for build instructions, static linking guide, and API reference.
