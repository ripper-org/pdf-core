# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `document` move construction and move assignment. Moves transfer state
  without copying object content and rebind every internal document reference.
- `document::set_writer()` for attaching a writer backend to an existing
  document, for example to make a read-only document saveable.

### Changed

- `document` is movable-only: copy construction and copy assignment are
  deleted. A document owns exclusive resources, so there is no unambiguous
  copy policy (clone the reader, but what to do with the stateful writer).

- Added checked boundary value types (`byte_count`, `byte_offset`,
  `xref_field_width`) and validated parser result objects for size, offset,
  and range arithmetic.

- Minimum toolchain baseline is now GCC 15+, Clang 19+, Apple Clang 17+
  (Xcode 16.3+), or MSVC 2022 17.13+. The library uses C++23 features such as
  deducing this and P1206R7 container ranges (`from_range`, `append_range`)
  directly. CI pins Ubuntu 24.04 and installs GCC 15 explicitly.

### Fixed

- GCC rejects `document::resource_limits()` because the accessor name shadows
  the `resource_limits` type (`-Wchanges-meaning`). The declarations now use
  elaborated type specifiers.

## [1.0.0] - 2026-09-25

First stable release. The public API contract is finalized; within the 1.x
line no breaking changes are introduced without a major version bump.
