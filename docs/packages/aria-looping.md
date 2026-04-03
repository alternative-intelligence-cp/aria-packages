# aria-looping — Looping Transformer (ML)

**Version:** 0.2.10  
**Type:** pure Aria  
**Ported:** v0.10.1

A looping transformer architecture that iterates over the same transformer block multiple times, halting early when output stabilizes. Useful for adaptive-depth inference.

## API Reference

```aria
// Model lifecycle
looping_create(d_model: int32, n_heads: int32, ffn_dim: int32, max_iters: int32, vocab_size: int32, max_seq: int32) → int64
looping_destroy(id: int64) → NIL
looping_set_seed(s: int32) → NIL

// Model inspection
looping_d_model(id: int64) → int32
looping_n_heads(id: int64) → int32
looping_ffn_dim(id: int64) → int32
looping_max_iters(id: int64) → int32
looping_vocab_size(id: int64) → int32
looping_last_iters_used(id: int64) → int32
looping_param_count(id: int64) → int64
looping_set_threshold(id: int64, thresh: flt64) → NIL

// Scratch buffers (input/output tensors)
looping_scratch_create(rows: int32, cols: int32) → int64
looping_scratch_destroy(id: int64) → NIL
looping_scratch_set(id: int64, idx: int32, val: flt64) → NIL
looping_scratch_get(id: int64, idx: int32) → flt64
looping_scratch_rows(id: int64) → int32
looping_scratch_cols(id: int64) → int32

// Inference
looping_forward(mid: int64, inp_id: int64) → int64         // run forward pass (adaptive iters)
looping_forward_n(mid: int64, inp_id: int64, n_iters: int32) → int64  // run with fixed iteration count
looping_argmax(sid: int64, row: int32) → int32              // argmax over a scratch row
looping_softmax_row(sid: int64, row: int32) → NIL           // in-place softmax over a scratch row
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_math.so`, `libaria_libc_string.so` at runtime.
