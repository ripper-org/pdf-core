# Contributing to pdf-core

## Prerequisites

- CMake 3.20+
- A C++23-compatible compiler (GCC 15+, Clang 19+, Apple Clang 17+, MSVC 2022 17.13+)
- `clang-format` (optional, for formatting support)
- `clang-tidy` (optional, for static analysis)

## Quick start

```bash
make build
```

This configures the project in `build/` (Debug mode) and compiles the library
and tests. Dependencies (io-core, zlib-ng, Catch2) are fetched automatically.

## Available targets

| Command                 | Description                              |
| ----------------------- | ---------------------------------------- |
| `make configure`        | Run CMake configure only                 |
| `make build`            | Configure and build library + tests      |
| `make test`             | Build and run the CTest suite            |
| `make format`           | Apply `clang-format` to all sources      |
| `make format-check`     | Verify `clang-format` compliance         |
| `make tidy`             | Run `clang-tidy` static analysis         |
| `make install`          | Install the library from `build/`        |
| `make install-check`    | Install to a temp prefix and verify an external CMake consumer |
| `make clean`            | Remove `build/` and `.deps/`             |
| `make rebuild`          | Clean then build                         |
| `make depclean`         | Remove `.deps/` only                     |

Configuration variables: `BUILD_DIR`, `BUILD_TYPE`, `GENERATOR`, `DEPS_DIR`.

## CMake options

| Option                              | Default | Description                              |
| ----------------------------------- | ------- | ---------------------------------------- |
| `PDF_RIPPER_CORE_ENABLE_TESTS`      | auto    | `ON` when this is the top-level project  |
| `PDF_RIPPER_CORE_TIDY_INCLUDE_TESTS`| `OFF`   | Include test sources in clang-tidy       |
| `PDF_RIPPER_CORE_CLANG_FORMAT_BIN`  | auto    | Path to `clang-format` (auto-detected)   |
| `PDF_RIPPER_CORE_CLANG_TIDY_BIN`    | auto    | Path to `clang-tidy` (auto-detected)     |

clang-tidy only analyzes library sources by default; test sources are
excluded unless `-DPDF_RIPPER_CORE_TIDY_INCLUDE_TESTS=ON` and tests are enabled.

io-core and zlib-ng are fetched automatically via `FetchContent`. You can
override their repository or tag with:
- `PDF_RIPPER_CORE_IO_CORE_GIT_REPOSITORY`
- `PDF_RIPPER_CORE_IO_CORE_GIT_TAG`

## Continuous integration

`.github/workflows/ci.yml` runs these jobs:

- `format-check` (Linux) - verifies `clang-format` compliance.
- `build-and-test` (Linux + macOS + Windows, Debug + Release) - compiles the
  library and runs the CTest suite.
- `sanitizers` (Linux) - builds with AddressSanitizer + UndefinedBehaviorSanitizer
  and runs the test suite.
- `install-consumer` (Linux) - installs the package plus its dependencies to a
  temp prefix and verifies an external CMake consumer can build and run against
  it.
- `clang-tidy` (Linux + Windows) - runs static analysis with tests disabled
  (`-DPDF_RIPPER_CORE_ENABLE_TESTS=OFF`).

`clang-format` is installed via `pip install clang-format==22.1.0` (pinned to
match the LLVM toolchain the project is formatted against — apt's v18 on Ubuntu
24.04 disagrees with v22 formatting). `clang-tidy` is pinned to LLVM 22.1.0 on
Linux via the apt.llvm.org installer for the same reason; apt's v18
clang-tidy can hang on template-heavy translation units under `clang-analyzer`
checks. Windows runners expose LLVM's tools at `C:\Program Files\LLVM\bin`.
Windows uses the Ninja generator in the `clang-tidy` job because it produces
`compile_commands.json`, which the tidy target requires. On Windows, the
`build-and-test` job configures with `-DBUILD_SHARED_LIBS=OFF` to force zlib-ng
to build static; its default shared build would otherwise require its DLL at
test runtime (Catch2 test discovery exits with `0xc0000135`).

## Test suite

Tests are written with [Catch2](https://github.com/catchorg/Catch2) v3 and are
automatically fetched by CMake.

```bash
make test
```

## Code style

This project uses `clang-format` with the configuration in `.clang-format`.
Always run `make format` before committing.

## Checked boundary values

Values read from a PDF are untrusted until they have been validated. When a
value participates in size, offset, or index arithmetic, convert it at the
parsing boundary into one of the checked value types instead of passing a raw
integer around:

- `byte_count` — a validated quantity of bytes; supports checked `plus()`,
  `minus()`, and `multiplied_by()`.
- `byte_offset` — a validated position; use `advanced_by()` and
  `distance_to()` rather than integer arithmetic.
- `xref_field_width` — a validated 0–8 field width for xref streams.
- `checked_numeric::narrow<T>()` — the single checked integer conversion helper.
- `checked_subspan()` — validates an offset/count pair against a specific
  buffer and returns the resulting span.

Only call `.value()` at the boundary where an external API still requires a
plain integer, and treat that call as a review point: the surrounding
arithmetic is no longer protected by the type.

Parsed results that carry several related values should be validated classes
with a parsing factory and read-only accessors, as `object_stream_layout` does.
Do not expose validated results as mutable aggregates.

The same factories and operations are `constexpr`, so values known at compile
time can be checked statically:

```cpp
constexpr auto row_bytes = byte_count::from_integer(1024);
constexpr auto image_bytes = row_bytes.multiplied_by(64);
static_assert(image_bytes.value() == 65536);
```

Runtime input follows the same path and throws `parse_exception` when a
constraint fails.
