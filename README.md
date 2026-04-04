# aria-packages

Ecosystem package libraries for the [Aria programming language](https://github.com/ailp/aria).

**102 packages** (83 with manifests) — see [PACKAGES.md](PACKAGES.md) for the full categorized list and [STDLIB.md](STDLIB.md) for standard library modules.

## Packages

| Package | Description |
|---------|-------------|
| aria-args | CLI argument parsing via /proc/self/cmdline |
| aria-ascii | ASCII classification and conversion |
| aria-audio | Software audio synthesis — 4-channel mixing |
| aria-base64 | Base64 encoding/decoding |
| aria-bigdecimal | Arbitrary precision decimal arithmetic |
| aria-bits | Bit manipulation — test/set/clear/flip, popcount |
| aria-bitset | Fixed-size bit sets with set operations |
| aria-body-parser | HTTP body parser — URL-encoded, content-type |
| aria-buf | Byte/word packing/unpacking (little-endian) |
| aria-clamp | Min, max, clamp, abs, sign |
| aria-cli | Rich CLI output — ANSI colors, progress bars |
| aria-color | RGBA color packing, pixel transforms |
| aria-compress | Gzip/deflate compression |
| aria-console | Virtual 8-bit console address map |
| aria-conv | Saturating narrowing and float/int conversion |
| aria-cookie | HTTP cookie parsing and Set-Cookie builder |
| aria-cors | CORS header builder and origin validation |
| aria-crypto | SHA-256, MD5, HMAC hashing |
| aria-csv | CSV parser and writer (RFC 4180) |
| aria-datetime | Date/time — timestamps, formatting |
| aria-decision-t | Two-axis gradient decision construct |
| aria-deque | Double-ended queue — O(1) push/pop |
| aria-diff | Sequence diffing — LCS, edit distance |
| aria-display | Terminal display — cursor, colors, dimensions |
| aria-dns | DNS resolution — forward, reverse, validation |
| aria-endian | Byte-swap and rotation utilities |
| aria-entangled | Coupled DecisionGradients with propagation |
| aria-env | Environment variable management |
| aria-fixed | Q32.32 fixed-point arithmetic |
| aria-freq | Frequency/period arithmetic |
| aria-fs | Filesystem utilities |
| aria-ftp | FTP command builder and reply parser |
| aria-glob | Glob pattern matching — *, **, ? wildcards |
| aria-gradient-field | Spatial decision field (DGT emitter) |
| aria-gtk4 | GTK4 native desktop GUI bindings |
| aria-hash | Non-cryptographic hashing — FNV-1a, djb2 |
| aria-http | HTTP client — GET/POST/PUT/DELETE via libcurl |
| aria-ini | INI file parser |
| aria-input | Key mapping, button bitmask, press/release |
| aria-json | JSON tokeniser — byte-level scanning |
| aria-log | Structured logging — levels, timestamps |
| aria-lru | LRU cache — O(1) get/put, clock-based eviction |
| aria-map | Hash map (string→int64), FNV-1a, arena-backed |
| aria-math | Trig, exponential, logarithm via libm |
| aria-matrix | Dense matrix — add, multiply, transpose |
| aria-mime | MIME type detection from file extensions |
| aria-mock | Mock/stub framework for testing |
| aria-msgpack | MessagePack binary serialization |
| aria-mux | Bit-select, field insert/extract, conditional mux |
| aria-mysql | MySQL client via libmysqlclient |
| aria-opengl | OpenGL 3.3 Core — shaders, buffers, 3D math |
| aria-postgres | PostgreSQL client via libpq |
| aria-pqueue | Priority queue (min-heap) |
| aria-rand | xorshift64 PRNG |
| aria-rate-limit | Token-bucket rate limiter with HTTP headers |
| aria-raylib | Raylib bindings — 2D, audio, input |
| aria-redis | Redis client via hiredis |
| aria-regex | POSIX extended regular expressions |
| aria-resource-mem | Consumable expiring memory cells |
| aria-result | Extended Result/Option combinators |
| aria-retry | Retry with exponential backoff |
| aria-ringbuf | Circular buffer (FIFO) |
| aria-router | Express-style HTTP router |
| aria-sdl2 | SDL2 — windowing, 2D rendering, events |
| aria-semver | Semantic versioning parsing |
| aria-server | HTTP/1.1 server — request parsing, responses |
| aria-session | Server-side session management |
| aria-smtp | SMTP command builder and reply parser |
| aria-socket | TCP/UDP sockets — connect, listen, send |
| aria-sort | Sorting — quicksort, insertion, merge |
| aria-sqlite | SQLite3 — open, query, transactions |
| aria-static | Static file serving — MIME, path resolution |
| aria-stats | Descriptive statistics — mean, median, stddev |
| aria-str | High-level string manipulation |
| aria-template | String template engine |
| aria-test | Lightweight test framework |
| aria-tetris | Full Tetris clone — 7-bag, hold, ghost piece |
| aria-toml | TOML configuration parser |
| aria-url | URL parsing, encoding, decoding |
| aria-uuid | 128-bit UUID arithmetic |
| aria-vec | 2D/3D float64 vector math |
| aria-websocket | WebSocket protocol — handshake, state, frames |
| aria-xml | XML parsing and querying |
| aria-yaml | YAML parser with dotted-path access |
| aria-zigzag | Zigzag encoding for varint serialization |

## Installation

### Via aria-pkg
```bash
aria-pkg install aria-test
```

### Via APT (Debian/Ubuntu)
```bash
sudo apt install aria-packages
```

### Manual
Copy the desired package directory into your project or Aria's package search path.

## Package Structure

Each package follows the standard Aria package layout:

```
aria-<name>/
├── aria-package.toml    # Package manifest
├── src/
│   └── <name>.aria      # Source code
├── tests/
│   └── test_<name>.aria # Tests
└── README.md
```

## Contributing

1. Fork this repository
2. Create your package under `packages/`
3. Include `aria-package.toml`, source, and tests
4. Submit a pull request

## License

AGPL-3.0 — see [LICENSE.md](LICENSE.md)
