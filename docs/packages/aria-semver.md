# aria-semver — Semantic Versioning

**Version:** 0.2.6  
**Type:** aria-libc backed (aria_libc_string)  
**Ported:** v0.10.2

## API Reference

```aria
semver_parse(version: string) → int32    // parse version string, returns 1 on success
semver_get_major() → int32               // major version number
semver_get_minor() → int32               // minor version number
semver_get_patch() → int32               // patch version number
```

## Example

```aria
use "aria_semver.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:ok = raw semver_parse("1.2.3");
    int32:major = raw semver_get_major();
    int32:minor = raw semver_get_minor();
    int32:patch = raw semver_get_patch();
    println(string_from_int(@cast<int64>(major)));  // 1
    println(string_from_int(@cast<int64>(minor)));  // 2
    println(string_from_int(@cast<int64>(patch)));  // 3
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so` at runtime.
