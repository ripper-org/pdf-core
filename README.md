# Ripper PDF Core

**Low-level PDF parsing, manipulation, and creation library.**

pdf-core is a C++23 library that gives developers precise control over PDF
documents. It parses PDF syntax into clean in-memory objects, allows
programmatic modification, and serializes back to valid PDF output — without
second-guessing your intent.

Built on [io-core](https://github.com/ripper-org/io-core) for portable I/O and
[zlib-ng](https://github.com/zlib-ng/zlib-ng) for stream compression.

## Use cases

- Extract metadata, pages, or structure from existing PDF documents
- Build PDF analyzers, validators, and repair tools
- Programmatically generate or modify PDF files at the object level
- Serve as the foundation for higher-level PDF libraries and language bindings
  (C, PHP, and more)

## Documentation

- [User guide](docs/USAGE.md) — building, installing, and CMake integration
- [Contributor guide](docs/CONTRIBUTING.md) — building from source, running
  tests, formatting, and static analysis

## License

This library is distributed under the **MIT License**. You are free to use,
modify, and distribute it in any project, proprietary or open-source. See
[LICENSE](LICENSE) for the full text.

## Contact

If this library fits your use case, or if you have ideas for features or
improvements, we would love to hear from you. Open an issue or start a
discussion on the [GitHub repository](https://github.com/ripper-org/pdf-core).
