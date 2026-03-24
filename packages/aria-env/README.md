# aria-env

Environment variable management for Aria

## Installation

```bash
aria-pkg install aria-env
```

## API

| Function | Description |
|----------|-------------|
| `get(string:name) -> string` | Get the value of an environment variable. |
| `set(string:name, string:value) -> int32` | Set an environment variable. Returns 1 on success. |
| `unset(string:name) -> int32` | Unset an environment variable. Returns 1 on success. |
| `has(string:name) -> int32` | Check if an environment variable exists (1 = yes, 0 = no). |
| `get_or(string:name, string:default_val) -> string` | Get a variable or return a default value if not set. |
| `count() -> int32` | Get the total number of environment variables. |
| `home() -> string` | Get the user's home directory (`$HOME`). |
| `path() -> string` | Get the system `$PATH`. |
| `user() -> string` | Get the current username. |
| `shell() -> string` | Get the current shell. |

## Example

```aria
use aria_env;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Set and get
    drop(set("MY_APP_MODE", "production"));
    string:mode = get("MY_APP_MODE");
    drop(println("Mode: &{mode}"));

    // Check existence with fallback
    string:port = get_or("PORT", "8080");
    drop(println("Port: &{port}"));

    // System info
    string:home = home();
    string:user = user();
    drop(println("User &{user} at &{home}"));

    // Clean up
    drop(unset("MY_APP_MODE"));
};
```

## Dependencies

None. The package uses POSIX environment APIs via `libaria_env_shim.so`.
