# aria-yaml — YAML Parsing

**Version:** 0.2.6  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
yaml_parse(text: string) → int32                      // parse YAML string, returns entry count
yaml_parse_file(path: string) → int32                 // parse YAML file
yaml_get_type(key: string) → int32                    // get value type (1=string, 2=int, 3=float, 4=bool)
yaml_get_string(key: string) → string                 // get string value
yaml_get_int(key: string) → int64                     // get integer value
yaml_get_float(key: string) → flt64                   // get float value
yaml_get_bool(key: string) → int32                    // get boolean value (1/0)
yaml_has_key(key: string) → int32                     // 1 if key exists
yaml_error() → string                                 // last error message
yaml_set_string(key: string, val: string) → int32     // set/update string value
yaml_set_int(key: string, val: int64) → int32         // set/update integer value
yaml_set_bool(key: string, val: int32) → int32        // set/update boolean value
yaml_count() → int32                                  // number of entries
yaml_clear() → NIL                                    // clear all parsed data
```

## Example

```aria
use "aria_yaml.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:n = raw yaml_parse("name: Aria\nversion: 10\ndebug: true");
    string:name = raw yaml_get_string("name");
    int64:ver = raw yaml_get_int("version");
    int32:dbg = raw yaml_get_bool("debug");
    println(name);  // Aria
    drop yaml_clear();
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_string.so` at runtime.
