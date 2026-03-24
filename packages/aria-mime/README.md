# aria-mime

MIME type detection from file extensions for Aria

## Installation

```bash
aria-pkg install aria-mime
```

## API

| Function | Description |
|----------|-------------|
| `from_filename(string:filename) -> string` | Detect MIME type from a filename (e.g. `"index.html"` → `"text/html"`). |
| `from_extension(string:ext) -> string` | Detect MIME type from an extension (e.g. `"png"` or `".json"`). |
| `to_extension(string:mime) -> string` | Get the file extension for a MIME type. |
| `is_text(string:mime) -> int32` | Check if a MIME type is text-based (1 = yes, 0 = no). |
| `count() -> int32` | Get the number of known MIME type mappings. |

## Example

```aria
use aria_mime;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Detect from filename
    string:mime = from_filename("style.css");
    drop(println("MIME: &{mime}"));

    // Detect from extension
    string:m2 = from_extension("json");
    drop(println("JSON MIME: &{m2}"));

    // Check if text-based
    int32:text = is_text("text/html");
    if (text == 1i32) {
        drop(println("HTML is text-based"));
    }

    // Reverse lookup
    string:ext = to_extension("image/png");
    drop(println("Extension: &{ext}"));
};
```

## Dependencies

None. The package includes its own MIME database via `libaria_mime_shim.so`.
