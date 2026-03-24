# aria-semver

Semantic versioning (SemVer) parsing and comparison for Aria

## Installation

```bash
aria-pkg install aria-semver
```

## API

| Function | Description |
|----------|-------------|
| `parse(string:version) -> int32` | Parse a SemVer string (e.g. `"1.2.3-beta+build"`). Returns 1 on success. |
| `get_major() -> int32` | Get the major version. Call after `parse()`. |
| `get_minor() -> int32` | Get the minor version. Call after `parse()`. |
| `get_patch() -> int32` | Get the patch version. Call after `parse()`. |
| `get_prerelease() -> string` | Get the prerelease tag (e.g. `"beta"`). |
| `get_build() -> string` | Get the build metadata. |
| `to_string() -> string` | Get the normalized version string. |
| `compare(string:a, string:b) -> int32` | Compare two versions. Returns -1, 0, or 1. |
| `satisfies(string:version, string:constraint) -> int32` | Check if a version satisfies a constraint (e.g. `">=1.0.0"`). Returns 1 if satisfied. |
| `bump_major(string:version) -> string` | Bump the major version (e.g. `"1.2.3"` → `"2.0.0"`). |
| `bump_minor(string:version) -> string` | Bump the minor version (e.g. `"1.2.3"` → `"1.3.0"`). |
| `bump_patch(string:version) -> string` | Bump the patch version (e.g. `"1.2.3"` → `"1.2.4"`). |

## Example

```aria
use aria_semver;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Parse a version
    drop(parse("1.2.3"));
    int32:major = get_major();
    int32:minor = get_minor();
    int32:patch = get_patch();
    drop(println("Version: &{major}.&{minor}.&{patch}"));

    // Compare versions
    int32:cmp = compare("1.0.0", "2.0.0");
    if (cmp == -1i32) {
        drop(println("1.0.0 is older than 2.0.0"));
    }

    // Check constraints
    int32:ok = satisfies("1.2.3", ">=1.0.0");

    // Bump versions
    string:next = bump_minor("1.2.3");
    drop(println("Next minor: &{next}"));
};
```

## Dependencies

None. The package includes its own SemVer parser via `libaria_semver_shim.so`.
