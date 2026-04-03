# aria-base64 — Base64 Encoding/Decoding

**Version:** 0.1.0  
**Type:** aria-libc backed (aria_libc_string, aria_libc_mem)  
**Ported:** v0.10.2

## API Reference

```aria
b64_encode_str(text: string) → string               // encode string to base64
b64_encode(text: string, len_param: int32) → string  // encode with explicit length
b64_encode_url_str(text: string) → string            // encode to URL-safe base64
b64_decode(encoded: string) → string                 // decode base64 to string
```

## Example

```aria
use "aria_base64.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:enc = raw b64_encode_str("Hello, World!");
    println(enc);  // SGVsbG8sIFdvcmxkIQ==
    string:dec = raw b64_decode(enc);
    println(dec);  // Hello, World!
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so`, `libaria_libc_mem.so` at runtime.
