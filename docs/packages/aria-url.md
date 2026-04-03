# aria-url — URL Parsing

**Version:** 0.2.6  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
parse(url: string) → int32            // parse URL, returns 1 on success
get_scheme() → string                 // "http", "https", etc.
get_host() → string                   // hostname
get_port() → int32                    // port number (0 if not specified)
get_path() → string                   // path component
get_query() → string                  // query string (after ?)
get_fragment() → string               // fragment (after #)
encode(input: string) → string        // percent-encode a string
```

## Example

```aria
use "aria_url.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:ok = raw parse("https://example.com:8080/path?q=test#frag");
    println(raw get_scheme());    // https
    println(raw get_host());      // example.com
    println(raw get_path());      // /path
    println(raw get_query());     // q=test
    println(raw get_fragment());  // frag
    exit 0;
};
```

## Dependencies

None (pure Aria).
