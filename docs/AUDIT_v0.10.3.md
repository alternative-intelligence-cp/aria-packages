# Aria v0.10.3 — Standard Library & Package Audit

**Date:** April 2, 2026  
**Compiler:** ariac (dev branch, v0.10.3 tag)  
**Packages repo:** aria-packages (main branch, v0.10.3 tag)  
**Previous audit:** v0.9.4

---

## Standard Library Audit

**Total stdlib files:** 59 (57 in stdlib/ + 2 in stdlib/traits/)

### Category A — Migrated to aria-libc (12 files)

These use `extern "aria_libc_*"` block externs backed by `.so` shim libraries. Zero direct C runtime calls.

| Module | aria-libc libs used | Since |
|--------|-------------------|-------|
| `math.aria` | aria_libc_math | v0.9.1 |
| `io.aria` | aria_libc_io | v0.9.1 |
| `string.aria` | aria_libc_string, aria_libc_mem | v0.9.1 |
| `mem.aria` | aria_libc_mem | v0.9.1 |
| `binary.aria` | aria_libc_mem, aria_libc_math, aria_libc_io | v0.10.0 |
| `pipe.aria` | aria_libc_process | v0.10.0 |
| `process.aria` | aria_libc_process, aria_libc_mem | v0.10.0 |
| `signal.aria` | aria_libc_process | v0.10.0 |
| `arena.aria` | aria_libc_mem | v0.10.0 |
| `pool_alloc.aria` | aria_libc_mem | v0.10.0 |
| `net.aria` | aria_libc_net, aria_libc_string | v0.10.0 |
| `hexstream.aria` | aria_libc_hexstream | v0.10.0 |

### Category B — Pure Aria (17 files)

No extern declarations at all. Work on any platform with an Aria compiler.

| Module | Description |
|--------|-------------|
| `fmt.aria` | String formatting ({0} placeholders) |
| `complex.aria` | Complex number arithmetic |
| `core.aria` | Core language helpers |
| `dbug.aria` | Debug utilities |
| `linalg.aria` | Linear algebra (vectors, matrices) |
| `number.aria` | Number type utilities |
| `print_utils.aria` | Print formatting helpers |
| `quantum.aria` | Quantum computing simulation |
| `string_builder.aria` | Efficient string construction |
| `string_convert.aria` | String type conversions |
| `sys.aria` | System information |
| `test_fixed_primitive.aria` | Fixed-point primitive tests |
| `test_fixed_struct.aria` | Fixed-point struct tests |
| `test_fixed_usage.aria` | Fixed-point usage examples |
| `wavemech.aria` | Wave mechanics |
| `traits/numeric.aria` | Numeric trait definitions |
| `traits/numeric_impls.aria` | Numeric trait implementations |

### Category C — Direct Extern FFI (30 files)

These use direct `extern func:` or non-aria-libc `extern { }` calling into the C runtime. Deprecation notices placed in v0.9.3.

| Module | Extern Count | Complexity | Migration Difficulty |
|--------|-------------|------------|---------------------|
| `type_checker.aria` | 105 | Very High | Hard — compiler internals |
| `json.aria` | 96 | Very High | Medium — parseable in pure Aria |
| `parser.aria` | 70 | Very High | Hard — compiler internals |
| `toml.aria` | 62 | High | Medium — parseable in pure Aria |
| `lexer.aria` | 52 | High | Hard — compiler internals |
| `collections.aria` | 33 | High | Medium — need GC-backed storage |
| `aifs.aria` | 25 | High | Hard — filesystem/kernel |
| `const_evaluator.aria` | 25 | High | Hard — compiler internals |
| `atomic.aria` | 24 | Medium | Hard — needs hardware intrinsics |
| `borrow_checker.aria` | 19 | Medium | Hard — compiler internals |
| `channel.aria` | 16 | Medium | Medium — concurrency |
| `lockfree.aria` | 16 | Medium | Hard — needs atomics |
| `exhaustiveness.aria` | 13 | Medium | Hard — compiler internals |
| `thread.aria` | 9 | Low | Easy — already have thread runtime |
| `safety_checker.aria` | 9 | Low | Hard — compiler internals |
| `actor.aria` | 8 | Low | Medium — concurrency |
| `lib_hashmap_int32_int64.aria` | 8 | Low | Medium — need GC storage |
| `lib_hashmap_int64_int64.aria` | 8 | Low | Medium — need GC storage |
| `lib_hashmap_int8_int8.aria` | 8 | Low | Medium — need GC storage |
| `rwlock.aria` | 7 | Low | Medium — concurrency |
| `thread_pool.aria` | 7 | Low | Medium — concurrency |
| `condvar.aria` | 6 | Low | Medium — concurrency |
| `lib_vec_int16.aria` | 5 | Low | Medium — need GC storage |
| `lib_vec_int32.aria` | 5 | Low | Medium — need GC storage |
| `lib_vec_int32_type.aria` | 5 | Low | Medium — need GC storage |
| `lib_vec_int64.aria` | 5 | Low | Medium — need GC storage |
| `mutex.aria` | 5 | Low | Medium — concurrency |
| `shm.aria` | 5 | Low | Medium — shared memory |
| `wave.aria` | 4 | Low | Deferred — uses compiler intrinsics |
| `barrier.aria` | 3 | Low | Medium — concurrency |

**Totals:** 30 files (down from 38 in v0.9.4)

---

## Package Audit

**Total packages:** 103

### Pure Aria (21 packages, 20.4%)

No extern calls. Fully portable.

| Package | Notes |
|---------|-------|
| aria-args | CLI argument parsing (v0.10.2) |
| aria-ascii | ASCII table/utilities |
| aria-bits | Bit manipulation |
| aria-buf | Byte buffers |
| aria-clamp | Value clamping |
| aria-color | Color manipulation |
| aria-conv | Type conversions |
| aria-endian | Endianness conversion |
| aria-entangled | Entangled state simulation |
| aria-fixed | Fixed-point arithmetic |
| aria-freq | Frequency analysis |
| aria-gradient-field | Gradient field computation |
| aria-hash | Hash functions |
| aria-mux | Multiplexer patterns |
| aria-puremath | Taylor series math (v0.9.3) |
| aria-rand | Random number generation |
| aria-rules-common | Common validation rules |
| aria-str | Extended string operations (v0.10.2) |
| aria-uuid | UUID generation |
| aria-vec | Dynamic arrays |
| aria-zigzag | ZigZag encoding |

### aria-libc Backed (24 packages, 23.3%)

Use aria-libc shims rather than direct extern. Portable across platforms with aria-libc.

| Package | aria-libc libs | Since |
|---------|---------------|-------|
| aria-libc | — | The bridge layer itself (10+ .so files) |
| aria-bench | aria_libc_time | v0.9.2 |
| aria-env | aria_libc_env | v0.9.2 |
| aria-hex | aria_libc_string | v0.9.2 |
| aria-math | aria_libc (various) | v0.9.2 |
| aria-path | aria_libc_string | v0.9.2 |
| aria-regex | aria_libc_string, aria_libc_mem | v0.9.2 |
| aria-sort | aria_libc_mem | v0.9.2 |
| aria-base64 | aria_libc_string, aria_libc_mem | v0.10.2 |
| aria-bigdecimal | aria_libc_mem, aria_libc_string | v0.10.1 |
| aria-csv | aria_libc_string, aria_libc_mem | v0.10.2 |
| aria-datetime | aria_libc_time | v0.10.2 |
| aria-ini | aria_libc_string, aria_libc_mem | v0.10.2 |
| aria-log | aria_libc_io, aria_libc_time | v0.10.2 |
| aria-looping | aria_libc_mem, aria_libc_math | v0.10.1 |
| aria-mock | aria_libc_mem | v0.10.1 |
| aria-rate-limit | aria_libc_mem, aria_libc_string | v0.10.1 |
| aria-semver | aria_libc_string | v0.10.2 |
| aria-static | aria_libc_mem, aria_libc_string | v0.10.1 |
| aria-template | aria_libc_string | v0.10.2 |
| aria-test | aria_libc_mem, aria_libc_string | v0.10.1 |
| aria-url | aria_libc_mem, aria_libc_string | v0.10.1 |
| aria-xml | aria_libc_mem, aria_libc_string | v0.10.1 |
| aria-yaml | aria_libc_mem, aria_libc_string | v0.10.1 |

### Direct Extern FFI (58 packages, 56.3%)

Use direct extern calls to C/C++ libraries or custom shim libraries.

**Unavoidable FFI** (hardware/OS/external library bindings):
aria-audio, aria-body-parser, aria-cli, aria-compress, aria-console, aria-cookie, aria-cors, aria-crypto, aria-cuda, aria-dns, aria-editor, aria-fs, aria-ftp, aria-gtk4, aria-http, aria-input, aria-jamba, aria-jit, aria-mamba, aria-mysql, aria-opengl, aria-postgres, aria-qt6, aria-raylib, aria-redis, aria-router, aria-sdl2, aria-sdl3, aria-server, aria-session, aria-smtp, aria-socket, aria-sqlite, aria-tensor, aria-tetris, aria-transformer, aria-uacp, aria-webkit-gtk, aria-websocket, aria-wxwidgets

**Could migrate to aria-libc** (use simple libc calls):
aria-diff, aria-display, aria-gml, aria-map, aria-matrix, aria-mime, aria-msgpack, aria-pqueue, aria-queue, aria-ringbuf, aria-stats, aria-toml

**Could port to pure Aria** (no C dependency needed):
aria-decision-t, aria-resource-mem

**Other FFI** (complex/mixed):
aria-actor, aria-aifs, aria-args (comment-only extern, functionally pure), aria-channel, aria-json

---

## v0.10.x Progress vs Goals

| Metric | v0.9.4 | v0.10.3 | Target | Status |
|--------|--------|---------|--------|--------|
| stdlib extern files | 38 | 30 | ≤ 29 | ⚠️ 30 (wave.aria deferred — compiler intrinsics) |
| Pure Aria packages | 19 | 21 | ≥ 31 | ⚠️ 21 (most v0.10.1 ports use aria-libc internally) |
| aria-libc backed packages | 8 | 24 | ≥ 18 | ✅ 24 (+16) |
| FFI packages | 76 | 58 | — | ✅ -18 packages migrated |
| Non-FFI packages (combined) | 27 | 45 | — | ✅ +18 migrated total |
| Performance regression | — | < 20% | < 20% | ✅ Met |
| Test coverage (ported) | — | All tested | All tested | ✅ Met |

### Analysis

The pure Aria target of ≥ 31 was not met because packages classified as "could port to pure Aria" in the v0.9.4 audit (e.g. bigdecimal, mock, rate-limit, static, test, url, xml, yaml) were rewritten in Aria but still use aria-libc for byte-level memory and string operations. This is the correct architecture — these packages are not direct FFI but they do depend on the aria-libc shim layer. Two packages — aria-args and aria-str — were successfully ported to fully pure Aria with zero extern dependencies.

The stdlib extern reduction target of ≤ 29 was missed by 1 due to wave.aria being deferred (requires compiler intrinsics `aria_pack_nyte`, `aria_nyte_get_nit` not available through aria-libc).

### What was accomplished in v0.10.x:
- **v0.10.0:** Ported 8 stdlib modules to aria-libc (binary, pipe, process, signal, arena, pool_alloc, net, hexstream). New shim: aria_libc_hexstream.
- **v0.10.1:** Ported 12 packages using aria-libc for byte access (bigdecimal, decision-t, mock, rate-limit, resource-mem, static, test, tetris, url, xml, yaml, looping)
- **v0.10.2:** Migrated 10 packages to aria-libc or pure Aria (args, base64, csv, datetime, ini, json, log, semver, str, template)
- **v0.10.3:** Test suites (≥10 tests for all ported modules/packages), documentation (29 doc pages), this audit, benchmarks

---

## Recommendations for v0.11.x

### Medium Effort Migrations
1. **Port 12 aria-libc packages**: diff, display, gml, map, matrix, mime, msgpack, pqueue, queue, ringbuf, stats, toml
2. **Port 2 remaining pure Aria candidates**: decision-t, resource-mem (remove extern func: usage)
3. **Concurrency stdlib**: mutex, condvar, barrier, rwlock, channel, actor, thread_pool (7 modules — need thread-safe aria-libc wrappers)

### Hard / Leave as FFI
- Compiler internals (type_checker, parser, lexer, etc.) — not worth porting
- GUI bindings (gtk4, qt6, sdl2/3, raylib, opengl) — must wrap C/C++ libs
- Database clients (mysql, postgres, redis, sqlite) — must wrap C client libs
- ML packages with custom shims (cuda, tensor, transformer, mamba, jamba, uacp) — keep custom shims
