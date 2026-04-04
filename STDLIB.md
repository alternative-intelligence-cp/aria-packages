# Aria Standard Library — Module Registry

**Version**: v0.12.3
**Total**: 57 modules

---

## Core

| Module | Description |
|--------|-------------|
| core.aria | Core language runtime support |
| mem.aria | Memory management primitives |
| io.aria | Input/output operations |
| sys.aria | System calls and process control |
| fmt.aria | String formatting |
| string.aria | String operations |
| string_builder.aria | Efficient string construction |
| string_convert.aria | String↔numeric conversion |
| print_utils.aria | Print formatting utilities |
| number.aria | Numeric utilities |
| math.aria | Mathematical functions |
| complex.aria | Complex number arithmetic |
| linalg.aria | Linear algebra operations |

## Collections & Data

| Module | Description |
|--------|-------------|
| collections.aria | Collection utilities |
| lib_hashmap_int32_int64.aria | HashMap<int32, int64> |
| lib_hashmap_int64_int64.aria | HashMap<int64, int64> |
| lib_hashmap_int8_int8.aria | HashMap<int8, int8> |
| lib_vec_int16.aria | Vec<int16> |
| lib_vec_int32.aria | Vec<int32> |
| lib_vec_int32_type.aria | Vec<int32> type wrapper |
| lib_vec_int64.aria | Vec<int64> |
| json.aria | JSON parsing |
| toml.aria | TOML parsing |

## Memory & Allocation

| Module | Description |
|--------|-------------|
| arena.aria | Arena allocator |
| pool_alloc.aria | Pool allocator |
| borrow_checker.aria | Borrow checking support |

## Concurrency & Threading

| Module | Description |
|--------|-------------|
| thread.aria | Thread creation and management |
| thread_pool.aria | Thread pool executor |
| mutex.aria | Mutual exclusion locks |
| condvar.aria | Condition variables |
| rwlock.aria | Read-write locks |
| channel.aria | Message-passing channels |
| actor.aria | Actor model concurrency |
| barrier.aria | Thread barriers |
| lockfree.aria | Lock-free data structures |
| atomic.aria | Atomic operations |
| shm.aria | Shared memory |

## I/O & System

| Module | Description |
|--------|-------------|
| pipe.aria | Inter-process pipes |
| binary.aria | Binary data operations |
| hexstream.aria | Hex stream I/O (FD 3-5) |
| signal.aria | Signal handling |
| process.aria | Process management |
| net.aria | Network operations |
| wave.aria | Audio wave generation |
| wavemech.aria | Wave mechanics |

## Compiler Infrastructure

| Module | Description |
|--------|-------------|
| lexer.aria | Tokenization |
| parser.aria | Parsing |
| type_checker.aria | Type checking |
| const_evaluator.aria | Compile-time constant evaluation |
| safety_checker.aria | Safety analysis |
| exhaustiveness.aria | Pattern match exhaustiveness checking |
| dbug.aria | Debug support |

## Experimental / Test

| Module | Description |
|--------|-------------|
| quantum.aria | Quantum computing primitives |
| aifs.aria | AI filesystem (FUSE-based) |
| test_fixed_primitive.aria | Fixed-point primitive tests |
| test_fixed_struct.aria | Fixed-point struct tests |
| test_fixed_usage.aria | Fixed-point usage tests |
