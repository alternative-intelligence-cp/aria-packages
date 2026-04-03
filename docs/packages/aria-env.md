# aria-env — Environment Variables

**Version:** 0.2.0  
**Type:** pure Aria (uses aria-libc_env for system calls)  
**Ported:** v0.9.2

## API Reference

```aria
env_get(name: string) → string          // get env var (empty if unset)
env_set(name, value: string) → int32    // set env var (0 success)
env_unset(name: string) → int32         // unset env var (0 success)
env_has(name: string) → int32           // 1 if set, 0 otherwise
env_home() → string                     // $HOME directory
```

## Example

```aria
use "aria_env.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:home = raw env_home();
    println(home);  // e.g., "/home/user"

    _ = raw env_set("MY_VAR", "hello");
    string:val = raw env_get("MY_VAR");
    println(val);  // "hello"

    int32:exists = raw env_has("MY_VAR");
    println(string_from_int(@cast<int64>(exists)));  // 1

    _ = raw env_unset("MY_VAR");
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_env.so` at runtime.
