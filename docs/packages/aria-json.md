# aria-json — JSON Tokenizer

**Version:** 0.2.0  
**Type:** aria-libc backed (aria_libc_string, aria_libc_mem)  
**Ported:** v0.10.2

## API Reference

```aria
json_is_ws(c: int64) → bool                                          // check if character is whitespace
json_is_digit(c: int64) → bool                                       // check if character is digit
next_tok(state: int64->, input: string, ttypes: int64->) → int64     // extract next token
tokenize(input: string, state: int64->, ttypes: int64->) → int64     // tokenize full input
seq_matches(ttypes: int64->, count: int64, expected: int64->) → bool // check if token types match expected sequence
```

## Example

```aria
use "aria_json.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    // Tokenize a JSON string
    string:json = "{\"name\":\"Aria\"}";
    // ... use tokenize() with state and ttypes arrays
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so`, `libaria_libc_mem.so` at runtime.
