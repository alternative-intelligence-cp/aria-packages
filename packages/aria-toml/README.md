# aria-toml

TOML configuration file parser — load, query, and modify TOML key-value pairs

## Installation

```bash
aria-pkg install aria-toml
```

## API

| Function | Description |
|----------|-------------|
| `toml_parse(string:text) -> int32` | Parse a TOML string. Returns entry count or negative on error. |
| `toml_parse_file(string:path) -> int32` | Parse a TOML file. Returns entry count or negative on error. |
| `toml_get_string(string:key) -> string` | Get a string value by dotted key (e.g. `"server.host"`). |
| `toml_get_int(string:key) -> int64` | Get an integer value by dotted key. |
| `toml_get_float(string:key) -> flt64` | Get a float value by dotted key. |
| `toml_get_bool(string:key) -> int32` | Get a boolean value (1 = true, 0 = false). |
| `toml_get_type(string:key) -> int32` | Get the type code of a value. |
| `toml_has_key(string:key) -> int32` | Check if a key exists (1 = yes, 0 = no). |
| `toml_set_string(string:key, string:val) -> int32` | Set a string value. |
| `toml_set_int(string:key, int64:val) -> int32` | Set an integer value. |
| `toml_set_bool(string:key, int32:val) -> int32` | Set a boolean value. |
| `toml_count() -> int32` | Get the total number of entries. |
| `toml_key_at(int32:idx) -> string` | Get the key name at a given index. |
| `toml_serialize() -> string` | Serialize the current state back to TOML text. |
| `toml_clear() -> NIL` | Clear all parsed data. |
| `toml_error() -> string` | Get the last error message. |

## Example

```aria
use aria_toml;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Parse a TOML file
    int32:n = toml_parse_file("config.toml");

    // Read values using dotted keys
    string:name = toml_get_string("package.name");
    int64:port  = toml_get_int("server.port");
    int32:debug = toml_get_bool("server.debug");

    drop(println("Name: &{name}"));

    // Modify and serialize
    drop(toml_set_string("server.host", "0.0.0.0"));
    string:out = toml_serialize();
    drop(println(out));

    toml_clear();
};
```

## Dependencies

None. The package includes its own TOML parser via `libaria_toml_shim.so`.
