# binary — Binary Data Serialization

**Backend:** aria-libc (aria_libc_mem, aria_libc_math, aria_libc_io)  
**Ported:** v0.10.0

## API Reference

```aria
bin_new() → int64                              // create new binary buffer (returns handle)
bin_from_file(path: string) → int64            // load binary buffer from file
bin_to_file(hdr: int64, path: string) → int32  // write binary buffer to file
bin_size(hdr: int64) → int64                   // current size in bytes
bin_pos(hdr: int64) → int64                    // current read/write position
bin_seek(hdr: int64, pos: int64) → NIL         // seek to position
bin_remaining(hdr: int64) → int64              // bytes remaining after position
bin_free(hdr: int64) → NIL                     // free the binary buffer

// Write functions (append at current position)
bin_write_int8(hdr: int64, val: int32) → NIL
bin_write_int16(hdr: int64, val: int32) → NIL
bin_write_int32(hdr: int64, val: int32) → NIL
bin_write_int64(hdr: int64, val: int64) → NIL
bin_write_flt32(hdr: int64, val: flt64) → NIL
bin_write_flt64(hdr: int64, val: flt64) → NIL
bin_write_str(hdr: int64, val: string) → NIL
bin_write_bool(hdr: int64, val: int32) → NIL

// Read functions (read from current position, advances position)
bin_read_int8(hdr: int64) → int32
bin_read_int16(hdr: int64) → int32
bin_read_int32(hdr: int64) → int32
bin_read_int64(hdr: int64) → int64
bin_read_flt32(hdr: int64) → flt64
bin_read_flt64(hdr: int64) → flt64
bin_read_str(hdr: int64) → string
bin_read_bool(hdr: int64) → int32
```

## Example

```aria
use "stdlib/binary.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:buf = raw bin_new();
    drop bin_write_int32(buf, 42i32);
    drop bin_write_str(buf, "hello");
    drop bin_seek(buf, 0i64);
    int32:n = raw bin_read_int32(buf);
    string:s = raw bin_read_str(buf);
    println(string_from_int(@cast<int64>(n)));  // 42
    println(s);  // hello
    drop bin_free(buf);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_math.so`, `libaria_libc_io.so` at runtime.
