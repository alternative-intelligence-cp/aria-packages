# pipe — Inter-Process Pipes

**Backend:** aria-libc (aria_libc_process)  
**Ported:** v0.10.0

## API Reference

```aria
Pipe.create() → int64                              // create pipe, returns read FD
Pipe.write_fd() → int64                            // get write FD of last created pipe
Pipe.destroy(read_fd: int64) → int32               // close both ends
Pipe.write_int64(fd: int64, value: int64) → int32  // write int64 to pipe
Pipe.read_int64(fd: int64) → int64                 // read int64 from pipe
Pipe.close_write(fd: int64) → int32                // close write end
Pipe.close_read(fd: int64) → int32                 // close read end
```

## Example

```aria
use "stdlib/pipe.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int64:rfd = raw Pipe.create();
    int64:wfd = raw Pipe.write_fd();
    int32:ok = raw Pipe.write_int64(wfd, 42i64);
    int64:val = raw Pipe.read_int64(rfd);
    println(string_from_int(val));  // 42
    int32:d = raw Pipe.destroy(rfd);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_process.so` at runtime.
