# aria-crypto

Cryptographic hashing (SHA-256, MD5, HMAC) for Aria

## Installation

```bash
aria-pkg install aria-crypto
```

## API

| Function | Description |
|----------|-------------|
| `sha256(string:data) -> string` | Compute SHA-256 hash. Returns hex-encoded digest. |
| `md5(string:data) -> string` | Compute MD5 hash. Returns hex-encoded digest. |
| `hmac_sha256(string:key, string:data) -> string` | Compute HMAC-SHA256. Returns hex-encoded digest. |
| `sha256_verify(string:data, string:expected) -> int32` | Verify data against expected SHA-256 hash (1 = match, 0 = no match). |
| `md5_verify(string:data, string:expected) -> int32` | Verify data against expected MD5 hash (1 = match, 0 = no match). |

## Example

```aria
use aria_crypto;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Hash a string
    string:hash = sha256("hello");
    drop(println("SHA-256: &{hash}"));

    // Verify a hash
    int32:ok = sha256_verify("hello", "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    if (ok == 1i32) {
        drop(println("Hash verified!"));
    }

    // HMAC for message authentication
    string:mac = hmac_sha256("secret-key", "message data");
    drop(println("HMAC: &{mac}"));
};
```

## Dependencies

None. The package includes its own crypto implementations via `libaria_crypto_shim.so`.
