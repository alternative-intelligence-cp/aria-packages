# aria-xml

XML parsing and querying for Aria

## Installation

```bash
aria-pkg install aria-xml
```

## API

| Function | Description |
|----------|-------------|
| `parse(string:xml) -> int32` | Parse an XML string. Returns node count or negative on error. |
| `get_text(string:tag) -> string` | Get the text content of the first element with the given tag name. |
| `get_attr(string:tag, string:attr_name) -> string` | Get an attribute value from the first element with the given tag name. |
| `count_tag(string:tag) -> int32` | Count the number of elements with the given tag name. |
| `has_tag(string:tag) -> int32` | Check if a tag exists (1 = yes, 0 = no). |
| `node_count() -> int32` | Get the total number of parsed nodes. |
| `get_path(string:path) -> string` | Get text content by path (e.g. `"root.child"`). |
| `clear() -> void` | Clear all parsed data. |

## Example

```aria
use aria_xml;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Parse XML
    int32:n = parse("<root><name>Aria</name><version>0.2.6</version><item id=\"42\">hello</item></root>");

    // Query by tag
    string:name = get_text("name");
    drop(println("Name: &{name}"));

    // Get attributes
    string:id = get_attr("item", "id");
    drop(println("Item ID: &{id}"));

    // Check tags
    int32:count = count_tag("item");
    int32:exists = has_tag("name");

    clear();
};
```

## Dependencies

None. The package includes its own XML parser via `libaria_xml_shim.so`.
