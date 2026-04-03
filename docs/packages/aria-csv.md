# aria-csv — CSV Parsing

**Version:** 0.2.2  
**Type:** aria-libc backed (aria_libc_string, aria_libc_mem)  
**Ported:** v0.10.2

## API Reference

```aria
csv_parse(text: string) → int32                     // parse CSV string, returns row count
csv_num_rows() → int32                              // number of rows
csv_num_cols() → int32                              // number of columns
csv_get(row: int32, col_param: int32) → string      // get cell value
csv_write_begin() → NIL                             // begin building CSV output
csv_write_field(value: string) → NIL                // write a field
csv_write_row_end() → NIL                           // end current row
csv_write_result() → string                         // get built CSV string
csv_write_length() → int32                          // length of built CSV
csv_cleanup() → NIL                                 // free parsed data
```

## Example

```aria
use "aria_csv.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:n = raw csv_parse("name,age\nAlice,30\nBob,25");
    string:name = raw csv_get(1i32, 0i32);   // Alice
    string:age = raw csv_get(1i32, 1i32);    // 30
    println(name);
    println(age);
    drop csv_cleanup();
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so`, `libaria_libc_mem.so` at runtime.
