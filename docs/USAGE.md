# Using pdf-core

## Dependencies

pdf-core depends on:
- [io-core](https://github.com/ripper-org/io-core) — I/O abstractions
- [zlib-ng](https://github.com/zlib-ng/zlib-ng) — compression support

Both are fetched automatically when building with CMake.

## Building and installing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /your/install/prefix
```

For a clean release install (no test dependencies such as Catch2), configure
with tests disabled:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPDF_RIPPER_CORE_ENABLE_TESTS=OFF
cmake --build build-release -j
cmake --install build-release --prefix /your/install/prefix
```

You can validate that an installed package works from an external CMake
consumer with:

```bash
make install-check   # installs to a temp prefix, builds/runs a consumer
```

## Integrating with CMake

### Installed package

```cmake
find_package(pdf_ripper_core CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE pdf_ripper_core::pdf_ripper_core)
```

io-core and zlib-ng are pulled in automatically as transitive dependencies.

### FetchContent

Prefer a released tag for reproducible builds:

```cmake
include(FetchContent)

FetchContent_Declare(
    pdf_ripper_core
    GIT_REPOSITORY https://github.com/ripper-org/pdf-core
    GIT_TAG        v1.0.0
)

FetchContent_MakeAvailable(pdf_ripper_core)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE pdf_ripper_core::pdf_ripper_core)
```

Use `GIT_TAG main` only for development snapshots.

## CMake options

| Option                                   | Default | Description                                    |
| ---------------------------------------- | ------- | ---------------------------------------------- |
| `PDF_RIPPER_CORE_ENABLE_TESTS`           | auto    | Enable test suite                              |
| `PDF_RIPPER_CORE_TIDY_INCLUDE_TESTS`     | `OFF`   | Include tests in clang-tidy analysis           |
| `PDF_RIPPER_CORE_IO_CORE_GIT_REPOSITORY` | GitHub  | Override io-core git repository                |
| `PDF_RIPPER_CORE_IO_CORE_GIT_TAG`        | `v1.0.0`| Override io-core git tag                       |

## Platforms and compilers

- CMake 3.20+
- C++23
- Linux: GCC 14+ or Clang 18+
- macOS: Apple Clang 16+
- Windows: MSVC 2022 17.12+ (or Clang with a compatible toolchain)

Continuous integration exercises Debug and Release builds on Linux, macOS, and
Windows, sanitizer builds, and an installed-package consumer test.

## API overview

pdf-core exposes a clean, object-oriented API for working with PDF documents:

- **Document** — top-level representation of a PDF file
- **Parser** — reads and parses PDF syntax into in-memory objects
- **Serializer** — writes in-memory objects back to valid PDF output
- **Object system** — indirect objects, streams, references, and value types
- **Cross-reference table** — management of object locations
- **Trailer** — document metadata and cross-reference root
- **Catalog / Pages tree** — document structure navigation

## Ownership, copying, and moving

A `document` owns the objects parsed from it, and those objects retain a
back-reference to their owning document. The library keeps those references
correct across both copy and move:

- **Copying** produces an independent in-memory snapshot. Parsed state is
  deep-copied, the reader is cloned (preserving its cursor position), and every
  resolved object is rebound to the copy. The writer is intentionally not
  copied; attach one with `set_writer()` when the copy needs to be saved.
  Parser, serializer, and save-strategy components are rebuilt from defaults,
  so injected custom components are not carried over.
- **Moving** transfers ownership without copying object content and rebinds
  every internal reference to the destination. Views obtained before a move
  construction (`catalog`, `pages`, `page`, ...) remain valid.

```cpp
auto original = ripper::pdf::core::document::open("input.pdf");

ripper::pdf::core::document copy{original};
copy.set_writer(std::make_unique<ripper::io::core::file_writer>("copy.pdf"));
copy.save();

ripper::pdf::core::document relocated{std::move(original)};
```

## Resource limits

Parsing untrusted PDF data applies bounded defaults to decode size, predictor
rows, object-stream counts, nesting depth, resolution depth, and page-tree
depth. Limits are configured per document before accessing its contents:

```cpp
ripper::pdf::core::document doc{/* reader */, /* writer */};

ripper::pdf::core::resource_limits limits;
limits.max_decoded_stream_bytes = 64U * 1024U * 1024U;
doc.set_resource_limits(limits);
```

Exceeding a limit raises `parse_exception` (or `logic_exception` for
structural traversal limits) instead of allowing unbounded allocation or
recursion. Custom `stream_filter` implementations receive the active limits
through `decode()` and should respect them.
