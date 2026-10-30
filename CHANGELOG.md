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

## [1.0.0] - 2026-09-25

First stable release. The public API contract is finalized; within the 1.x
line no breaking changes are introduced without a major version bump.
