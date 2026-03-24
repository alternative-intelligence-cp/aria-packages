# aria-compress

Compression and decompression (gzip/deflate) for Aria

## Installation

```bash
aria-pkg install aria-compress
```

## API

| Function | Description |
|----------|-------------|
| `deflate(string:data, int32:len) -> int32` | Compress data using deflate. Returns compressed length. |
| `inflate(string:data, int32:len) -> int32` | Decompress deflate data. Returns decompressed length. |
| `gzip(string:data, int32:len) -> int32` | Compress data using gzip format. Returns compressed length. |
| `gunzip(string:data, int32:len) -> int32` | Decompress gzip data. Returns decompressed length. |
| `get_result() -> string` | Get the result buffer from the last operation. |
| `get_length() -> int32` | Get the length of the last result. |
| `get_error() -> int32` | Get the error code from the last operation. |
| `roundtrip(string:data, int32:len) -> int32` | Compress then decompress data, verifying integrity. Returns result length. |
| `version() -> string` | Get the zlib version string. |

## Example

```aria
use aria_compress;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    string:data = "Hello World! This is a test of compression in Aria.";

    // Compress with deflate
    int32:clen = deflate(data, 51i32);
    drop(println("Compressed length: &{clen}"));

    // Get compressed result
    string:compressed = get_result();

    // Decompress
    int32:dlen = inflate(compressed, clen);
    string:original = get_result();
    drop(println("Decompressed: &{original}"));

    // Or use roundtrip to verify
    int32:rt = roundtrip(data, 51i32);
};
```

## Dependencies

Requires **zlib** (`libz`) installed on the system.
