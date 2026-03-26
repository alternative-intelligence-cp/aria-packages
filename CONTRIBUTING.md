# Contributing to Aria Packages

Thank you for your interest in contributing to the Aria ecosystem!

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/<your-username>/aria-packages.git`
3. Create a branch: `git checkout -b feature/your-feature`
4. Make your changes
5. Push and open a Pull Request

## Package Structure

Each package lives in `packages/<package-name>/` and must contain:

```
packages/aria-example/
├── aria-example.aria     # Main source file
├── test_aria-example.aria # Test file (20+ tests)
├── aria_pkg.toml         # Package manifest
└── README.md             # Package documentation
```

## Package Manifest (`aria_pkg.toml`)

```toml
[package]
name = "aria-example"
version = "0.1.0"
description = "Brief description"
license = "Apache-2.0"

[dependencies]
# Other aria packages this depends on

[build]
# Build configuration
```

## Testing

Every package must include tests. Run a package's tests:

```bash
cd packages/aria-example
../../build/ariac test_aria-example.aria -o test && ./test
```

## Commit Messages

Use conventional commit format:
- `feat(aria-example): add new feature`
- `fix(aria-example): fix bug description`
- `test(aria-example): add edge case tests`
- `docs(aria-example): update README`

## Code Style

- Use Aria's `type:name` declaration syntax
- Use `pass()` / `fail()` for function returns
- Include a `failsafe` block in every program
- Follow the patterns established in existing packages

## License

By contributing, you agree that your contributions will be licensed under the Apache License 2.0.
