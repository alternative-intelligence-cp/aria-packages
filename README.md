# aria-packages

Ecosystem package libraries for the [Aria programming language](https://github.com/ailp/aria).

## Packages

| Package | Description |
|---------|-------------|
| aria-args | Command-line argument parsing |
| aria-ascii | ASCII art and character utilities |
| aria-audio | Audio playback and processing |
| aria-base64 | Base64 encoding/decoding |
| aria-bits | Bitwise operations and bit manipulation |
| aria-buf | Buffer management |
| aria-clamp | Value clamping utilities |
| aria-color | Color representation and conversion |
| aria-console | Terminal/console utilities |
| aria-conv | Type conversion utilities |
| aria-csv | CSV parsing and writing |
| aria-datetime | Date and time operations |
| aria-decision-t | Decision tree data structure |
| aria-display | Display/rendering utilities |
| aria-endian | Byte order conversion |
| aria-entangled | Entangled value pairs |
| aria-fixed | Fixed-point arithmetic |
| aria-freq | Frequency analysis |
| aria-fs | Filesystem operations |
| aria-gradient-field | Gradient field computation |
| aria-gtk4 | GTK4 GUI toolkit bindings |
| aria-hash | Hashing algorithms |
| aria-http | HTTP client/server |
| aria-input | User input handling |
| aria-json | JSON parsing and generation |
| aria-log | Logging framework |
| aria-math | Extended math operations |
| aria-mux | Multiplexer/router |
| aria-rand | Random number generation |
| aria-raylib | Raylib game library bindings |
| aria-regex | Regular expression support |
| aria-resource-mem | Memory resource management |
| aria-sdl2 | SDL2 multimedia bindings |
| aria-socket | Network socket operations |
| aria-str | String manipulation utilities |
| aria-test | Testing framework |
| aria-uuid | UUID generation |
| aria-vec | Vector/dynamic array |
| aria-zigzag | Zigzag encoding |

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
