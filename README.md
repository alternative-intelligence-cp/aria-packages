# aria-packages

Ecosystem package libraries for the [Aria programming language](https://github.com/alternative-intelligence-cp/aria).

**80 packages** organized into utility, graphics/game, server/middleware, database, and AI/ML tiers.

## Packages

| Package | Description |
|---------|-------------|
| aria-args | Command-line argument parsing |
| aria-ascii | ASCII character classification and conversion |
| aria-audio | Software synthesis, MIDI note table, ALSA backend |
| aria-base64 | Base64 encoding/decoding |
| aria-bench | Benchmarking |
| aria-bits | Bit test/set/clear/flip, nibble extraction, popcount |
| aria-body-parser | HTTP body parsing middleware (JSON, URL-encoded, multipart) |
| aria-buf | Byte/word packing for uint64 buffers |
| aria-clamp | min, max, clamp, abs, sign for int64/uint64 |
| aria-cli | Enhanced CLI parsing with subcommands |
| aria-color | RGBA packing/unpacking and pixel transforms |
| aria-compress | Data compression (LZ4/zstd via FFI) |
| aria-console | 16-bit memory-mapped address space + 60fps frame scheduler |
| aria-conv | Saturating narrowing and float/int conversion |
| aria-cookie | Cookie parsing and Set-Cookie builder |
| aria-cors | CORS middleware with configurable origins |
| aria-crypto | Cryptographic primitives (SHA-256, AES, HMAC via FFI) |
| aria-csv | CSV parsing and generation |
| aria-cuda | CUDA FFI: device management, memory ops, kernel launch, cuBLAS |
| aria-datetime | Date/time formatting and arithmetic |
| aria-decision-t | Decision tree classifier (entropy, information gain) |
| aria-display | ANSI/termios terminal rendering (virtual console) |
| aria-editor | Terminal-mode text editor with search |
| aria-endian | Big/little-endian byte-swap for 16/32/64-bit |
| aria-entangled | Quantum-inspired entangled variable pairs |
| aria-env | Environment variable access and process info |
| aria-fixed | Q32.32 fixed-point arithmetic on uint64 |
| aria-freq | Frequency/period/baud integer arithmetic |
| aria-fs | File system utilities (stat, mkdir, readdir, copy) |
| aria-gml | GML compatibility layer: 40+ functions, xorshift32 RNG |
| aria-gradient-field | 3D gradient field computation |
| aria-gtk4 | GTK4 desktop GUI: widget registry, events, non-blocking UI |
| aria-hash | FNV-1a and djb2 string hashing |
| aria-http | HTTP client (GET/POST requests) |
| aria-input | Raw keyboard input with SNES-style button mapping |
| aria-jamba | Hybrid Transformer + Mamba + Mixture of Experts model |
| aria-jit | WildX JIT helpers |
| aria-json | JSON encoding for basic types |
| aria-log | Structured logging with severity levels |
| aria-looping | Iterative refinement model with convergence stopping |
| aria-mamba | Mamba selective state space model with SiLU gating |
| aria-map | Map data structure |
| aria-math | Trig, exp, log, rounding via C libm |
| aria-mime | MIME type detection and mapping |
| aria-mux | Bit-select, field insert/extract, mask ops, blend |
| aria-mysql | MySQL/MariaDB client via libmysqlclient |
| aria-opengl | OpenGL 3.3 Core via GLAD + SDL2 |
| aria-path | Path manipulation | 
| aria-postgres | PostgreSQL client via libpq (parameterized, LISTEN/NOTIFY) |
| aria-queue | Queue data structure |
| aria-rand | xorshift64 pseudo-random number generator |
| aria-rate-limit | Token bucket rate limiting middleware |
| aria-raylib | raylib v6.0 bindings: window, drawing, shapes, text, input, audio, gamepad |
| aria-redis | Redis client via hiredis (strings, lists, hashes, sets) |
| aria-regex | Regular expression matching |
| aria-resource-mem | RAII-style resource lifecycle management |
| aria-router | Express-style router: path params, middleware, wildcards |
| aria-sdl2 | SDL2 multimedia bindings: window, renderer, drawing, events |
| aria-semver | Semantic versioning: parse, compare, satisfy |
| aria-server | HTTP/1.1 server: listen, accept, parse, respond |
| aria-session | In-memory session management with crypto IDs |
| aria-socket | Socket abstraction layer |
| aria-sort | Various sorting algorithms |
| aria-sqlite | SQLite3 embedded database client (parameterized queries) |
| aria-static | Static file serving with MIME detection and path traversal protection |
| aria-str | String utilities (pad, trim, repeat, contains, split) |
| aria-template | String template rendering with variable substitution |
| aria-tensor | Dense tensor library: creation, arithmetic, matmul, activations, GPU interop |
| aria-test | Test framework with assertion helpers |
| aria-tetris | Full Tetris clone: sound effects, gamepad, high score |
| aria-toml | TOML configuration file parsing |
| aria-transformer | Transformer encoder: multi-head attention, causal masking |
| aria-uacp | Universal AI Communication Protocol: binary framing, 8 message types |
| aria-url | URL parsing, encoding, query string manipulation |
| aria-uuid | UUID v4 generation and formatting |
| aria-vec | 2D/3D float64 vector math (dot, cross, length) |
| aria-websocket | WebSocket client/server (RFC 6455) |
| aria-xml | XML parsing and generation |
| aria-yaml | YAML parsing and serialization |
| aria-zigzag | Zigzag encode/decode for signed integer interleaving |

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
