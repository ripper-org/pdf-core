#!/usr/bin/env bash
#
# verify_install.sh
#
# Installs pdf-core and its dependencies (io-core, zlib-ng) to a temporary
# prefix and validates that an external CMake consumer can find_package() the
# installed pdf-core package, link against it, and run.
#
# Usage:
#   ./scripts/verify_install.sh          # uses Release
#   BUILD_TYPE=Debug ./scripts/verify_install.sh
#
# Exits non-zero on the first failure.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pdf_ripper_core_install_check.XXXXXX")"
trap 'rm -rf "${WORK_DIR}"' EXIT

PREFIX="${WORK_DIR}/prefix"

IO_CORE_TAG="v1.0.0"
ZLIB_NG_TAG="2.3.2"

CMAKE_ARGS=(-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_INSTALL_PREFIX="${PREFIX}")

echo "==> install io-core (${IO_CORE_TAG})"
git clone --depth 1 --branch "${IO_CORE_TAG}" \
    https://github.com/ripper-org/io-core.git "${WORK_DIR}/io-core"
cmake -S "${WORK_DIR}/io-core" -B "${WORK_DIR}/io-core-build" \
    "${CMAKE_ARGS[@]}" -DIO_RIPPER_CORE_ENABLE_TESTS=OFF
cmake --build "${WORK_DIR}/io-core-build" -j
cmake --install "${WORK_DIR}/io-core-build"

echo "==> install zlib-ng (${ZLIB_NG_TAG})"
git clone --depth 1 --branch "${ZLIB_NG_TAG}" \
    https://github.com/zlib-ng/zlib-ng.git "${WORK_DIR}/zlib-ng"
cmake -S "${WORK_DIR}/zlib-ng" -B "${WORK_DIR}/zlib-ng-build" \
    "${CMAKE_ARGS[@]}" \
    -DZLIB_COMPAT=ON -DBUILD_SHARED_LIBS=OFF \
    -DZLIB_ENABLE_TESTS=OFF -DZLIBNG_ENABLE_TESTS=OFF
cmake --build "${WORK_DIR}/zlib-ng-build" -j
cmake --install "${WORK_DIR}/zlib-ng-build"

echo "==> install pdf-core"
cmake -S "${ROOT_DIR}" -B "${WORK_DIR}/pdf-build" \
    "${CMAKE_ARGS[@]}" \
    -DPDF_RIPPER_CORE_ENABLE_TESTS=OFF \
    -DFETCHCONTENT_BASE_DIR="${WORK_DIR}/deps"
cmake --build "${WORK_DIR}/pdf-build" -j
cmake --install "${WORK_DIR}/pdf-build"

echo "==> configure and build external consumer"
CONSUMER_DIR="${WORK_DIR}/consumer"
mkdir -p "${CONSUMER_DIR}"
cat > "${CONSUMER_DIR}/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.20)
project(pdf_ripper_core_consumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(pdf_ripper_core CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE pdf_ripper_core::pdf_ripper_core)
CMAKE
cat > "${CONSUMER_DIR}/main.cpp" <<'CPP'
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/parser/parser.hpp"

#include <filesystem>

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;

    auto doc = ripper::pdf::core::document::open(std::filesystem::path{argv[1]});
    ripper::pdf::core::parser parser{doc};
    const auto header = parser.parse_header();
    if (const auto revisions = parser.revision_history(); revisions == nullptr)
        return 3;
    if (const auto version = header.version(); version.empty())
        return 4;

    return 0;
}
CPP

cmake -S "${CONSUMER_DIR}" -B "${CONSUMER_DIR}/build" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_PREFIX_PATH="${PREFIX}"
cmake --build "${CONSUMER_DIR}/build" -j

echo "==> run consumer"
cp "${ROOT_DIR}/tests/fixtures/pdfs/test.pdf" "${WORK_DIR}/consumer.pdf"
"${CONSUMER_DIR}/build/consumer" "${WORK_DIR}/consumer.pdf"

echo "==> install verification passed"