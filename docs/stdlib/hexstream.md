# hexstream — AriaX Hex Streams (FD 3-5)

**Backend:** aria-libc (aria_libc_hexstream)  
**Ported:** v0.10.0

AriaX extends the standard file descriptor model with three additional hex-encoded streams on FD 3 (data), FD 4 (control), and FD 5 (debug). Hexstream provides read/write access to these channels.

## API Reference

```aria
Hexstream.init() → int32                                        // initialize hex stream subsystem
Hexstream.fd(name: string) → int32                              // get FD by name ("data"=3, "control"=4, "debug"=5)
Hexstream.redirect_to_file(fd: int32, path: string) → int32    // redirect stream output to file
Hexstream.redirect_from_file(fd: int32, path: string) → int32  // redirect stream input from file
Hexstream.send(fd: int32, data: string) → int32                // write string to hex stream
Hexstream.dbg(msg: string) → int32                             // write debug message (FD 5)
Hexstream.write_int64(fd: int32, value: int64) → int32         // write int64 to hex stream
Hexstream.read_line(fd: int32) → string                        // read line from hex stream
Hexstream.read_int64(fd: int32) → int64                        // read int64 from hex stream
Hexstream.is_open(fd: int32) → int32                           // check if stream is open (1=yes)
```

## Example

```aria
use "stdlib/hexstream.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:ok = raw Hexstream.init();
    int32:data_fd = raw Hexstream.fd("data");      // 3
    int32:r = raw Hexstream.redirect_to_file(data_fd, "/tmp/hex.out");
    int32:s = raw Hexstream.send(data_fd, "hello hex");
    int32:dbg = raw Hexstream.dbg("debug info");
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_hexstream.so` at runtime.
