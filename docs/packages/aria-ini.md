# aria-ini — INI File Parsing

**Version:** 0.3.0  
**Type:** aria-libc backed (aria_libc_string, aria_libc_mem)  
**Ported:** v0.10.2

## API Reference

```aria
ini_parse(text: string) → int32                        // parse INI text, returns section count
ini_num_sections() → int32                             // number of sections
ini_section_name(idx: int32) → string                  // get section name by index
ini_num_keys(section: string) → int32                  // number of keys in a section
ini_get(section: string, key: string) → string         // get value for section.key
ini_has_key(section: string, key: string) → int32      // 1 if key exists
ini_cleanup() → NIL                                    // free parsed data
```

## Example

```aria
use "aria_ini.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:n = raw ini_parse("[database]\nhost=localhost\nport=5432\n[app]\nname=myapp");
    string:host = raw ini_get("database", "host");
    string:name = raw ini_get("app", "name");
    println(host);  // localhost
    println(name);  // myapp
    drop ini_cleanup();
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so`, `libaria_libc_mem.so` at runtime.
