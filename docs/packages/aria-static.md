# aria-static — Static File Serving

**Version:** 0.2.9  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
static_set_root(root: string) → NIL            // set document root directory
static_get_root() → string                     // get current document root
static_set_index(filename: string) → NIL       // set index filename (e.g. "index.html")
static_resolve(url_path: string) → int32       // resolve URL path to file (1=found, 0=not found)
```

## Example

```aria
use "aria_static.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    drop static_set_root("/var/www/html");
    drop static_set_index("index.html");
    int32:found = raw static_resolve("/about");
    println(string_from_int(@cast<int64>(found)));
    exit 0;
};
```

## Dependencies

None (pure Aria).
