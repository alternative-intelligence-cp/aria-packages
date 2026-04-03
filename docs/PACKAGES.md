# Aria Packages Reference

The Aria package ecosystem. Packages live in `packages/` and are described by `manifest.toml`.

## Package Categories

### Core Infrastructure
| Package | Version | Type | Description |
|---------|---------|------|-------------|
| `aria-libc` | 0.9.1 | C shim library | Bridge layer: 231 C functions wrapping libc for ABI-safe FFI |
| `aria-puremath` | 0.1.0 | pure Aria | Taylor series math for freestanding platforms (zero FFI) |

### Ported to Pure Aria (v0.9.2)
These packages were rewritten from C FFI shims to pure Aria, using aria-libc for low-level byte access:

| Package | Version | Functions | Description |
|---------|---------|-----------|-------------|
| `aria-math` | 0.2.0 | 15 | Math operations: factorial, fibonacci, is_prime, combinations, etc. |
| `aria-bench` | 0.2.0 | 4 | Micro-benchmarking: bench_run, bench_report, timing |
| `aria-path` | 0.2.0 | 11 | Path manipulation: join, dirname, basename, extension, normalize |
| `aria-env` | 0.2.0 | 5 | Environment variables: get, set, unset, list, has |
| `aria-hex` | 0.2.0 | 4 | Hex encoding: encode, decode, encode_upper, is_valid |
| `aria-sort` | 0.2.0 | 4 | Sorting: quicksort, insertion_sort, is_sorted, reverse |
| `aria-regex` | 0.2.0 | 7 | Regex: match, match_all, replace, split, count, is_valid, escape |

### Ported to Pure Aria (v0.10.1)

| Package | Version | Functions | Description |
|---------|---------|-----------|-------------|
| `aria-bigdecimal` | 0.3.2 | 11 | Arbitrary precision decimals: create, add, subtract, multiply, compare |
| `aria-decision-t` | 0.2.0 | 30 | Decision tree data structure: graph operations, triggers, snapshots |
| `aria-mock` | 0.3.2 | 11 | Test mocking: create mock functions, track calls, set return values |
| `aria-rate-limit` | 0.2.9 | 10 | Rate limiting: token bucket, check, remaining, retry-after |
| `aria-resource-mem` | 0.2.0 | 15 | Memory resource management: read/write limits, TTL, expiry |
| `aria-static` | 0.2.9 | 4 | Static file serving: set root, resolve URL paths |
| `aria-test` | 0.1.0 | 18 | Test framework: assertions, suites, pass/fail/skip counting |
| `aria-tetris` | 0.2.8 | — | Terminal Tetris game (executable) |
| `aria-url` | 0.2.6 | 8 | URL parsing: scheme, host, port, path, query, fragment, encode |
| `aria-xml` | 0.2.6 | 8 | XML parsing: parse, get text/attr, count/has tag, XPath-like lookup |
| `aria-yaml` | 0.2.6 | 14 | YAML parsing: parse, typed getters, set, has_key, error |
| `aria-looping` | 0.2.10 | 21 | Looping transformer (ML): adaptive-depth inference |

### Migrated to aria-libc (v0.10.2)

| Package | Version | Functions | Description |
|---------|---------|-----------|-------------|
| `aria-args` | 0.2.0 | 13 | CLI argument parsing: has, get, at, count, make |
| `aria-base64` | 0.1.0 | 4 | Base64 encoding: encode, decode, URL-safe encode |
| `aria-csv` | 0.2.2 | 10 | CSV parsing: parse, get cells, write/build CSV |
| `aria-datetime` | 0.2.2 | 27 | Date/time: epoch, components, format, make, diff, add |
| `aria-ini` | 0.3.0 | 7 | INI file parsing: parse, sections, keys, get, has_key |
| `aria-json` | 0.2.0 | 5 | JSON tokenizer: tokenize, next_tok, seq_matches |
| `aria-log` | 0.2.2 | 15 | Logging: levels, init, set level, output targets, timestamps |
| `aria-semver` | 0.2.6 | 4 | Semantic versioning: parse, get major/minor/patch |
| `aria-str` | 0.2.0 | 9 | Extended strings: char_at, slice, before/after, between, replace |
| `aria-template` | 0.2.6 | 3 | String templates: set variable, get, render {{key}} |

### Data Structures
| Package | Description |
|---------|-------------|
| `aria-vec` | Dynamic arrays |
| `aria-map` | Hash maps |
| `aria-queue` | FIFO queues |
| `aria-pqueue` | Priority queues |
| `aria-ringbuf` | Ring buffers |
| `aria-buf` | Byte buffers |
| `aria-matrix` | 2D matrices |

### String & Text
| Package | Description |
|---------|-------------|
| `aria-str` | Extended string operations |
| `aria-base64` | Base64 encoding/decoding |
| `aria-ascii` | ASCII table and utilities |
| `aria-csv` | CSV parsing |
| `aria-json` | JSON parse/stringify |
| `aria-toml` | TOML parsing |
| `aria-xml` | XML parsing |
| `aria-yaml` | YAML parsing |
| `aria-ini` | INI file parsing |
| `aria-template` | String templates |
| `aria-diff` | Text diffing |

### Networking
| Package | Description |
|---------|-------------|
| `aria-http` | HTTP client/server |
| `aria-socket` | Raw sockets |
| `aria-websocket` | WebSocket protocol |
| `aria-dns` | DNS resolution |
| `aria-ftp` | FTP client |
| `aria-smtp` | SMTP email |
| `aria-url` | URL parsing |

### Crypto & Security
| Package | Description |
|---------|-------------|
| `aria-crypto` | Cryptographic primitives |
| `aria-hash` | Hash functions |
| `aria-uuid` | UUID generation |

### System
| Package | Description |
|---------|-------------|
| `aria-fs` | Filesystem operations |
| `aria-args` | CLI argument parsing |
| `aria-cli` | CLI app framework |
| `aria-log` | Logging |
| `aria-compress` | Compression |
| `aria-datetime` | Date/time |
| `aria-semver` | Semantic versioning |
| `aria-conv` | Type conversions |

### Concurrency
| Package | Description |
|---------|-------------|
| `aria-actor` | Actor model |
| `aria-channel` | Thread-safe channels |

### GUI & Graphics
| Package | Description |
|---------|-------------|
| `aria-gtk4` | GTK4 bindings |
| `aria-qt6` | Qt6 bindings |
| `aria-sdl2` | SDL2 bindings |
| `aria-sdl3` | SDL3 bindings |
| `aria-raylib` | Raylib bindings |
| `aria-opengl` | OpenGL bindings |
| `aria-webkit-gtk` | WebKit GTK bindings |
| `aria-wxwidgets` | wxWidgets bindings |

### AI/ML
| Package | Description |
|---------|-------------|
| `aria-tensor` | Tensor operations |
| `aria-cuda` | CUDA GPU compute |
| `aria-transformer` | Transformer architecture |
| `aria-mamba` | Mamba (SSM) architecture |
| `aria-jamba` | Jamba hybrid architecture |
| `aria-looping` | Looping transformer |
| `aria-uacp` | Universal Acoustic Codec Protocol |

### Database
| Package | Description |
|---------|-------------|
| `aria-sqlite` | SQLite bindings |
| `aria-postgres` | PostgreSQL bindings |
| `aria-mysql` | MySQL bindings |
| `aria-redis` | Redis client |

### Testing
| Package | Description |
|---------|-------------|
| `aria-test` | Test framework |
| `aria-mock` | Mocking utilities |

---

## Detailed Package Documentation

- [aria-libc](packages/aria-libc.md) — Core C bridge library
- [aria-puremath](packages/aria-puremath.md) — Pure Aria math (Taylor series)
- [aria-math](packages/aria-math.md) — Math operations
- [aria-bench](packages/aria-bench.md) — Micro-benchmarking
- [aria-path](packages/aria-path.md) — Path manipulation
- [aria-env](packages/aria-env.md) — Environment variables
- [aria-hex](packages/aria-hex.md) — Hex encoding/decoding
- [aria-sort](packages/aria-sort.md) — Sorting algorithms
- [aria-regex](packages/aria-regex.md) — Regular expressions
- [aria-bigdecimal](packages/aria-bigdecimal.md) — Arbitrary precision decimals
- [aria-decision-t](packages/aria-decision-t.md) — Decision tree
- [aria-mock](packages/aria-mock.md) — Test mocking
- [aria-rate-limit](packages/aria-rate-limit.md) — Rate limiting
- [aria-resource-mem](packages/aria-resource-mem.md) — Memory resource management
- [aria-static](packages/aria-static.md) — Static file serving
- [aria-test](packages/aria-test.md) — Test framework
- [aria-url](packages/aria-url.md) — URL parsing
- [aria-xml](packages/aria-xml.md) — XML parsing
- [aria-yaml](packages/aria-yaml.md) — YAML parsing
- [aria-looping](packages/aria-looping.md) — Looping transformer (ML)
- [aria-args](packages/aria-args.md) — CLI argument parsing
- [aria-base64](packages/aria-base64.md) — Base64 encoding
- [aria-csv](packages/aria-csv.md) — CSV parsing
- [aria-datetime](packages/aria-datetime.md) — Date/time
- [aria-ini](packages/aria-ini.md) — INI file parsing
- [aria-json](packages/aria-json.md) — JSON tokenizer
- [aria-log](packages/aria-log.md) — Logging
- [aria-semver](packages/aria-semver.md) — Semantic versioning
- [aria-str](packages/aria-str.md) — Extended strings
- [aria-template](packages/aria-template.md) — String templates
