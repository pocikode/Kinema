#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────
#  package.sh
#  Build Kinema from source and create a platform installer.
#
#  Usage:
#    macOS:  ./package.sh
#    Windows: ./package.bat
# ──────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
PACKAGE_DIR="${SCRIPT_DIR}/package"
NPROC=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

echo "=== Configuring Kinema (Release) ==="
cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" -DCMAKE_BUILD_TYPE=Release

echo ""
echo "=== Building Kinema (${NPROC} parallel jobs) ==="
cmake --build "${BUILD_DIR}" --parallel "${NPROC}"

echo ""
echo "=== Creating Installer Package ==="
mkdir -p "${PACKAGE_DIR}"
cpack --config "${BUILD_DIR}/CPackConfig.cmake" -B "${PACKAGE_DIR}"

echo ""
echo "=== Package created! ==="
ls -lh "${PACKAGE_DIR}/"
