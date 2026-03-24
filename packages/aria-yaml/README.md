# aria-yaml

YAML configuration parser — load, query, and modify YAML key-value pairs with dotted-path access

## Installation

```bash
aria-pkg install aria-yaml
```

## API

| Function | Description |
|----------|-------------|
| `yaml_parse(string:text) -> int32` | Parse a YAML string. Returns entry count or negative on error. |
| `yaml_parse_file(string:path) -> int32` | Parse a YAML file. Returns entry count or negative on error. |
| `yaml_get_string(string:key) -> string` | Get a string value by dotted key (e.g. `"server.host"`). |
| `yaml_get_int(string:key) -> int64` | Get an integer value by dotted key. |
| `yaml_get_float(string:key) -> flt64` | Get a float value by dotted key. |
| `yaml_get_bool(string:key) -> int32` | Get a boolean value (1 = true, 0 = false). |
| `yaml_get_type(string:key) -> int32` | Get the type code of a value. |
| `yaml_has_key(string:key) -> int32` | Check if a key exists (1 = yes, 0 = no). |
| `yaml_set_string(string:key, string:val) -> int32` | Set a string value. |
| `yaml_set_int(string:key, int64:val) -> int32` | Set an integer value. |
| `yaml_set_bool(string:key, int32:val) -> int32` | Set a boolean value. |
| `yaml_count() -> int32` | Get the total number of entries. |
| `yaml_clear() -> NIL` | Clear all parsed data. |
| `yaml_error() -> string` | Get the last error message. |

## Example

```aria
use aria_yaml;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Parse inline YAML
    int32:n = yaml_parse("name: MyApp\nversion: 3\nserver:\n  host: localhost\n  port: 8080\n");

    // Read values using dotted paths
    string:name = yaml_get_string("name");
    int64:port  = yaml_get_int("server.port");

    drop(println("App: &{name}"));

    // Check for keys
    int32:exists = yaml_has_key("server.host");

    yaml_clear();
};
```

## Dependencies

None. The package includes its own YAML parser via `libaria_yaml_shim.so`.
