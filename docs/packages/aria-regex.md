# aria-regex — Regular Expressions

**Version:** 0.2.0  
**Type:** pure Aria (finite automaton, uses aria-libc_string + aria-libc_mem)  
**Ported:** v0.9.2

## API Reference

```aria
regex_match(pattern, text: string) → int32              // 1 if full match
regex_match_all(pattern, text: string) → int64           // handle to all matches
regex_match_count(handle: int64) → int64                 // number of matches
regex_match_get(handle: int64, idx: int64) → string      // get match by index
regex_match_free(handle: int64) → NIL                    // free match handle
regex_replace(pattern, text, replacement: string) → string  // replace matches
regex_split(pattern, text: string) → int64               // split handle
regex_split_count(handle: int64) → int64
regex_split_get(handle: int64, idx: int64) → string
regex_split_free(handle: int64) → NIL
regex_count(pattern, text: string) → int64               // count matches
regex_is_valid(pattern: string) → int32                  // 1 if valid pattern
regex_escape(text: string) → string                      // escape metacharacters
```

### Supported Pattern Syntax

| Pattern | Meaning |
|---------|---------|
| `.` | Any character |
| `*` | Zero or more of preceding |
| `+` | One or more of preceding |
| `?` | Zero or one of preceding |
| `[abc]` | Character class |
| `[^abc]` | Negated character class |
| `[a-z]` | Character range |
| `\d` | Digit `[0-9]` |
| `\w` | Word char `[a-zA-Z0-9_]` |
| `\s` | Whitespace |
| `^` | Start of string |
| `$` | End of string |
| `\` | Escape next character |

## Example

```aria
use "aria_regex.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    // Simple match
    int32:m = raw regex_match("[0-9]+", "42");
    // m == 1

    // Count digits in text
    int64:n = raw regex_count("[0-9]+", "abc 123 def 456 ghi");
    // n == 2

    // Replace
    string:result = raw regex_replace("[0-9]+", "abc 123 def", "NUM");
    // result == "abc NUM def"

    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so` and `libaria_libc_mem.so` at runtime.
