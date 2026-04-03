# stdlib/io — File Streaming I/O

**Version:** v0.9.1  
**Backend:** aria-libc (`libaria_libc_io.so`)  
**Import:** `use "stdlib/io.aria".*;`

## Overview

Provides streaming file I/O through the `FileStream` type. For whole-file operations, use the compiler builtins (`readFile`, `writeFile`, `fileExists`, `fileSize`, `deleteFile`).

## API Reference

### FileStream Type

```aria
FileStream.open(path: string, mode: string) → FileStream
    // mode: "r" (read), "w" (write), "a" (append)
    //       "rb", "wb", "ab" (binary variants)

FileStream.close(self: FileStream) → NIL
FileStream.write(self: FileStream, content: string) → int64  // bytes written
FileStream.readLine(self: FileStream) → string   // one line (incl. newline)
FileStream.eof(self: FileStream) → int8          // 1 at EOF, 0 otherwise
FileStream.flush(self: FileStream) → int32       // 0 success, -1 error
FileStream.seek(self: FileStream, offset: int64, whence: int32) → int32
FileStream.tell(self: FileStream) → int64        // current byte position
```

### Seek Constants

```aria
FileStream.SEEK_SET() → int32  // 0 — from start
FileStream.SEEK_CUR() → int32  // 1 — from current
FileStream.SEEK_END() → int32  // 2 — from end
```

## Example: Write and Read a File

```aria
use "stdlib/io.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    // Write
    FileStream:out = raw FileStream.open("output.txt", "w");
    drop out.write("Line 1\n");
    drop out.write("Line 2\n");
    drop out.close();

    // Read back
    FileStream:in = raw FileStream.open("output.txt", "r");
    while (raw in.eof() == 0i8) {
        string:line = raw in.readLine();
        if (string_is_empty(line) == false) {
            print(line);
        }
    }
    drop in.close();

    exit 0;
};
```

## Example: Seek and Tell

```aria
use "stdlib/io.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    FileStream:f = raw FileStream.open("data.bin", "rb");
    // Jump to byte 100
    _ = raw f.seek(100i64, raw FileStream.SEEK_SET());
    int64:pos = raw f.tell();
    println(string_from_int(pos));  // 100
    drop f.close();
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_io.so` at runtime. Link with `-laria_libc_io`.
