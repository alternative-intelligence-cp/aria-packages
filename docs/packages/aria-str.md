# aria-str — Extended String Operations

**Version:** 0.2.0  
**Type:** aria-libc backed (aria_libc_string)  
**Ported:** v0.10.2

## API Reference

```aria
str_char_at(s: string, i: int64) → string               // character at index as string
str_slice(s: string, start: int64) → string              // substring from start to end
str_before(s: string, delim: string) → string            // text before first occurrence of delimiter
str_after(s: string, delim: string) → string             // text after first occurrence of delimiter
str_before_last(s: string, delim: string) → string       // text before last occurrence of delimiter
str_after_last(s: string, delim: string) → string        // text after last occurrence of delimiter
str_between(s: string, open_: string, close_: string) → string  // text between open and close delimiters
str_replace(s: string, old_: string, new_: string) → string     // replace first occurrence
str_replace_all(s: string, old_: string, new_: string) → string // replace all occurrences
```

## Example

```aria
use "aria_str.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    string:path = "user/documents/file.txt";
    println(raw str_before(path, "/"));       // user
    println(raw str_after(path, "/"));        // documents/file.txt
    println(raw str_after_last(path, "/"));   // file.txt
    println(raw str_before_last(path, "/"));  // user/documents
    println(raw str_between("<b>hello</b>", "<b>", "</b>"));  // hello
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so` at runtime.
