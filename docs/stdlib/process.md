# process — Process Management

**Backend:** aria-libc (aria_libc_process, aria_libc_mem)  
**Ported:** v0.10.0

## API Reference

```aria
Process.getpid() → int64                   // get current process ID
Process.getppid() → int64                  // get parent process ID
Process.system(cmd: string) → int32        // execute shell command, returns exit code
Process.getenv(name: string) → string      // get environment variable value
```

## Example

```aria
use "stdlib/process.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:pid = raw Process.getpid();
    println(string_from_int(pid));

    string:home = raw Process.getenv("HOME");
    println(home);

    int32:rc = raw Process.system("echo hello");
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_process.so`, `libaria_libc_string.so` at runtime.
