# aria-hex — Hex Encoding/Decoding

**Version:** 0.2.0  
**Type:** pure Aria (uses aria-libc_string for byte access)  
**Ported:** v0.9.2

## API Reference

```aria
hex_encode(input: string) → string        // encode bytes to lowercase hex
hex_encode_upper(input: string) → string   // encode bytes to uppercase hex
hex_decode(hex: string) → string           // decode hex string to bytes
hex_is_valid(hex: string) → int32          // 1 if valid hex (even length, valid chars)
```

## Example

```aria
use "aria_hex.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:data = "Hello!";
    string:encoded = raw hex_encode(data);
    println(encoded);  // "48656c6c6f21"

    string:upper = raw hex_encode_upper(data);
    println(upper);  // "48656C6C6F21"

    string:decoded = raw hex_decode(encoded);
    println(decoded);  // "Hello!"

    int32:valid = raw hex_is_valid(encoded);
    println(string_from_int(@cast<int64>(valid)));  // 1

    int32:invalid = raw hex_is_valid("xyz");
    println(string_from_int(@cast<int64>(invalid)));  // 0

    exit 0;
};
```

## Performance (1k iterations)

| Operation | 12 bytes | 54 bytes |
|-----------|----------|----------|
| Encode | 4,773 ns | 12,234 ns |
| Decode | 4,212 ns | 5,305 ns |

## Dependencies

Requires `libaria_libc_string.so` at runtime.
