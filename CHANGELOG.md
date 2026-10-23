# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `document` copy construction and copy assignment. Copies deep-clone parsed
  state, clone the reader, and rebuild parser/serializer components; the
  writer is not copied.
- `document` move construction and move assignment. Moves transfer state
  without copying object content and rebind every internal document reference.
- `document::set_writer()` for attaching a writer backend to an existing
  document, primarily to make copied documents saveable.

### Changed

- Added checked boundary value types (`byte_count`, `byte_offset`,
  `xref_field_width`) and validated parser result objects for size, offset,
  and range arithmetic.

## [1.0.0] - 2026-09-25

First stable release. The public API contract is finalized; within the 1.x
line no breaking changes are introduced without a major version bump.
