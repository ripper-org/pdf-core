# Contributing to pdf-core

## Prerequisites

- CMake 3.20+
- A C++23-compatible compiler (GCC 14+, Clang 18+, Apple Clang 16+, MSVC 2022 17.12+)
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

`.github/workflows/ci.yml` runs three jobs:

- `format-check` (Linux) - verifies `clang-format` compliance.
- `build-and-test` (Linux + Windows) - compiles the library and runs the
  CTest suite.
- `clang-tidy` (Linux + Windows) - runs static analysis with tests disabled
  (`-DPDF_RIPPER_CORE_ENABLE_TESTS=OFF`).

`clang-format` is installed via `pip install clang-format==22.1.0` (pinned to
match the LLVM toolchain the project is formatted against — apt's v18 on Ubuntu
24.04 disagrees with v22 formatting). `clang-tidy` is installed via `apt` on
Linux; Windows runners expose LLVM's tools at `C:\Program Files\LLVM\bin`.
Windows uses the Ninja generator in the `clang-tidy` job because it produces
`compile_commands.json`, which the tidy target requires.

## Test suite

Tests are written with [Catch2](https://github.com/catchorg/Catch2) v3 and are
automatically fetched by CMake.

```bash
make test
```

## Code style

This project uses `clang-format` with the configuration in `.clang-format`.
Always run `make format` before committing.
