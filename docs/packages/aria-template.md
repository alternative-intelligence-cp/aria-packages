# aria-template — String Templating

**Version:** 0.2.6  
**Type:** aria-libc backed (aria_libc_string)  
**Ported:** v0.10.2

## API Reference

```aria
template_set(key: string, value: string) → int32    // set template variable, returns slot index
template_get(key: string) → string                   // get template variable value
template_render(tmpl: string) → string               // render template, replacing {{key}} with values
```

## Example

```aria
use "aria_template.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:s1 = raw template_set("name", "Aria");
    int32:s2 = raw template_set("version", "0.10");
    string:result = raw template_render("Welcome to {{name}} v{{version}}!");
    println(result);  // Welcome to Aria v0.10!
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_string.so` at runtime.
