# aria-cli

Rich CLI output with ANSI colors, styles, and progress bars for Aria

## Installation

```bash
aria-pkg install aria-cli
```

## API

| Function | Description |
|----------|-------------|
| `red(string:text) -> string` | Wrap text in red ANSI color. |
| `green(string:text) -> string` | Wrap text in green ANSI color. |
| `yellow(string:text) -> string` | Wrap text in yellow ANSI color. |
| `blue(string:text) -> string` | Wrap text in blue ANSI color. |
| `magenta(string:text) -> string` | Wrap text in magenta ANSI color. |
| `cyan(string:text) -> string` | Wrap text in cyan ANSI color. |
| `white(string:text) -> string` | Wrap text in white ANSI color. |
| `gray(string:text) -> string` | Wrap text in gray ANSI color. |
| `bright_red(string:text) -> string` | Wrap text in bright red ANSI color. |
| `bright_green(string:text) -> string` | Wrap text in bright green ANSI color. |
| `bright_blue(string:text) -> string` | Wrap text in bright blue ANSI color. |
| `bold(string:text) -> string` | Apply bold style. |
| `dim(string:text) -> string` | Apply dim style. |
| `italic(string:text) -> string` | Apply italic style. |
| `underline(string:text) -> string` | Apply underline style. |
| `strikethrough(string:text) -> string` | Apply strikethrough style. |
| `progress(int32:current, int32:total, int32:width) -> string` | Generate a progress bar string. |
| `hr(int32:width, string:ch) -> string` | Generate a horizontal rule with a given character. |
| `banner(string:text, int32:width) -> string` | Generate a centered banner. |
| `strip(string:text) -> string` | Strip ANSI escape codes from text. |

## Example

```aria
use aria_cli;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Colored output
    drop(println(green("Build successful!")));
    drop(println(red("Error: file not found")));
    drop(println(bold("Important notice")));

    // Progress bar
    string:bar = progress(75i32, 100i32, 30i32);
    drop(println(bar));

    // Horizontal rule
    string:line = hr(40i32, "=");
    drop(println(line));

    // Banner
    string:b = banner("Aria CLI", 40i32);
    drop(println(b));
};
```

## Dependencies

None. The package includes its own ANSI formatting via `libaria_cli_shim.so`.
