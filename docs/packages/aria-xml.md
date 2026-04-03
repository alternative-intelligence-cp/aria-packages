# aria-xml — XML Parsing

**Version:** 0.2.6  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
parse(xml: string) → int32                       // parse XML string, returns node count
get_text(tag: string) → string                   // get text content of first matching tag
get_attr(tag: string, attr_name: string) → string // get attribute value from first matching tag
count_tag(tag: string) → int32                    // count occurrences of a tag
has_tag(tag: string) → int32                      // 1 if tag exists
node_count() → int32                              // total number of parsed nodes
xml_clear() → NIL                                 // clear parsed data
get_path(path_str: string) → string               // XPath-like lookup (e.g. "root.child")
```

## Example

```aria
use "aria_xml.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    int32:n = raw parse("<root><name>Aria</name><version>0.10</version></root>");
    println(raw get_text("name"));     // Aria
    println(raw get_text("version"));  // 0.10
    int32:has = raw has_tag("name");   // 1
    drop xml_clear();
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_string.so` at runtime.
