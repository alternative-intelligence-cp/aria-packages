# aria-path — Path Manipulation

**Version:** 0.2.0  
**Type:** pure Aria (uses aria-libc_string for byte access)  
**Ported:** v0.9.2

## API Reference

```aria
path_join(a, b: string) → string          // join two path segments
path_dirname(p: string) → string          // directory portion
path_basename(p: string) → string         // filename portion
path_extension(p: string) → string        // file extension (with dot)
path_stem(p: string) → string             // filename without extension
path_is_absolute(p: string) → int32       // 1 if starts with /
path_is_relative(p: string) → int32       // 1 if not absolute
path_normalize(p: string) → string        // resolve . and .. segments
path_has_extension(p: string) → int32     // 1 if has extension
path_parent(p: string) → string           // parent directory
path_with_extension(p, ext: string) → string  // replace extension
```

## Example

```aria
use "aria_path.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:p = raw path_join("/home/user", "docs/readme.md");
    println(p);  // "/home/user/docs/readme.md"

    println(raw path_dirname(p));    // "/home/user/docs"
    println(raw path_basename(p));   // "readme.md"
    println(raw path_extension(p));  // ".md"
    println(raw path_stem(p));       // "readme"

    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so` at runtime.
