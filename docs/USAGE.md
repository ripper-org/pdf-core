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