# Aria Packages — Canonical Package List

**Version**: v0.12.3
**Total**: 83 packages (with manifest) + 19 legacy = 102 directories

---

## Networking & Web

| Package | Type | Description |
|---------|------|-------------|
| aria-http | library | HTTP client — GET, POST, PUT, DELETE, headers, status codes via libcurl |
| aria-dns | library | DNS resolution — hostname to IP, reverse lookup, validation |
| aria-socket | library | TCP/UDP sockets — connect, listen, accept, send, receive |
| aria-server | library | HTTP/1.1 server — request parsing, response helpers |
| aria-url | library | URL parsing, encoding, and decoding |
| aria-redis | library | Redis client via hiredis |
| aria-router | library | Express-style HTTP router — route matching, path params |

## HTTP Middleware

| Package | Type | Description |
|---------|------|-------------|
| aria-cookie | pure | HTTP cookie parsing and Set-Cookie builder (RFC 6265) |
| aria-cors | pure | CORS header builder and origin validation |
| aria-body-parser | pure | HTTP body parser — URL-encoded, content-type detection |
| aria-session | pure | Server-side session management — variables, cookie headers |
| aria-static | pure | Static file path resolution and MIME detection |
| aria-rate-limit | pure | Token-bucket rate limiter with HTTP headers |

## Protocol Builders

| Package | Type | Description |
|---------|------|-------------|
| aria-ftp | pure | FTP command builder and reply parser |
| aria-smtp | pure | SMTP command builder, email composer, reply parser |
| aria-websocket | pure | WebSocket protocol — handshake, state tracking, frame types |

## Terminal & Input

| Package | Type | Description |
|---------|------|-------------|
| aria-display | pure | Terminal display — cursor, colors, attributes, dimensions |
| aria-input | pure | Key mapping, button bitmask, press/release tracking |
| aria-cli | library | Rich CLI output — ANSI colors, styles, progress bars |
| aria-console | library | Virtual 8-bit console address map and frame scheduler |

## Data Structures & Algorithms

| Package | Type | Description |
|---------|------|-------------|
| aria-lru | pure | LRU cache — O(1) get/put, clock-based eviction |
| aria-map | library | Hash map (string→int64) using FNV-1a, arena-backed |
| aria-sort | library | Sorting algorithms — quicksort, insertion sort, merge sort |
| aria-pqueue | library | Priority queue (min-heap) with priorities |
| aria-deque | library | Double-ended queue — O(1) push/pop, circular buffer |
| aria-bitset | library | Fixed-size bit sets — union, intersect, complement |
| aria-ringbuf | library | Fixed-capacity circular buffer (FIFO) |
| aria-result | library | Extended Result/Option combinators |
| aria-diff | pure | Sequence diffing — LCS, edit distance, patch generation |
| aria-matrix | pure | Dense matrix operations — add, multiply, transpose, determinant |

## Utility

| Package | Type | Description |
|---------|------|-------------|
| aria-glob | pure | Glob pattern matching — *, **, ? wildcards for file paths |
| aria-retry | pure | Retry with exponential backoff — configurable cap |
| aria-test | library | Lightweight test framework |
| aria-mock | library | Mock/stub framework for testing |
| aria-args | library | CLI argument parsing via /proc/self/cmdline |
| aria-env | library | Environment variable management |
| aria-template | library | String template engine with variable substitution |
| aria-semver | library | Semantic versioning parsing and comparison |
| aria-log | library | Structured logging — levels, timestamps, formatted output |

## Data Formats

| Package | Type | Description |
|---------|------|-------------|
| aria-json | library | JSON tokeniser — byte-level scanning |
| aria-toml | library | TOML configuration file parser |
| aria-yaml | library | YAML parser with dotted-path access |
| aria-xml | library | XML parsing and querying |
| aria-csv | library | CSV parser and writer (RFC 4180) |
| aria-ini | library | INI file parser |
| aria-msgpack | library | MessagePack binary serialization |
| aria-base64 | library | Base64 encoding and decoding |

## Database

| Package | Type | Description |
|---------|------|-------------|
| aria-sqlite | library | SQLite3 client — open, query, execute, transactions |
| aria-mysql | library | MySQL client via libmysqlclient |
| aria-postgres | library | PostgreSQL client via libpq |

## Math & Numeric

| Package | Type | Description |
|---------|------|-------------|
| aria-math | library | Trig, exponential, logarithm, rounding via C libm |
| aria-stats | aria-libc | Descriptive statistics — mean, median, stddev, correlation |
| aria-vec | library | 2D/3D float64 vector math — dot, cross, length |
| aria-bigdecimal | library | Arbitrary precision decimal arithmetic |
| aria-rand | library | xorshift64 PRNG |
| aria-clamp | library | Min, max, clamp, abs, sign for int64/uint64 |
| aria-conv | library | Saturating narrowing and float/int conversion |
| aria-freq | library | Frequency/period arithmetic — hz, ns, baud timing |
| aria-fixed | library | Q32.32 fixed-point arithmetic on uint64 |

## Bit & Byte Operations

| Package | Type | Description |
|---------|------|-------------|
| aria-bits | library | Bit manipulation — test/set/clear/flip, nibble, popcount |
| aria-buf | library | Byte/word packing/unpacking (little-endian) |
| aria-endian | library | Byte-swap and rotation — endian conversion, circular shifts |
| aria-hash | library | Non-cryptographic hashing — FNV-1a, djb2, bit-mixing |
| aria-mux | library | Bit-select, field insert/extract, conditional mux |
| aria-zigzag | library | Zigzag encoding/decoding for varint serialization |
| aria-uuid | library | 128-bit UUID arithmetic — pack/unpack, version extraction |
| aria-ascii | library | ASCII classification — is_upper, is_digit, to_lower |
| aria-color | library | RGBA color packing, unpacking, pixel transforms |

## Crypto & Security

| Package | Type | Description |
|---------|------|-------------|
| aria-crypto | library | SHA-256, MD5, HMAC hashing |
| aria-compress | library | Gzip/deflate compression and decompression |

## String

| Package | Type | Description |
|---------|------|-------------|
| aria-str | library | High-level string manipulation |
| aria-mime | library | MIME type detection from file extensions |
| aria-regex | library | POSIX extended regular expressions |

## AI / ML

| Package | Type | Description |
|---------|------|-------------|
| aria-decision-t | library | Two-axis gradient decision construct with time-axis history |
| aria-entangled | library | Coupled DecisionGradients with tilt-axis propagation |
| aria-gradient-field | library | Spatial decision field driven by DGT emitter |
| aria-resource-mem | library | Consumable expiring memory cells with TTL limits |

## Game Development

| Package | Type | Description |
|---------|------|-------------|
| aria-raylib | library | Raylib bindings — windowing, 2D drawing, textures, audio |
| aria-tetris | executable | Full Tetris clone — 7-bag, hold piece, ghost piece |
| aria-gml | library | GameMaker Language compatibility layer |
| aria-opengl | library | OpenGL 3.3 Core — shaders, buffers, textures, 3D math |
| aria-audio | library | Software audio synthesis — 4-channel mixing, waveforms |

## GUI & Graphics

| Package | Type | Description |
|---------|------|-------------|
| aria-gtk4 | library | GTK4 bindings — native desktop GUI |
| aria-sdl2 | library | SDL2 bindings — windowing, 2D rendering, events |
| aria-cuda | library | CUDA GPU compute bindings |

## System & I/O

| Package | Type | Description |
|---------|------|-------------|
| aria-fs | library | Filesystem utilities |
| aria-datetime | library | Date and time — timestamps, formatting, components |
| aria-editor | library | Text editor utilities |
| aria-libc | library | Core C standard library bindings |
