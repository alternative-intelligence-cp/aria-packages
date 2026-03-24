# aria-url

URL parsing, encoding, and decoding for Aria

## Installation

```bash
aria-pkg install aria-url
```

## API

| Function | Description |
|----------|-------------|
| `parse(string:url) -> int32` | Parse a URL into components. Returns 1 on success, 0 on failure. |
| `get_scheme() -> string` | Get the scheme (e.g. `"https"`). Call after `parse()`. |
| `get_host() -> string` | Get the hostname. Call after `parse()`. |
| `get_port() -> int32` | Get the port number. Call after `parse()`. |
| `get_path() -> string` | Get the path. Call after `parse()`. |
| `get_query() -> string` | Get the query string. Call after `parse()`. |
| `get_fragment() -> string` | Get the fragment/anchor. Call after `parse()`. |
| `encode(string:input) -> string` | Percent-encode a string for use in URLs. |
| `decode(string:input) -> string` | Decode a percent-encoded string. |

## Example

```aria
use aria_url;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Parse a URL
    int32:ok = parse("https://example.com:8080/api/data?key=val#section");

    if (ok == 1i32) {
        string:scheme = get_scheme();
        string:host   = get_host();
        int32:port    = get_port();
        string:path   = get_path();

        drop(println("Host: &{host}"));
        drop(println("Port: &{port}"));
    }

    // Encode/decode
    string:enc = encode("hello world");
    string:dec = decode("hello%20world");
};
```

## Dependencies

None. The package includes its own URL parser via `libaria_url_shim.so`.
