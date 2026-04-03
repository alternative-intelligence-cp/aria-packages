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
